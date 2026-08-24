#!/bin/bash
# Pre-generate NUM_RUNS workloads for each utilization value in UTILS.
# Companion to run_simul.sh (which consumes these workloads).
#
# Usage: ./gen_simul.sh [NUM_RUNS] [BASE_DIR]
#   NUM_RUNS defaults to 100
#   BASE_DIR defaults to ./results_util
#
# Layout produced:
#   $BASE_DIR/workloads/util_<U>/<NNN>/
#     ├── taskparam.csv
#     ├── cyc.csv            (only if TASKGEN emits it)
#     ├── loc.csv
#     ├── wr_t*.csv
#     ├── rd_t*.csv
#     ├── gen.log
#     └── .done              (marker: this iteration finished successfully)
#
# If .done exists for an iteration, generation is skipped (idempotent).
# If $BASE_DIR/workloads.tar.gz exists but the workloads/ dir does not,
# it is auto-extracted first so skip-detection works across compressed runs.

set -u

NUM_RUNS="${1:-100}"
BASE_DIR="${2:-./results_util}"
BIN="$(readlink -f ./statesimul.out)"

UTILS=(0.1 0.15 0.2 0.25 0.3)

if [ ! -x "$BIN" ]; then
    echo "error: $BIN not found or not executable" >&2
    exit 1
fi

mkdir -p "$BASE_DIR"
WORKLOADS_DIR="$BASE_DIR/workloads"

# auto-extract previously-compressed workloads so we can extend / re-use
if [ ! -d "$WORKLOADS_DIR" ] && [ -f "$BASE_DIR/workloads.tar.gz" ]; then
    echo "[INFO] extracting existing workloads.tar.gz"
    tar xzf "$BASE_DIR/workloads.tar.gz" -C "$BASE_DIR"
fi
mkdir -p "$WORKLOADS_DIR"

for U in "${UTILS[@]}"; do
    UTIL_DIR="$WORKLOADS_DIR/util_$U"
    mkdir -p "$UTIL_DIR"
    echo "########## generating workloads: util=$U (-> $UTIL_DIR) ##########"

    for i in $(seq 1 "$NUM_RUNS"); do
        IDX=$(printf "%03d" "$i")
        GENDIR="$UTIL_DIR/$IDX"

        if [ -f "$GENDIR/.done" ]; then
            continue
        fi

        rm -rf "$GENDIR"
        mkdir -p "$GENDIR"
        if (
            cd "$GENDIR" && \
            "$BIN" NO NO NO TASKGEN 4 "$U" -1 0.05 0.95 0 > gen.log  2> gen.err && \
            "$BIN" NO NO NO WORKGEN 4 "$U" -1 0.05 0.95 0 >> gen.log 2>> gen.err
        ); then
            touch "$GENDIR/.done"
        else
            echo "  error: workload generation failed util=$U iter $IDX" >&2
            rm -rf "$GENDIR"
        fi
    done
done

echo "Done. Workloads under $WORKLOADS_DIR"
