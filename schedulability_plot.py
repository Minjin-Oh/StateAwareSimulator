#!/usr/bin/env python3
"""
schedulability_plot.py

Read sweep_results/schedulability.csv produced by schedulability_sweep.sh,
compute per-scheme schedulability ratio at each U_r, and emit a Fig.2a-style
line plot.

Exit code -> outcome mapping (from types.h):
  0   EXIT_SUCCESS_RUNTIME  -> schedulable
  1   EXIT_UTIL_OVERFLOW    -> UNSCHEDULABLE
  2   EXIT_MAXPE            -> schedulable (endurance limit; user policy)
  3   EXIT_DL_MISS          -> UNSCHEDULABLE
  10  EXIT_TASKGEN_FAIL     -> exclude from denominator
  124 /usr/bin/timeout      -> schedulable-so-far (survived RUN_TIMEOUT)

Usage:
  python schedulability_plot.py                        # writes fig2a_style.png
  python schedulability_plot.py --csv other.csv --out foo.png
  python schedulability_plot.py --strict               # MAXPE counts as fail
"""

from __future__ import annotations
import argparse
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


SCHED_LENIENT = {0, 2, 124}   # MAXPE and timeout survivors count as success
SCHED_STRICT  = {0, 124}      # only clean RUNTIME finish (or timeout) counts
UNSCHED       = {1, 3}
SKIP          = {10, "GEN_TASK_FAIL", "GEN_WL_FAIL"}

SCHEME_ORDER = ["Baseline", "WAOGC", "DynWL", "HybWL", "LaWL"]
SCHEME_COLORS = {
    "Baseline": "#888888",
    "WAOGC":    "#e07a5f",
    "DynWL":    "#81b29a",
    "HybWL":    "#3d5a80",
    "LaWL":     "#c1121f",
}
SCHEME_MARKERS = {
    "Baseline": "s",
    "WAOGC":    "^",
    "DynWL":    "v",
    "HybWL":    "D",
    "LaWL":     "o",
}


def load(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    # exit_code column can carry non-numeric sentinels ("GEN_TASK_FAIL" etc.)
    def _norm(x):
        try:
            return int(x)
        except (ValueError, TypeError):
            return x
    df["exit_code"] = df["exit_code"].map(_norm)
    df["util"] = df["util"].astype(float)
    return df


def classify(df: pd.DataFrame, strict: bool) -> pd.DataFrame:
    ok = SCHED_STRICT if strict else SCHED_LENIENT
    df = df[~df["exit_code"].isin(SKIP)].copy()
    df["schedulable"] = df["exit_code"].isin(ok)
    return df


def ratio_table(df: pd.DataFrame) -> pd.DataFrame:
    # rows = util, cols = scheme, values = mean(schedulable)
    tbl = (df.groupby(["util", "scheme"])["schedulable"]
             .mean()
             .unstack("scheme"))
    # keep only known schemes, in canonical order
    keep = [c for c in SCHEME_ORDER if c in tbl.columns]
    return tbl[keep]


def sample_size_table(df: pd.DataFrame) -> pd.DataFrame:
    return (df.groupby(["util", "scheme"])["schedulable"]
              .count()
              .unstack("scheme"))


def plot(tbl: pd.DataFrame, out_path: Path, title: str) -> None:
    fig, ax = plt.subplots(figsize=(8, 5))
    for scheme in tbl.columns:
        ax.plot(
            tbl.index, tbl[scheme],
            label=scheme,
            color=SCHEME_COLORS.get(scheme, None),
            marker=SCHEME_MARKERS.get(scheme, "o"),
            linewidth=1.5,
            markersize=6,
        )
    ax.set_xlabel(r"Total utilization of RT tasks ($U_r$)")
    ax.set_ylabel("Schedulability ratio")
    ax.set_ylim(-0.02, 1.02)
    ax.set_xlim(tbl.index.min() - 0.02, tbl.index.max() + 0.02)
    ax.grid(True, linestyle=":", linewidth=0.7)
    ax.legend(loc="lower left", framealpha=0.9)
    ax.set_title(title)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"[plot] wrote {out_path}")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--csv", default="sweep_results/schedulability.csv",
                   help="master CSV produced by schedulability_sweep.sh")
    p.add_argument("--out", default="sweep_results/fig2a_style.png")
    p.add_argument("--strict", action="store_true",
                   help="MAXPE counts as failure (default: MAXPE = success)")
    p.add_argument("--print-table", action="store_true",
                   help="also print the schedulability ratio table to stdout")
    args = p.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        print(f"[plot] CSV not found: {csv_path}", file=sys.stderr)
        return 1

    df = load(csv_path)
    df = classify(df, strict=args.strict)
    if df.empty:
        print("[plot] no usable rows after classification", file=sys.stderr)
        return 1

    tbl = ratio_table(df)
    n = sample_size_table(df)

    if args.print_table:
        print("== schedulability ratio ==")
        print(tbl.round(3).to_string())
        print("\n== sample sizes ==")
        print(n.to_string())

    title = "Schedulability ratio vs $U_r$"
    title += " (strict: MAXPE = fail)" if args.strict else " (lenient: MAXPE = ok)"
    plot(tbl, Path(args.out), title)
    return 0


if __name__ == "__main__":
    sys.exit(main())
