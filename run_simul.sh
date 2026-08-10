#!/bin/bash
#
# LaWL sweep runner — implements the paper §2.9 sweep matrix.
#
#   axis         values
#   ----         -----------------------------------------------------------
#   task set     $NUM_TASKSETS regenerations (seed advances each iter)
#   U_{r+w}      $UTIL_LIST                (default "0.1 0.15 0.2 0.25 0.3")
#   variant      Baseline | LaWL (STATE) | LaWL-opt | LaWL-avg | LaWL-pes
#                (+ LaWL-D when INCLUDE_EXTRAS=1)
#
# All variants at a given (U, iter) run against the same TASKGEN + WORKGEN
# output — identical taskset and identical invalidation-time profile. The
# 4 LaWL variants differ only in the decision-layer latency lens (argv[12]
# -> latency_mode); Baseline (NO NO SKIPRR) uses no LaWL controller at all
# and provides the lower-bound reference for the §C4 table.
#
# Output contract with statesimul.out:
#   The binary writes fixed filenames (LaWL_lifetime.csv, LaWL_opt_*.csv,
#   Baseline_*, LaWL_D_*, ...) into the CURRENT WORKING DIRECTORY. This
#   script therefore `cd`s into a per-U output directory before invoking
#   the binary, so each U slice collects its own files. TASKGEN/WORKGEN
#   also run in that dir so their auxiliaries (taskparam.csv, wr_t*.csv,
#   rd_t*.csv, loc.csv, ...) are self-contained per U — the 4 parallel
#   variant runs then all read the same taskparam.csv for pairwise
#   comparison at each iteration.
#
# Env knobs:
#   NUM_TASKSETS   $1 override, default 100
#   UTIL_LIST      space-separated list of U values (default: 0.1..0.3)
#   BASE_DIR       output root, default: ./sweep_results
#   INIT_CYC       initial PEC cycles fed to statesimul.out (default 1500)
#   CORES          space-separated CPU IDs for pinning (needs >=5, or 6 with EXTRAS)
#   INCLUDE_EXTRAS =1 also runs LaWL-D per iter (needs 6 cores; baseline
#                    always runs regardless of this flag)

set -e

NUM_TASKSETS=${1:-100}
UTIL_LIST=${UTIL_LIST:-"0.1 0.15 0.2 0.25 0.3"}
BASE_DIR=${BASE_DIR:-./sweep_results}
INIT_CYC=${INIT_CYC:-1500}
INCLUDE_EXTRAS=${INCLUDE_EXTRAS:-0}

# Resolve to absolute paths so we can cd around without breaking references.
# statesimul.out lives at the script's origin; BASE_DIR may be given as a
# relative path but is resolved before any cd so it always points at the
# same place.
ORIG_DIR=$(pwd)
SIMUL="$ORIG_DIR/statesimul.out"
if [ ! -x "$SIMUL" ]; then
    echo "[FATAL] statesimul.out not found or not executable at $SIMUL"
    exit 1
fi
case "$BASE_DIR" in
    /*) ;;                          # already absolute
    *)  BASE_DIR="$ORIG_DIR/${BASE_DIR#./}" ;;
esac

# taskset is Linux-only. On Windows/Git Bash it's absent, so every
# `taskset -c N ...` launch would fail with exit 127. Degrade to plain
# execution (no CPU pinning) when taskset is missing.
if command -v taskset >/dev/null 2>&1; then
    HAVE_TASKSET=1
else
    HAVE_TASKSET=0
    echo "[WARN] 'taskset' not found — running without CPU pinning"
fi

# Default: 4 LaWL variants + Baseline = 5 cores.
# With INCLUDE_EXTRAS=1: also LaWL-D = 6 cores.
if [ "$INCLUDE_EXTRAS" = "1" ]; then
    NEED_CORES=6
else
    NEED_CORES=5
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

# Launch statesimul.out with a specific pinned core, optionally.
# The binary writes CSVs to CWD; caller is responsible for having cd'd
# into the desired output directory.
run_sim() {
    local core="$1"; shift
    if [ "$HAVE_TASKSET" = "1" ]; then
        taskset -c "$core" "$SIMUL" "$@"
    else
        "$SIMUL" "$@"
    fi
}

echo "[INFO] Sweep matrix: variants=[Baseline, LaWL, opt, avg, pes]$([ "$INCLUDE_EXTRAS" = "1" ] && echo ' +LaWL-D')"
echo "[INFO] U_LIST=$UTIL_LIST  NUM_TASKSETS=$NUM_TASKSETS  INIT_CYC=$INIT_CYC"
echo "[INFO] statesimul.out : $SIMUL"
echo "[INFO] Output root    : $BASE_DIR"

for U in $UTIL_LIST; do
    U_DIR="$BASE_DIR/u_${U}"
    mkdir -p "$U_DIR"

    echo "[INFO] ================== U=${U}  (cwd: $U_DIR) =================="

    # Run all commands in a subshell so `cd` doesn't leak between U slices.
    (
        cd "$U_DIR"

        for i in $(seq 1 "$NUM_TASKSETS"); do
            echo "[INFO] ---------- U=$U  taskset $i / $NUM_TASKSETS ----------"

            echo "[INFO] Generating task configuration..."
            "$SIMUL" NO NO NO TASKGEN 4 "$U" -1 0.05 0.95 0 \
                > /dev/null 2> taskgen.err

            echo "[INFO] Generating workload..."
            "$SIMUL" NO NO NO WORKGEN 4 "$U" -1 0.05 0.95 0 \
                > /dev/null 2> workgen.err

            echo "[INFO] Running simulations..."
            # The 4 LaWL variants share the decision path (UTILGC INVW RR005).
            # Only argv[12] (latency_mode) differs — that selects both the lens
            # the controller uses AND (in C) which filename prefix the binary
            # picks, so the four processes write to distinct *_lifetime.csv /
            # *_overhead.csv / *_utilization.csv.gz files in the same CWD
            # without racing.
            #
            # Baseline (NO NO SKIPRR) runs alongside — it doesn't invoke the
            # LaWL controller at all and gives the §C4 lower-bound reference.
            # Its output filenames start with "baseline_" so no collision.
            declare -A pids
            run_sim "${CORE_ARR[0]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> LaWL.err &
            pids[LaWL]=$!
            run_sim "${CORE_ARR[1]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_OPT \
                > /dev/null 2> LaWL_opt.err &
            pids[opt]=$!
            run_sim "${CORE_ARR[2]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_AVG \
                > /dev/null 2> LaWL_avg.err &
            pids[avg]=$!
            run_sim "${CORE_ARR[3]}" UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_PES \
                > /dev/null 2> LaWL_pes.err &
            pids[pes]=$!
            run_sim "${CORE_ARR[4]}" NO NO SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> baseline.err &
            pids[baseline]=$!

            if [ "$INCLUDE_EXTRAS" = "1" ]; then
                run_sim "${CORE_ARR[5]}" UTILGC INVW SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                    > /dev/null 2> LaWL_D.err &
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

            # Exit code 1 is the simulator's NORMAL end-of-life signal (MAXPE
            # hit — the sole termination condition now that overflow/dl_miss
            # only log and continue). Only treat other codes (segfault 139,
            # sigkill 137, exit 2 = FATAL log-open mismatch, etc.) as failure.
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
    )
done

echo "[DONE] Completed sweep over U=[$UTIL_LIST] × $NUM_TASKSETS tasksets"
