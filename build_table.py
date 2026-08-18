#!/usr/bin/env python3
"""
build_table.py — Case Study §5.2 Table generator (100 taskset average).

Reads sweep outputs from ./sweep_results/u_<UTIL>/ and produces:
  - table_u_<UTIL>.csv  : one row per scheme, columns = paper table §5.2
  - table_all.csv       : long-format table with all (U, Mode) pairs

Column definitions (paper §5.2):
  Sched_LT      = t_first_infeasible / baseline_lifetime  (per iter, then mean)
                  Falls back to scheme_lifetime/baseline_lifetime when
                  t_first_infeasible = -1 (test never rejected during run).
  Phys_LT       = scheme_lifetime / baseline_lifetime  (per iter, then mean).
  Delta_unsafe  = t_first_infeasible - t_first_dlmiss  (per iter, then mean).
                  Only iters where BOTH timestamps fired contribute.
  FN_rate       = mean of jobs_summary.FN_rate  (unsafe: predicted feasible
                  but job missed deadline).
  FP_rate       = mean of jobs_summary.FP_rate  (over-conservative:
                  predicted infeasible but job made deadline).
  EU_tie        = sum(shadow_gc.n_tie_mode) / sum(shadow_gc.n_candidates).
                  Fraction of GC candidates that tie on min gc_util under
                  the mode's lens. Fixed lens → close to 1.0.
  Divergence_W  = |sum(feas_m) - sum(feas_sh)| / sum(n_cand)  (shadow_write).
                  Proxy for §C1 chosen-block divergence — we don't replay
                  the mutating allocator, so we measure the gap in
                  feasibility judgments instead.
  Divergence_GC = mean(shadow_gc.div)  (fraction of GC decisions where
                  mode's victim ≠ STATE-lens victim).
  Divergence_Ad = mean(admit.div_admit)  (fraction of LaWL-S calls where
                  mode's admit/skip decision ≠ STATE-lens decision).
  Admit_rate    = mean of admit_summary.admit_rate.
  PEC_spread    = mean of (pec_max - pec_min)  (end-of-run wear spread).

Baseline row has "—" for schemes that never invoke LaWL-family instrumentation
(shadow_*, admit_*). These cells emit as empty strings for a clean paper table.

Usage:
  python3 build_table.py                     # default BASE_DIR=./sweep_results
  python3 build_table.py --base other_dir    # explicit base
  python3 build_table.py --out-dir tables    # output CSVs land in tables/
"""

import argparse
import glob
import os
import sys
import numpy as np
import pandas as pd

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
UTIL_VALUES = ["0.1", "0.15", "0.2", "0.25", "0.3"]
# Schemes in paper table order. File prefix is the key; display name is value.
SCHEMES = [
    ("baseline", "Baseline"),
    ("LaWL_opt", "LaWL-opt"),
    ("LaWL_avg", "LaWL-avg"),
    ("LaWL_pes", "LaWL-pes"),
    ("LaWL",     "LaWL"),
]
EQ5_LHS = {
    "baseline": "—",
    "LaWL_opt": "const",
    "LaWL_avg": "const",
    "LaWL_pes": "const",
    "LaWL":     "state",
}

# CSV schema definitions (must match src/emul_main.c writers exactly).
JOBS_SUM_COLS = ["cur_cp", "t_first_dl", "t_first_inf", "delta_unsafe",
                 "n_rel", "n_feas", "n_inf", "n_miss",
                 "FN", "FP", "TN", "TP", "FN_rate", "FP_rate",
                 "t_first_util_ov"]
ADMIT_SUM_COLS = ["cur_cp", "admit", "skip_ns", "skip_nv", "admit_rate",
                  "pec_max", "pec_min", "pec_mean", "pec_std"]
SHADOW_GC_COLS = ["cur_cp", "task", "n_cand", "n_tie", "min_util",
                  "pick_m", "pick_sh", "div"]
SHADOW_W_COLS  = ["cur_cp", "task", "n_cand", "feas_m", "feas_sh",
                  "pick_m", "pick_sh", "y_o", "div"]
ADMIT_COLS     = ["cur_cp", "wcu", "rrutil", "yng", "old", "dec",
                  "wcu_sh", "rrutil_sh", "dec_sh", "div_admit"]

# ---------------------------------------------------------------------------
# Loaders
# ---------------------------------------------------------------------------
def load_lifetime(scheme, u_dir):
    """Return (life_list, util_ov_list) — per-iter physical lifetime and
    per-iter first util-overflow timestamps (µs).

    Current format (one iter per line, 2 cols):
      end_cur_cp,t_first_util_overflow
    Legacy format (single comma-separated stream, values only):
      t1,t2,t3,...
    Legacy is silently promoted: util_ov_list is filled with -1s so callers
    that only need life_list keep working."""
    fp = os.path.join(u_dir, f"{scheme}_lifetime.csv")
    if not os.path.exists(fp):
        return [], []
    life, util_ov = [], []
    with open(fp) as f:
        text = f.read().strip()
    if not text:
        return [], []
    # Newline-delimited new format.
    if "\n" in text:
        for ln in text.splitlines():
            ln = ln.strip().rstrip(",")
            if not ln:
                continue
            parts = [x.strip() for x in ln.split(",") if x.strip()]
            if len(parts) >= 2:
                life.append(int(parts[0]))
                util_ov.append(int(parts[1]))
            elif len(parts) == 1:
                life.append(int(parts[0]))
                util_ov.append(-1)
        return life, util_ov
    # Legacy: single stream of comma-separated values, no util_overflow.
    life = [int(x) for x in text.rstrip(",").split(",") if x.strip()]
    util_ov = [-1] * len(life)
    return life, util_ov


def load_jobs_sum(scheme, u_dir):
    fp = os.path.join(u_dir, f"{scheme}_jobs_summary.csv")
    if not os.path.exists(fp):
        return pd.DataFrame(columns=JOBS_SUM_COLS)
    return pd.read_csv(fp, header=None, names=JOBS_SUM_COLS)


def load_admit_sum(scheme, u_dir):
    fp = os.path.join(u_dir, f"{scheme}_admit_summary.csv")
    if not os.path.exists(fp):
        return pd.DataFrame(columns=ADMIT_SUM_COLS)
    return pd.read_csv(fp, header=None, names=ADMIT_SUM_COLS)


def _iter_files(scheme, u_dir, kind):
    """Enumerate per-iter gz files for a given trace kind.
    Matches both `<scheme>_<kind>.csv.gz` (single-shot run, no SIM_ITER)
    and `<scheme>_<kind>_<NNN>.csv.gz` (sweep with SIM_ITER exported).
    Anchoring to `_<kind>[._]` guards against e.g. 'LaWL_opt_shadow_gc_*'
    leaking into scheme='LaWL' via prefix ambiguity — after `<scheme>_`
    the next token must be exactly `<kind>` followed by `.` or `_`."""
    bare    = os.path.join(u_dir, f"{scheme}_{kind}.csv.gz")
    numbered = glob.glob(os.path.join(u_dir, f"{scheme}_{kind}_[0-9]*.csv.gz"))
    out = numbered.copy()
    if os.path.exists(bare):
        out.append(bare)
    return sorted(out)


def aggregate_shadow_gc(scheme, u_dir):
    """Pool all per-iter shadow_gc rows and return (sum_n_tie, sum_n_cand,
    total_rows, sum_div)."""
    tie = cand = rows = div = 0
    for fp in _iter_files(scheme, u_dir, "shadow_gc"):
        try:
            df = pd.read_csv(fp, compression="gzip", header=None,
                             names=SHADOW_GC_COLS)
        except Exception as e:
            print(f"[WARN] skip {fp}: {e}", file=sys.stderr)
            continue
        tie  += df["n_tie"].sum()
        cand += df["n_cand"].sum()
        rows += len(df)
        div  += df["div"].sum()
    return int(tie), int(cand), int(rows), int(div)


def aggregate_shadow_write(scheme, u_dir):
    """Pool all per-iter shadow_write rows. Return (sum_feas_m, sum_feas_sh,
    sum_n_cand)."""
    fm = fs = cand = 0
    for fp in _iter_files(scheme, u_dir, "shadow_write"):
        try:
            df = pd.read_csv(fp, compression="gzip", header=None,
                             names=SHADOW_W_COLS)
        except Exception as e:
            print(f"[WARN] skip {fp}: {e}", file=sys.stderr)
            continue
        fm   += df["feas_m"].sum()
        fs   += df["feas_sh"].sum()
        cand += df["n_cand"].sum()
    return int(fm), int(fs), int(cand)


def aggregate_admit(scheme, u_dir):
    """Pool all per-iter admit trace rows. Return (sum_div_admit, total_rows)."""
    div = rows = 0
    for fp in _iter_files(scheme, u_dir, "admit"):
        try:
            df = pd.read_csv(fp, compression="gzip", header=None,
                             names=ADMIT_COLS)
        except Exception as e:
            print(f"[WARN] skip {fp}: {e}", file=sys.stderr)
            continue
        div  += df["div_admit"].sum()
        rows += len(df)
    return int(div), int(rows)


# ---------------------------------------------------------------------------
# Per-scheme row computation
# ---------------------------------------------------------------------------
def compute_row(scheme, display_name, u_dir, base_life):
    """Return a dict of paper-table columns for this (scheme, U) combo.
    base_life is Baseline's per-iter lifetime list, used as normalization
    denominator for Sched_LT and Phys_LT."""
    life, util_ov = load_lifetime(scheme, u_dir)
    js   = load_jobs_sum(scheme, u_dir)
    adm  = load_admit_sum(scheme, u_dir)

    n_iter = min(len(life), len(base_life)) if base_life else 0
    row = {
        "Mode":     display_name,
        "Eq5_LHS":  EQ5_LHS[scheme],
        "n_iter":   n_iter,
    }

    # ------- Sched_LT & Phys_LT (both normalized to Baseline) --------------
    if n_iter > 0:
        phys_ratios = [life[i] / base_life[i] for i in range(n_iter)
                       if base_life[i] > 0]
        row["Phys_LT"] = float(np.mean(phys_ratios)) if phys_ratios else np.nan

        if len(js) > 0:
            sched_ratios = []
            for i in range(min(n_iter, len(js))):
                t_inf = js["t_first_inf"].iloc[i]
                # Never predicted infeasible → schedulable to end-of-run
                effective = t_inf if t_inf != -1 else life[i]
                if base_life[i] > 0:
                    sched_ratios.append(effective / base_life[i])
            row["Sched_LT"] = float(np.mean(sched_ratios)) if sched_ratios else np.nan
        else:
            # Baseline has no jobs_summary in the paper sense — its Sched_LT
            # is the reference (1.00) since Phys_LT is 1.00 by definition
            # AND the test never runs. Report Phys_LT ratio here too.
            row["Sched_LT"] = row["Phys_LT"]

        # Util_OV_LT — Baseline-normalized first utilization-overflow moment.
        # Values from lifetime.csv col 2 (added post-hoc so total_u>=1.0
        # landmark is co-located with device-EOL timestamp). Iters where
        # overflow never fired (util_ov == -1) fall back to life[i], matching
        # the Sched_LT convention. Util_OV_n reports how many iters actually
        # overflowed — small n means the mode kept load below capacity for
        # most tasksets, so the mean is dominated by fallback lifetimes.
        util_ratios = []
        util_ov_hits = 0
        for i in range(min(n_iter, len(util_ov))):
            if util_ov[i] != -1:
                util_ov_hits += 1
                effective = util_ov[i]
            else:
                effective = life[i]
            if base_life[i] > 0:
                util_ratios.append(effective / base_life[i])
        row["Util_OV_LT"] = float(np.mean(util_ratios)) if util_ratios else np.nan
        row["Util_OV_n"]  = util_ov_hits
    else:
        row["Phys_LT"]   = np.nan
        row["Sched_LT"]  = np.nan
        row["Util_OV_LT"] = np.nan
        row["Util_OV_n"]  = 0

    # ------- Δ_unsafe (only iters with both timestamps fired) ---------------
    if len(js) > 0:
        valid = js[(js["t_first_dl"] != -1) & (js["t_first_inf"] != -1)]
        if len(valid) > 0:
            row["Delta_unsafe"] = float((valid["t_first_inf"]
                                         - valid["t_first_dl"]).mean())
            row["Delta_unsafe_n"] = int(len(valid))
        else:
            row["Delta_unsafe"]   = np.nan
            row["Delta_unsafe_n"] = 0
    else:
        row["Delta_unsafe"]   = np.nan
        row["Delta_unsafe_n"] = 0

    # ------- FN / FP rates --------------------------------------------------
    if len(js) > 0:
        row["FN_rate"] = float(js["FN_rate"].mean())
        row["FP_rate"] = float(js["FP_rate"].mean())
    else:
        row["FN_rate"] = np.nan
        row["FP_rate"] = np.nan

    # ------- EU tie (§C2 pooled aggregate) ---------------------------------
    tie, cand, gc_rows, gc_div = aggregate_shadow_gc(scheme, u_dir)
    row["EU_tie"]        = (tie / cand)  if cand > 0 else np.nan
    row["Divergence_GC"] = (gc_div / gc_rows) if gc_rows > 0 else np.nan

    # ------- Divergence (§C1 write feasibility gap) ------------------------
    fm, fs, sc_cand = aggregate_shadow_write(scheme, u_dir)
    row["Divergence_W"] = (abs(fm - fs) / sc_cand) if sc_cand > 0 else np.nan

    # ------- Divergence (§C3 admit criterion divergence, bonus col) --------
    ad_div, ad_rows = aggregate_admit(scheme, u_dir)
    row["Divergence_Ad"] = (ad_div / ad_rows) if ad_rows > 0 else np.nan

    # ------- Admit rate & terminal PEC spread ------------------------------
    if len(adm) > 0:
        row["Admit_rate"] = float(adm["admit_rate"].mean())
        row["PEC_spread"] = float((adm["pec_max"] - adm["pec_min"]).mean())
    else:
        row["Admit_rate"] = np.nan
        row["PEC_spread"] = np.nan

    return row


# ---------------------------------------------------------------------------
# Main driver
# ---------------------------------------------------------------------------
COLUMN_ORDER = [
    "Mode", "Eq5_LHS", "n_iter",
    "Sched_LT", "Phys_LT", "Util_OV_LT", "Util_OV_n",
    "Delta_unsafe", "Delta_unsafe_n",
    "FN_rate", "FP_rate",
    "EU_tie", "Divergence_W", "Divergence_GC", "Divergence_Ad",
    "Admit_rate", "PEC_spread",
]

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--base", default="./sweep_results",
                    help="Base directory containing u_<UTIL>/ folders")
    ap.add_argument("--util-list", default=",".join(UTIL_VALUES),
                    help="Comma-separated U values to process")
    ap.add_argument("--out-dir", default=".",
                    help="Directory for output CSVs")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    util_list = [u.strip() for u in args.util_list.split(",") if u.strip()]

    all_rows = []
    for u in util_list:
        u_dir = os.path.join(args.base, f"u_{u}")
        if not os.path.isdir(u_dir):
            print(f"[SKIP] {u_dir} not found", file=sys.stderr)
            continue

        base_life = load_lifetime("baseline", u_dir)
        if not base_life:
            print(f"[WARN] {u_dir}: baseline_lifetime.csv missing/empty — "
                  f"normalization ratios will be NaN", file=sys.stderr)

        per_u_rows = []
        for scheme, name in SCHEMES:
            row = compute_row(scheme, name, u_dir, base_life)
            row["U"] = float(u)
            per_u_rows.append(row)
            all_rows.append(row)

        df = pd.DataFrame(per_u_rows)[COLUMN_ORDER]
        out_fp = os.path.join(args.out_dir, f"table_u_{u}.csv")
        # na_rep="" mirrors the paper's "—" cells for schemes that don't
        # produce a given metric (baseline has no admit/shadow instrumentation).
        df.to_csv(out_fp, index=False, float_format="%.4f", na_rep="")
        print(f"[OK] {out_fp}  ({len(df)} rows)")

    # Aggregate long-format table with U as a column
    if all_rows:
        df_all = pd.DataFrame(all_rows)[["U"] + COLUMN_ORDER]
        out_fp = os.path.join(args.out_dir, "table_all.csv")
        df_all.to_csv(out_fp, index=False, float_format="%.4f", na_rep="")
        print(f"[OK] {out_fp}  ({len(df_all)} rows)")


if __name__ == "__main__":
    main()
