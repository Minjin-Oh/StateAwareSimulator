#!/bin/bash
#
# LaWL sweep runner — implements the paper §2.9 sweep matrix.
#
#   axis         values
#   ----         -----------------------------------------------------------
#   task set     $NUM_TASKSETS regenerations (seed advances each iter)
#   U_{r+w}      $UTIL_LIST                (default "0.1 0.15 0.2 0.25 0.3")
#   variant      LaWL | LaWL-Opt | LaWL-Nom | LaWL-Pes
#
# All four variants at a given (U, iter) run against the same TASKGEN +
# WORKGEN output, i.e. identical taskset and identical invalidation-time
# profile — the ONLY thing that differs is the decision-layer latency lens
# (argv[12] -> latency_mode). Ground-truth PEC-dependent execution stays
# fixed (§2.4).
#
# Output goes to $BASE_DIR/u_<U>/<variant-dir>/*_lifetime.csv|_overhead.csv,
# via the SIM_LOG_DIR env var honored by emul_main.c. Results accumulate
# with fopen("a") — delete files under $BASE_DIR to start fresh.
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

# Need 4 cores for the paper matrix, 6 if extras are on.
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
# Probe each requested core. This works under Docker `--cpuset-cpus=63-67`
# where nproc=5 but valid IDs are 63..67 — comparing IDs against nproc
# would falsely reject them.
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

run_sim() {
    local core="$1"; shift
    if [ "$HAVE_TASKSET" = "1" ]; then
        taskset -c "$core" "$@"
    else
        "$@"
    fi
}

# Map internal mode -> subdir under $SIM_LOG_DIR (see emul_main.c tagging).
declare -A DIR_OF=(
    [LaWL]=LaWL
    [Opt]=LaWL-Opt
    [Nom]=LaWL-Nom
    [Pes]=LaWL-Pes
    [baseline]=baseline
    [LaWL_D]=LaWL-D
)

# NOTE on accumulation: statesimul.out opens *_lifetime.csv / *_overhead.csv
# with fopen("a") under $SIM_LOG_DIR/<variant-dir>/, so results accumulate
# naturally across iters AND across script invocations. Delete files under
# $BASE_DIR/u_<U>/ manually for a fresh sweep.

echo "[INFO] Sweep matrix: variants=[LaWL, Opt, Nom, Pes]$([ "$INCLUDE_EXTRAS" = "1" ] && echo ' +baseline +LaWL-D')"
echo "[INFO] U_LIST=$UTIL_LIST  NUM_TASKSETS=$NUM_TASKSETS  INIT_CYC=$INIT_CYC"
echo "[INFO] Output root: $BASE_DIR"

for U in $UTIL_LIST; do
    U_DIR="$BASE_DIR/u_${U}"
    # Make every subdir referenced by DIR_OF up-front so fopen("a") succeeds
    # even for variants we skip on this run — cheap and idempotent.
    mkdir -p "$U_DIR"/{LaWL,LaWL-Opt,LaWL-Nom,LaWL-Pes,baseline,LaWL-D}
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
        # All four variants use the LaWL decision path (UTILGC INVW RR005);
        # only argv[12] (latency_mode) differs. LaWL omits argv[12] so it
        # defaults to STATE (assumed == physical).
        declare -A pids
        run_sim "${CORE_ARR[0]}" ./statesimul.out UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
            > /dev/null 2> "$U_DIR/${DIR_OF[LaWL]}/run.err" &
        pids[LaWL]=$!
        run_sim "${CORE_ARR[1]}" ./statesimul.out UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_OPT \
            > /dev/null 2> "$U_DIR/${DIR_OF[Opt]}/run.err" &
        pids[Opt]=$!
        run_sim "${CORE_ARR[2]}" ./statesimul.out UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_NOM \
            > /dev/null 2> "$U_DIR/${DIR_OF[Nom]}/run.err" &
        pids[Nom]=$!
        run_sim "${CORE_ARR[3]}" ./statesimul.out UTILGC INVW RR005 nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_PES \
            > /dev/null 2> "$U_DIR/${DIR_OF[Pes]}/run.err" &
        pids[Pes]=$!

        if [ "$INCLUDE_EXTRAS" = "1" ]; then
            run_sim "${CORE_ARR[4]}" ./statesimul.out NO NO SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> "$U_DIR/${DIR_OF[baseline]}/run.err" &
            pids[baseline]=$!
            run_sim "${CORE_ARR[5]}" ./statesimul.out UTILGC INVW SKIPRR nogen 4 "$U" -1 0.05 0.95 0 $INIT_CYC \
                > /dev/null 2> "$U_DIR/${DIR_OF[LaWL_D]}/run.err" &
            pids[LaWL_D]=$!
        fi

        # Wait each individually so we know which one failed
        set +e
        declare -A results
        for name in "${!pids[@]}"; do
            wait "${pids[$name]}"
            results[$name]=$?
        done
        set -e

        # The simulator uses exit code 1 as its NORMAL end-of-life signal —
        # fired from emul_main.c when a block hits MAXPE, utilization
        # exceeds 1.0, or a deadline is missed. Those are exactly the events
        # we're measuring. Only treat other codes (segfault 139, sigkill 137,
        # etc.) as failure.
        fail=0
        for name in "${!results[@]}"; do
            rc=${results[$name]}
            if [ "$rc" -eq 0 ] || [ "$rc" -eq 1 ]; then
                echo "[OK]    U=$U iter=$i $name  exit=$rc"
            else
                echo "[FAIL]  U=$U iter=$i $name  exit=$rc  (see $U_DIR/${DIR_OF[$name]}/run.err)"
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
