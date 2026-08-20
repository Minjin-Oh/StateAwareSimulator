#!/bin/bash
#
# Per-U variant sweep — for each U in UTIL_LIST, generate one taskset
# (TASKGEN + WORKGEN) inside its own directory, then launch the 5 variants
# (Baseline, LaWL, LaWL-opt, LaWL-avg, LaWL-pes) in parallel, one CPU core
# each. As soon as a U completes, the whole directory is tar.gz'd and the
# uncompressed copy removed to keep disk usage bounded across U values.
#
# Layout:
#   BASE_DIR/
#     u_0.1/    ← taskparam.csv + wr_t*.csv + rd_t*.csv + variant outputs
#     u_0.1.tar.gz  ← produced on U-slice completion; u_0.1/ removed after
#     u_0.15/
#     ...
#
# Env knobs:
#   BASE_DIR   root output dir (default: ./variant_run)
#   UTIL_LIST  space-separated U values (default: "0.1 0.15 0.2 0.25 0.3")
#   TASKNUM    argv[5] task count (default: 4, matches run_simul.sh)
#   INIT_CYC   initial PEC cycles (default: 1500)
#   CORES      space-separated CPU IDs for pinning (needs >=5)
#   KEEP_DIR   set to 1 to skip the post-tar rm (default: 0 = delete)

set -e

BASE_DIR=${BASE_DIR:-./variant_run}
UTIL_LIST=${UTIL_LIST:-"0.1 0.15 0.2 0.25 0.3"}
TASKNUM=${TASKNUM:-4}
INIT_CYC=${INIT_CYC:-1500}
KEEP_DIR=${KEEP_DIR:-0}

ORIG_DIR=$(pwd)
SIMUL="$ORIG_DIR/statesimul.out"
if [ ! -x "$SIMUL" ]; then
    echo "[FATAL] statesimul.out not found or not executable at $SIMUL"
    exit 1
fi
case "$BASE_DIR" in
    /*) ;;
    *)  BASE_DIR="$ORIG_DIR/${BASE_DIR#./}" ;;
esac
mkdir -p "$BASE_DIR"

# taskset pinning — Linux only; falls back to no pinning on Git Bash /
# environments without the util-linux binary.
if command -v taskset >/dev/null 2>&1; then
    HAVE_TASKSET=1
else
    HAVE_TASKSET=0
    echo "[WARN] 'taskset' not found — running without CPU pinning"
fi

NEED_CORES=5
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
        CORE_ARR=(0 0 0 0 0)
    fi
fi
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
        taskset -c "$core" "$SIMUL" "$@"
    else
        "$SIMUL" "$@"
    fi
}

echo "[INFO] Variants   : Baseline, LaWL, LaWL-opt, LaWL-avg, LaWL-pes"
echo "[INFO] UTIL_LIST  : $UTIL_LIST"
echo "[INFO] TASKNUM=$TASKNUM  INIT_CYC=$INIT_CYC  KEEP_DIR=$KEEP_DIR"
echo "[INFO] statesimul : $SIMUL"
echo "[INFO] Output root: $BASE_DIR"

for U in $UTIL_LIST; do
    U_DIR="$BASE_DIR/u_${U}"
    U_TGZ="$BASE_DIR/u_${U}.tar.gz"
    echo "[INFO] ================== U=${U}  (cwd: $U_DIR) =================="

    # Fresh output slot per U — nuke any stale run so a partial previous
    # attempt doesn't get bundled into the new tar.
    rm -rf "$U_DIR"
    mkdir -p "$U_DIR"
    rm -f "$U_TGZ"

    # Run everything for this U in a subshell so cd doesn't leak between iters.
    (
        cd "$U_DIR"

        echo "[INFO] U=$U TASKGEN..."
        "$SIMUL" NO NO NO TASKGEN "$TASKNUM" "$U" -1 0.05 0.95 0 \
            > taskgen.log 2> taskgen.err

        echo "[INFO] U=$U WORKGEN..."
        "$SIMUL" NO NO NO WORKGEN "$TASKNUM" "$U" -1 0.05 0.95 0 \
            > workgen.log 2> workgen.err

        # argv layout (per run_simul.sh):
        #   wflag gcflag rrflag gen/nogen TASKNUM U -1 0.05 0.95 0 INIT_CYC [LAT_MODE]
        # 4 LaWL variants share (UTILGC INVW RR005 nogen); argv[12] selects the
        # latency lens AND drives the output filename prefix. Baseline uses
        # (NO NO SKIPRR) — no LaWL controller.
        declare -A pids
        run_sim "${CORE_ARR[0]}" UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 $INIT_CYC \
            > LaWL.log 2> LaWL.err &
        pids[LaWL]=$!
        run_sim "${CORE_ARR[1]}" UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_OPT \
            > LaWL_opt.log 2> LaWL_opt.err &
        pids[LaWL_opt]=$!
        run_sim "${CORE_ARR[2]}" UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_AVG \
            > LaWL_avg.log 2> LaWL_avg.err &
        pids[LaWL_avg]=$!
        run_sim "${CORE_ARR[3]}" UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 $INIT_CYC LAWL_PES \
            > LaWL_pes.log 2> LaWL_pes.err &
        pids[LaWL_pes]=$!
        run_sim "${CORE_ARR[4]}" NO NO SKIPRR nogen "$TASKNUM" "$U" -1 0.05 0.95 0 $INIT_CYC \
            > baseline.log 2> baseline.err &
        pids[baseline]=$!

        echo "[INFO] U=$U launched PIDs:"
        for name in "${!pids[@]}"; do
            printf "  %-10s pid=%s\n" "$name" "${pids[$name]}"
        done

        # Wait each individually so we know which one failed.
        set +e
        declare -A results
        for name in "${!pids[@]}"; do
            wait "${pids[$name]}"
            results[$name]=$?
        done
        set -e

        # Exit code 1 is the simulator's NORMAL end-of-life signal (MAXPE hit).
        # Only treat other codes (segfault 139, sigkill 137, exit 2 = FATAL
        # log-open mismatch, etc.) as failure.
        fail=0
        for name in "${!results[@]}"; do
            rc=${results[$name]}
            if [ "$rc" -eq 0 ] || [ "$rc" -eq 1 ]; then
                echo "[OK]    U=$U $name  exit=$rc"
            else
                echo "[FAIL]  U=$U $name  exit=$rc  (see $U_DIR/$name.err)"
                fail=1
            fi
        done
        exit $fail
    )
    U_RC=$?

    if [ "$U_RC" -ne 0 ]; then
        echo "[ABORT] U=$U had failures — leaving $U_DIR uncompressed for inspection"
        exit 1
    fi

    # Compress the U slice, then optionally drop the raw dir to reclaim disk.
    # -C into BASE_DIR + name-relative arg so paths inside the tar are
    # 'u_${U}/…' rather than the full absolute prefix.
    echo "[INFO] U=$U archiving -> $U_TGZ"
    tar -czf "$U_TGZ" -C "$BASE_DIR" "u_${U}"
    if [ "$KEEP_DIR" = "1" ]; then
        echo "[INFO] U=$U keeping $U_DIR (KEEP_DIR=1)"
    else
        rm -rf "$U_DIR"
        echo "[INFO] U=$U removed uncompressed dir"
    fi
done

echo "[DONE] Completed variant sweep over U=[$UTIL_LIST]"
echo "[DONE] Archives in $BASE_DIR (u_*.tar.gz)"
