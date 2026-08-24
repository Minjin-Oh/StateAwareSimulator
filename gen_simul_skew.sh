#!/bin/bash
# Pre-generate NUM_RUNS workloads for each RNUM in RNUMS at a fixed
# utilization. Companion to run_simul_skew.sh.
#
# Usage: ./gen_simul_skew.sh [NUM_RUNS] [BASE_DIR]
#   NUM_RUNS defaults to 100
#   BASE_DIR defaults to ./results_skew
#
# Semantics of RNUM (0..TASKNUM):
#   With TASKNUM=4 and RNUM=S, the generated taskset has:
#     S       read-intensive  tasks
#     4 - S   write-intensive tasks
#
# emul_main.c dispatches skewness>=0 to generate_taskset_skew2, where the
# C-level `skewnum` argument means "# of WRITE-intensive tasks". We
# therefore pass argv[10] = TASKNUM - RNUM.
#
# Layout produced:
#   $BASE_DIR/workloads/rnum_<S>/<NNN>/
#     ├── taskparam.csv
#     ├── loc.csv
#     ├── wr_t*.csv
#     ├── rd_t*.csv
#     ├── gen.log
#     └── .done              (marker: iteration finished successfully)
#
# If .done exists for an iteration, generation is skipped (idempotent).
# If $BASE_DIR/workloads.tar.gz exists but the workloads/ dir does not,
# it is auto-extracted first so skip-detection works across compressed runs.

set -u

NUM_RUNS="${1:-100}"
BASE_DIR="${2:-./results_skew}"
BIN="$(readlink -f ./statesimul.out)"

U=0.２
TASKNUM=4
SKEWTYPE=1
RNUMS=(0 1 2 3 4)

if [ ! -x "$BIN" ]; then
    echo "error: $BIN not found or not executable" >&2
    exit 1
fi

mkdir -p "$BASE_DIR"
WORKLOADS_DIR="$BASE_DIR/workloads"

if [ ! -d "$WORKLOADS_DIR" ] && [ -f "$BASE_DIR/workloads.tar.gz" ]; then
    echo "[INFO] extracting existing workloads.tar.gz"
    tar xzf "$BASE_DIR/workloads.tar.gz" -C "$BASE_DIR"
fi
mkdir -p "$WORKLOADS_DIR"

for S in "${RNUMS[@]}"; do
    WNUM_ARG=$(( TASKNUM - S ))
    RNUM_DIR="$WORKLOADS_DIR/rnum_$S"
    mkdir -p "$RNUM_DIR"
    echo "########## generating workloads: rnum=$S (write=$WNUM_ARG, util=$U -> $RNUM_DIR) ##########"

    for i in $(seq 1 "$NUM_RUNS"); do
        IDX=$(printf "%03d" "$i")
        GENDIR="$RNUM_DIR/$IDX"

        if [ -f "$GENDIR/.done" ]; then
            continue
        fi

        rm -rf "$GENDIR"
        mkdir -p "$GENDIR"
        if (
            cd "$GENDIR" && \
            "$BIN" NO NO NO TASKGEN "$TASKNUM" "$U" "$SKEWTYPE" 0.05 0.95 "$WNUM_ARG" > gen.log  2> gen.err && \
            "$BIN" NO NO NO WORKGEN "$TASKNUM" "$U" "$SKEWTYPE" 0.05 0.95 "$WNUM_ARG" >> gen.log 2>> gen.err
        ); then
            touch "$GENDIR/.done"
        else
            echo "  error: workload generation failed rnum=$S iter $IDX" >&2
            rm -rf "$GENDIR"
        fi
    done
done

echo "Done. Workloads under $WORKLOADS_DIR"
