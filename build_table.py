#!/usr/bin/env python3
"""Build the ablation Table (plan §3) from run_variants.sh outputs.

Columns per the plan:
    Scheme   |  Norm. lifetime  |  Silent misses  |  Reloc. admission (%)

Extra columns useful for the paper text and §6 sanity checks:
    schedulable lifetime, FN_rate, FP_rate, wfb/gfb (pes fallback counts).

Input:
    A directory that holds either
      (a) extracted per-U subdirs:  u_0.1/, u_0.15/, ...
      (b) archives from run_variants.sh: results_u_0.1.tar.gz, ...
    Case (b) is extracted to a tempdir on the fly.

Usage:
    python3 build_table.py results/                          # pretty stdout
    python3 build_table.py results/ -o table.csv             # CSV file
    python3 build_table.py results/ --utils "0.2 0.3"        # subset
"""

from __future__ import annotations

import argparse
import sys
import tarfile
import tempfile
from contextlib import contextmanager
from pathlib import Path

import numpy as np
import pandas as pd

# --- Config ---------------------------------------------------------------

# Row order in the emitted table. Baseline is included as the normalization
# anchor (its Norm.Life is trivially 1.0 by construction).
SCHEMES = [
    ("baseline", "Baseline"),
    ("LaWL",     "LaWL"),
    ("LaWL_opt", "LaWL-opt"),
    ("LaWL_avg", "LaWL-avg"),
    ("LaWL_pes", "LaWL-pes"),
]
BASELINE_KEY = "baseline"

# CSV schemas — must match src/emul_main.c writers exactly.
LIFETIME_COLS  = ["cur_cp", "t_first_util_ov"]
JOBS_SUM_COLS  = ["cur_cp", "t_first_dl", "t_first_inf", "delta_unsafe",
                  "n_rel", "n_feas", "n_inf", "n_miss",
                  "FN", "FP", "TN", "TP", "FN_rate", "FP_rate",
                  "t_first_util_ov"]
ADMIT_SUM_COLS = ["cur_cp", "admit", "skip_ns", "skip_nv", "admit_rate",
                  "pec_max", "pec_min", "pec_mean", "pec_std",
                  "wfb_events", "gfb_events"]


# --- Loading --------------------------------------------------------------

def _load(u_dir: Path, scheme: str, kind: str, cols: list[str]) -> pd.DataFrame:
    fp = u_dir / f"{scheme}_{kind}.csv"
    if not fp.exists():
        return pd.DataFrame(columns=cols)
    # emul_main.c writes headerless CSVs (each row appended one per seed).
    return pd.read_csv(fp, header=None, names=cols)


@contextmanager
def _resolve_u_dir(root: Path, U: str):
    """Yield a Path to u_${U}/. Extract results_u_${U}.tar.gz if only the
    archive is present."""
    direct = root / f"u_{U}"
    if direct.is_dir():
        yield direct
        return
    tgz = root / f"results_u_{U}.tar.gz"
    if not tgz.exists():
        raise FileNotFoundError(
            f"neither {direct} nor {tgz} exists — did run_variants.sh complete for U={U}?"
        )
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        with tarfile.open(tgz, "r:gz") as tf:
            # tar built via `tar -czf -C RESULT_DIR u_${U}` → top-level u_${U}/.
            tf.extractall(tmp_path)
        yield tmp_path / f"u_{U}"


# --- Per-U aggregation ----------------------------------------------------

def _paired_ratio(numer: pd.Series, denom: pd.Series) -> tuple[float, float]:
    """Compute mean/std of paired ratios. Rows align by index because
    run_variants.sh processes seeds in order and each variant appends one
    row per seed. Returns (nan, nan) if either side is empty or shorter."""
    n = min(len(numer), len(denom))
    if n == 0:
        return float("nan"), float("nan")
    a = np.asarray(numer.iloc[:n], dtype=float)
    b = np.asarray(denom.iloc[:n], dtype=float)
    ratio = np.where(b > 0, a / b, np.nan)
    return float(np.nanmean(ratio)), float(np.nanstd(ratio))


def aggregate_u(u_dir: Path) -> pd.DataFrame:
    """One row per scheme for this U slice."""
    base_life = _load(u_dir, BASELINE_KEY, "lifetime", LIFETIME_COLS)

    rows = []
    for key, display in SCHEMES:
        life = _load(u_dir, key, "lifetime",      LIFETIME_COLS)
        jobs = _load(u_dir, key, "jobs_summary",  JOBS_SUM_COLS)
        adm  = _load(u_dir, key, "admit_summary", ADMIT_SUM_COLS)

        n_seeds = len(life)
        phys      = life["cur_cp"].astype(float) if n_seeds else pd.Series(dtype=float)
        sched_raw = life["t_first_util_ov"].astype(float) if n_seeds else pd.Series(dtype=float)
        # -1 sentinel = "util never overflowed" → schedulable lifetime is
        # at least the physical run length; use phys as a lower-bound proxy.
        sched = sched_raw.where(sched_raw != -1, phys) if n_seeds else pd.Series(dtype=float)

        # Paired normalization vs baseline (row i in each *_lifetime.csv
        # corresponds to seed i, since run_variants.sh runs all 5 variants
        # for one seed before moving to the next).
        norm_phys_mean,  norm_phys_std  = _paired_ratio(phys,  base_life["cur_cp"]) if len(base_life) else (np.nan, np.nan)
        norm_sched_mean, norm_sched_std = _paired_ratio(sched, base_life["cur_cp"]) if len(base_life) else (np.nan, np.nan)

        row = {
            "scheme":            display,
            "n_seeds":           n_seeds,
            "phys_life_mean":    float(phys.mean())  if n_seeds else np.nan,
            "sched_life_mean":   float(sched.mean()) if n_seeds else np.nan,
            "norm_life_mean":    norm_phys_mean,
            "norm_life_std":     norm_phys_std,
            "norm_sched_mean":   norm_sched_mean,
            "norm_sched_std":    norm_sched_std,
            "silent_miss_mean":  float(jobs["FN"].mean())      if len(jobs) else np.nan,
            "silent_miss_std":   float(jobs["FN"].std())       if len(jobs) else np.nan,
            "FN_rate_mean":      float(jobs["FN_rate"].mean()) if len(jobs) else np.nan,
            "FP_rate_mean":      float(jobs["FP_rate"].mean()) if len(jobs) else np.nan,
            "admit_pct_mean":    float((adm["admit_rate"] * 100).mean()) if len(adm) else np.nan,
            "admit_pct_std":     float((adm["admit_rate"] * 100).std())  if len(adm) else np.nan,
            "wfb_events_mean":   float(adm["wfb_events"].mean()) if len(adm) else np.nan,
            "gfb_events_mean":   float(adm["gfb_events"].mean()) if len(adm) else np.nan,
        }
        rows.append(row)
    return pd.DataFrame(rows)


# --- Driver ---------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="Build the ablation Table (plan §3) from run_variants.sh outputs.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("input_dir", type=Path,
                    help="directory with u_${U}/ subdirs or results_u_${U}.tar.gz archives")
    ap.add_argument("-o", "--out", default="-",
                    help="output CSV path (default: pretty-print to stdout)")
    ap.add_argument("--utils", default="0.1 0.15 0.2 0.25 0.3",
                    help='space-separated U values (default: full sweep)')
    args = ap.parse_args()

    if not args.input_dir.is_dir():
        sys.exit(f"[FATAL] not a directory: {args.input_dir}")

    parts = []
    for U in args.utils.split():
        try:
            with _resolve_u_dir(args.input_dir, U) as u_dir:
                df = aggregate_u(u_dir)
                df.insert(0, "U", U)
                parts.append(df)
        except FileNotFoundError as e:
            print(f"[WARN] U={U}: {e}", file=sys.stderr)

    if not parts:
        sys.exit("[FATAL] no U slices loaded")

    full = pd.concat(parts, ignore_index=True)

    if args.out == "-":
        # Pretty stdout — narrower float format so lines fit in ~120 cols.
        print(full.to_string(index=False,
                             float_format=lambda x: f"{x:.4g}"))
    else:
        full.to_csv(args.out, index=False)
        print(f"[OK] wrote {args.out}  ({len(full)} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
