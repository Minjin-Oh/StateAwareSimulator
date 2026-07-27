#!/bin/bash

set -e

BASE_DIR=./ablation
NUM_TASKSETS=${1:-100}

mkdir -p "$BASE_DIR"/{baseline,fixed-S,fixed-E,LaWL-D,LaWL}

# taskset is Linux-only. On Windows/Git Bash it's absent, so every
# `taskset -c N ...` launch would fail with exit 127. Degrade to plain
# execution (no CPU pinning) when taskset is missing.
if command -v taskset >/dev/null 2>&1; then
    HAVE_TASKSET=1
else
    HAVE_TASKSET=0
    echo "[WARN] 'taskset' not found — running without CPU pinning"
fi

# Pick 5 cores for pinning. Override with CORES env var (space-separated,
# e.g. `CORES="23 24 25 26 27" ./run_ablation.sh`). Auto-picks the last 5
# cores otherwise so it works on both a 16-core laptop and a 40+ core
# server. If any picked core exceeds nproc, disable pinning entirely.
NCPU=$(nproc 2>/dev/null || echo 4)
if [ -n "$CORES" ]; then
    read -r -a CORE_ARR <<< "$CORES"
else
    if [ "$NCPU" -ge 5 ]; then
        CORE_ARR=($((NCPU-5)) $((NCPU-4)) $((NCPU-3)) $((NCPU-2)) $((NCPU-1)))
    else
        HAVE_TASKSET=0
        echo "[WARN] only $NCPU cores available (<5) — disabling pinning"
        CORE_ARR=(0 0 0 0 0)
    fi
fi
if [ "$HAVE_TASKSET" = "1" ]; then
    for c in "${CORE_ARR[@]}"; do
        if [ "$c" -ge "$NCPU" ] || [ "$c" -lt 0 ]; then
            echo "[WARN] core $c out of range [0,$((NCPU-1))] — disabling pinning"
            HAVE_TASKSET=0
            break
        fi
    done
    [ "$HAVE_TASKSET" = "1" ] && echo "[INFO] Pinning to cores: ${CORE_ARR[*]} (nproc=$NCPU)"
fi

run_sim() {
    local core="$1"; shift
    if [ "$HAVE_TASKSET" = "1" ]; then
        taskset -c "$core" "$@"
    else
        "$@"
    fi
}

# Map internal mode name -> ablation subdir (needed because `${name/_/-}`
# only helps for LaWL_D; fixedS/fixedE have no underscore).
declare -A DIR_OF=(
    [baseline]=baseline
    [fixedS]=fixed-S
    [fixedE]=fixed-E
    [LaWL_D]=LaWL-D
    [LaWL]=LaWL
)

# NOTE on accumulation: the simulator opens *_lifetime.csv and *_overhead.csv
# with fopen("a") directly under ablation/<mode>/, so results accumulate
# naturally — both across the N iterations of this script AND across repeated
# invocations of the script. No wipe here on purpose: re-running the script
# adds more samples on top of what's already there. Delete the target files
# manually if you want a fresh sweep.

for i in $(seq 1 "$NUM_TASKSETS"); do
    echo "[INFO] ================ Taskset $i / $NUM_TASKSETS ================"

    echo "[INFO] Generating task configuration..."
    ./statesimul.out NO NO NO TASKGEN 4 0.3 -1 0.05 0.95 0 \
        > /dev/null 2> "$BASE_DIR/taskgen.err"

    echo "[INFO] Generating workload..."
    ./statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95 0 \
        > /dev/null 2> "$BASE_DIR/workgen.err"

    echo "[INFO] Running simulations (iteration $i)..."

    run_sim "${CORE_ARR[0]}" ./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0 \
        > /dev/null 2> "$BASE_DIR/baseline/run.err" &
    pid_baseline=$!

    run_sim "${CORE_ARR[1]}" ./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0 0 FIXED_S \
        > /dev/null 2> "$BASE_DIR/fixed-S/run.err" &
    pid_fixedS=$!

    run_sim "${CORE_ARR[2]}" ./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0 0 FIXED_E \
        > /dev/null 2> "$BASE_DIR/fixed-E/run.err" &
    pid_fixedE=$!

    run_sim "${CORE_ARR[3]}" ./statesimul.out UTILGC INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 0 \
        > /dev/null 2> "$BASE_DIR/LaWL-D/run.err" &
    pid_LaWL_D=$!

    run_sim "${CORE_ARR[4]}" ./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0 \
        > /dev/null 2> "$BASE_DIR/LaWL/run.err" &
    pid_LaWL=$!

    # Wait each individually so we know which one failed
    set +e
    declare -A results
    for name in baseline fixedS fixedE LaWL_D LaWL; do
        var="pid_${name}"
        wait "${!var}"
        results[$name]=$?
    done
    set -e

    # The simulator uses exit code 1 as its NORMAL end-of-life signal —
    # fired from emul_main.c when a block hits MAXPE, utilization exceeds 1.0,
    # or a deadline is missed. Those are exactly the events we're measuring.
    # Only treat other exit codes (segfault 139, sigkill 137, etc.) as failure.
    fail=0
    for name in "${!results[@]}"; do
        rc=${results[$name]}
        if [ "$rc" -eq 0 ] || [ "$rc" -eq 1 ]; then
            echo "[OK]    iter=$i $name  exit=$rc"
        else
            echo "[FAIL]  iter=$i $name  exit=$rc  (see $BASE_DIR/${DIR_OF[$name]}/run.err)"
            fail=1
        fi
    done
    if [ "$fail" -ne 0 ]; then
        echo "[ABORT] iteration $i had failures — stopping sweep"
        exit 1
    fi
done

echo "[DONE] Completed $NUM_TASKSETS taskset iterations"
