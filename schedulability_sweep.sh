#!/usr/bin/env bash
# schedulability_sweep.sh
#
# Fig.2a-style schedulability ratio sweep across five FTL/GC/WL schemes.
#
# For each (U_r, seed) tuple:
#   1. generate a taskset (TASKGEN) and workload (WORKGEN) into a private
#      working directory so the five schemes share the same trace;
#   2. run each scheme, capture its process exit code, and append one row
#      to the master CSV.
#
# Exit code -> classification (see types.h):
#   0  = EXIT_SUCCESS_RUNTIME  -> schedulable (finished RUNTIME cleanly)
#   1  = EXIT_UTIL_OVERFLOW    -> UNSCHEDULABLE
#   2  = EXIT_MAXPE            -> schedulable (endurance limit; user policy)
#   3  = EXIT_DL_MISS          -> UNSCHEDULABLE
#   10 = EXIT_TASKGEN_FAIL     -> skip (no feasible taskset at this U_r)
#   124 = /usr/bin/timeout     -> schedulable-so-far (survived RUN_TIMEOUT)
#
# Usage:
#   ./schedulability_sweep.sh                    # defaults below
#   NUM_SEEDS=100 STEP=0.025 ./schedulability_sweep.sh
#
# Result:
#   schedulability.csv        -- one row per (util, seed, scheme)
#   runs/U{util}_S{seed}/     -- per-tuple working dirs (kept for debugging)

set -u
set -o pipefail

REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BIN="${BIN:-$REPO/statesimul.out}"

# ---- knobs ------------------------------------------------------------------
TASKNUM=${TASKNUM:-8}          # number of RT tasks per set (paper uses 8)
NUM_SEEDS=${NUM_SEEDS:-50}     # task sets per util point
U_MIN=${U_MIN:-0.10}
U_MAX=${U_MAX:-0.95}
STEP=${STEP:-0.05}
GEN_TIMEOUT=${GEN_TIMEOUT:-60}   # sec, per TASKGEN / WORKGEN call
RUN_TIMEOUT=${RUN_TIMEOUT:-600}  # sec, per scheme simulation (10 min)
S_LOC=${S_LOC:-0.05}
T_LOC=${T_LOC:-0.95}
SKEW=${SKEW:--1}                 # -1 == no skew (UUniFast)

# ---- schemes (label -> flag triplet) ---------------------------------------
SCHEME_NAMES=(Baseline WAOGC DynWL HybWL LaWL)
SCHEME_FLAGS=(
  "NO        NO         SKIPRR"     # Baseline
  "WAOGC     NO         SKIPRR"     # WAO-GC (Zhang'15)
  "NO        MOTIVALLY  SKIPRR"     # Dynamic WL only
  "NO        MOTIVALLY  BASE005"    # Hybrid WL (Dyn + static WL)
  "UTILGC    INVW       RR005"      # LaWL
)

# ---- outputs ---------------------------------------------------------------
RESULT_DIR="$REPO/sweep_results"
mkdir -p "$RESULT_DIR"
RESULT_CSV="$RESULT_DIR/schedulability.csv"
LOG_DIR="$RESULT_DIR/logs"
mkdir -p "$LOG_DIR"

if [ ! -f "$RESULT_CSV" ]; then
    echo "util,seed,scheme,exit_code,cur_cp_at_exit" > "$RESULT_CSV"
fi

echo "[sweep] REPO=$REPO"
echo "[sweep] BIN=$BIN"
echo "[sweep] util in [$U_MIN, $U_MAX] step $STEP, seeds=$NUM_SEEDS, tasknum=$TASKNUM"
echo "[sweep] gen_timeout=${GEN_TIMEOUT}s, run_timeout=${RUN_TIMEOUT}s"
echo "[sweep] schemes: ${SCHEME_NAMES[*]}"

# floating-point util iteration via awk
UTILS=$(awk -v a=$U_MIN -v b=$U_MAX -v s=$STEP \
    'BEGIN{ for(u=a; u<=b+1e-9; u+=s) printf("%.4f ",u); }')

total_started=0
total_done=0
sweep_t0=$(date +%s)

for U in $UTILS; do
    U_TAG=$(printf '%.4f' "$U")
    for S in $(seq 1 "$NUM_SEEDS"); do
        S_TAG=$(printf '%03d' "$S")
        WD="$REPO/runs/U${U_TAG}_S${S_TAG}"
        mkdir -p "$WD"

        # skip this tuple entirely if it already has all 5 rows in the master
        # (idempotent resume). uses awk to count rows.
        existing=$(awk -F',' -v u="$U_TAG" -v s="$S" \
            'NR>1 && $1==u && $2==s { c++ } END{ print c+0 }' "$RESULT_CSV")
        if [ "$existing" -ge "${#SCHEME_NAMES[@]}" ]; then
            continue
        fi

        total_started=$((total_started+1))
        pushd "$WD" > /dev/null

        # 1) taskset generation (unless already there)
        if [ ! -f taskparam.csv ]; then
            timeout "$GEN_TIMEOUT" "$BIN" NO NO NO TASKGEN \
                "$TASKNUM" "$U" "$SKEW" -2.0 -2.0 0 \
                > "$LOG_DIR/gen_task_U${U_TAG}_S${S_TAG}.log" 2>&1
            EC=$?
            if [ $EC -ne 0 ]; then
                echo "$U_TAG,$S,GEN_TASK_FAIL,$EC," >> "$RESULT_CSV"
                popd > /dev/null
                continue
            fi
        fi

        # 2) workload generation (wN.csv / rN.csv)
        if [ ! -f w0.csv ]; then
            timeout "$GEN_TIMEOUT" "$BIN" NO NO NO WORKGEN \
                "$TASKNUM" "$U" "$SKEW" "$S_LOC" "$T_LOC" 0 \
                > "$LOG_DIR/gen_wl_U${U_TAG}_S${S_TAG}.log" 2>&1
            if [ $? -ne 0 ]; then
                echo "$U_TAG,$S,GEN_WL_FAIL,$?," >> "$RESULT_CSV"
                popd > /dev/null
                continue
            fi
        fi

        # 3) run each scheme
        for i in "${!SCHEME_NAMES[@]}"; do
            NAME=${SCHEME_NAMES[$i]}
            FLAGS=${SCHEME_FLAGS[$i]}

            # skip if this (util, seed, scheme) is already recorded
            already=$(awk -F',' -v u="$U_TAG" -v s="$S" -v n="$NAME" \
                'NR>1 && $1==u && $2==s && $3==n { c++ } END{ print c+0 }' \
                "$RESULT_CSV")
            if [ "$already" -ge 1 ]; then continue; fi

            timeout "$RUN_TIMEOUT" "$BIN" $FLAGS nogen \
                "$TASKNUM" "$U" "$SKEW" "$S_LOC" "$T_LOC" 0 \
                > "$LOG_DIR/run_${NAME}_U${U_TAG}_S${S_TAG}.log" 2>&1
            EC=$?

            # last cur_cp is the trailing "N," in <NAME>_lifetime.csv
            LT="${NAME}_lifetime.csv"
            if [ -f "$LT" ]; then
                CP=$(tail -c 128 "$LT" | tr ',' '\n' | grep -E '^[0-9]+$' \
                     | tail -1)
            else
                CP=""
            fi

            echo "$U_TAG,$S,$NAME,$EC,$CP" >> "$RESULT_CSV"
        done

        popd > /dev/null
        total_done=$((total_done+1))

        # heartbeat every 20 tuples
        if [ $((total_done % 20)) -eq 0 ]; then
            now=$(date +%s)
            elapsed=$((now - sweep_t0))
            echo "[sweep] $total_done tuples done in ${elapsed}s (last: U=$U_TAG S=$S)"
        fi
    done
done

echo "[sweep] finished: $total_done tuples completed"
echo "[sweep] master CSV: $RESULT_CSV"
