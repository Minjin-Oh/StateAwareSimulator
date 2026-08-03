#!/bin/bash
#
# LaWL sweep runner — implements the paper §2.9 sweep matrix.
#
#   axis         values
#   ----         -----------------------------------------------------------
#   task set     $NUM_TASKSETS regenerations (seed advances each iter)
#   U_{r+w}      $UTIL_LIST                (default "0.1 0.15 0.2 0.25 0.3")
#   variant      LaWL (STATE) | LaWL-opt | LaWL-avg | LaWL-pes
#
# All four variants at a given (U, iter) run against the same TASKGEN +
# WORKGEN output — identical taskset and identical invalidation-time
# profile. The ONLY thing that differs is the decision-layer latency lens
# (argv[12] -> latency_mode). Ground-truth PEC-dependent execution stays
# fixed (§2.4).
#
# Output contract with statesimul.out:
#   The binary writes  <scheme>_lifetime.csv / <scheme>_overhead.csv  into
#   $SIM_LOG_DIR (open in "a" mode). Filename encodes scheme (Baseline /
#   Hyb / LaWL_D / LaWL / LaWL_opt / LaWL_avg / LaWL_pes / Dyn) so all
#   variants for one U-value coexist in one directory without collision.
#   Results accumulate across iters and across script re-runs — delete
#   files under $BASE_DIR for a fresh sweep.
#
# Env knobs:
#   NUM_TASKSETS   $1 override, default 100
#   UTIL_LIST      space-separated list of U values (default: 0.1..0.3)
#   BASE_DIR       output root (default: ./sweep_results)
#   INIT_CYC       initial PEC cycles fed to statesimul.out (default 1500)
#   CORES          space-separated CPU IDs for pinning (needs >=4)
#   INCLUDE_EXTRAS =1 also runs baseline + LaWL-D per iter (needs 6 cores)

set -e

NUM_TASKSETS=${1:-100}
UTIL_LIST=${UTIL_LIST:-"0.1 0.15 0.2 0.25 0.3"}
BASE_DIR=${BASE_DIR:-./sweep_results}
INIT_CYC=${INIT_CYC:-1500}
INCLUDE_EXTRAS=${INCLUDE_EXTRAS:-0}

# taskset is Linux-only. On Windows/Git Bash it's absent, so every
# `taskset -c N ...` launch would fail with exit 127. Degrade to plain
# execution (no CPU pinning) when taskset is missing.
if command -v taskset >/dev/null 2>&1; then
    HAVE_TASKSET=1
else
    HAVE_TASKSET=0
    echo "[WARN] 'taskset' not found — running without CPU pinning"
fi

if [ "$INCLUDE_EXTRAS" = "1" ]; then
    NEED_CORES=6
else
    NEED_CORES=4
fi

NCPU=$(nproc 2>/dev/null || echo 4)
if [ -n "$CORES" ]; then
    read -r -a CORE_ARR <<< "$CORES"
else
    if [ "$NCPU" -ge "$NEED_CORES" ]; then
        CORE_ARR=()
        for k in $(seq $((NEED_CORES-1)) -1 0); do
            CORE_ARR+=($((NCPU-1-k)))
        done
    else
        HAVE_TASKSET=0
        echo "[WARN] only $NCPU cores available (<$NEED_CORES) — disabling pinning"
        CORE_ARR=(0 0 0 0 0 0)
    fi
fi
# Probe each requested core. Works under Docker `--cpuset-cpus=63-67` where
# nproc=5 but valid IDs are 63..67 — comparing IDs against nproc would
# falsely reject them.
if [ "$HAVE_TASKSET" = "1" ]; then
    for c in "${CORE_ARR[@]}"; do
        if ! taskset -c "$c" true 2>/dev/null; then
            echo "[WARN] taskset -c $c failed — disabling pinning"
            HAVE_TASKSET=0
            break
        fi
    done
    [ "$HAVE_TASKSET" = "1" ] && echo "[INFO] Pinning to cores: ${CORE_ARR[*]} (nproc=$NCPU)"
fi

# Launch statesimul.out with a specific SIM_LOG_DIR, optionally pinned.
# Extra args after $core are forwarded to statesimul.out verbatim.
# SIM_LOG_DIR comes from the exported env so all runs at a given U slot
# into the same directory; distinct scheme prefixes keep filenames unique.
run_sim() {
    local core="$1"; shift
    if [ "$HAVE_TASKSET" = "1" ]; then
        taskset -c "$core" ./statesimul.out "$@"
    else
        ./statesimul.out "$@"
    fi
}

echo "[INFO] Sweep matrix: variants=[LaWL, opt, avg, pes]$([ "$INCLUDE_EXTRAS" = "1" ] && echo ' +baseline +LaWL-D')"
echo "[INFO] U_LIST=$UTIL_LIST  NUM_TASKSETS=$NUM_TASKSETS  INIT_CYC=$INIT_CYC"
echo "[INFO] Output root: $BASE_DIR"

for U in $UTIL_LIST; do
    U_DIR="$BASE_DIR/u_${U}"
    mkdir -p "$U_DIR"
    export SIM_LOG_DIR="$U_DIR"

    echo "[INFO] ================== U=${U}  (out: $U_DIR) =================="

    for i in $(seq 1 "$NUM_TASKSETS"); do
        echo "[INFO] ---------- U=$U  taskset $i / $NUM_TASKSETS ----------"

        echo "[INFO] Generating task configuration..."
        ./statesimul.out NO NO NO TASKGEN 4 "$U" -1 0.05 0.95 0 \
            > /dev/null 2> "$U_DIR/taskgen.err"

        echo "[INFO] Generating workload..."
        ./statesimul.out NO NO NO WORKGEN 4 "$U" -1 0.05 0.95 0 \
            > /dev/null 2> "$U_DIR/workgen.err"

        echo "[INFO] Running simulations..."
        # All four LaWL variants share the decision path (UTILGC INVW RR005).
        # Only argv[12] (latency_mode) differs — that selects which lens the
        # controller uses AND (in C) which filename prefix is picked, so the
        # four processes write to distinct *_lifetime.csv / *_overhead.csv
        # files in the same $SIM_LOG_DIR without racing.
        declare -A pids
        run_sim "${CORE_ARR[0]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
            > /dev/null 2> "$U_DIR/LaWL.err" &
        pids[LaWL]=$!
        run_sim "${CORE_ARR[1]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_OPT \
            > /dev/null 2> "$U_DIR/LaWL_opt.err" &
        pids[opt]=$!
        run_sim "${CORE_ARR[2]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_AVG \
            > /dev/null 2> "$U_DIR/LaWL_avg.err" &
        pids[avg]=$!
        run_sim "${CORE_ARR[3]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_PES \
            > /dev/null 2> "$U_DIR/LaWL_pes.err" &
        pids[pes]=$!

        if [ "$INCLUDE_EXTRAS" = "1" ]; then
            run_sim "${CORE_ARR[4]}" NO NO SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> "$U_DIR/Baseline.err" &
            pids[baseline]=$!
            run_sim "${CORE_ARR[5]}" UTILGC INVW SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> "$U_DIR/LaWL_D.err" &
            pids[LaWL_D]=$!
        fi

        # Wait each individually so we know which one failed.
        set +e
        declare -A results
        for name in "${!pids[@]}"; do
            wait "${pids[$name]}"
            results[$name]=$?
        done
        set -e

        # Exit code 1 is the simulator's NORMAL end-of-life signal (MAXPE hit,
        # utilization > 1.0, or deadline miss — the events being measured).
        # Only treat other codes (segfault 139, sigkill 137, etc.) as failure.
        fail=0
        for name in "${!results[@]}"; do
            rc=${results[$name]}
            if [ "$rc" -eq 0 ] || [ "$rc" -eq 1 ]; then
                echo "[OK]    U=$U iter=$i $name  exit=$rc"
            else
                echo "[FAIL]  U=$U iter=$i $name  exit=$rc  (see $U_DIR/$name.err)"
                fail=1
            fi
        done
        if [ "$fail" -ne 0 ]; then
            echo "[ABORT] U=$U iter=$i had failures — stopping sweep"
            exit 1
        fi
    done
done

echo "[DONE] Completed sweep over U=[$UTIL_LIST] × $NUM_TASKSETS tasksets"
