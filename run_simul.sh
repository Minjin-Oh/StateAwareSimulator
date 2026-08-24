#!/bin/bash
# Sweep initial (read+write) utilization values and repeat the 7-workload
# parallel experiment NUM_RUNS times for each utilization.
#
# Usage: ./run_simul.sh [NUM_RUNS] [BASE_DIR]
#   NUM_RUNS defaults to 100 (per utilization value)
#   BASE_DIR defaults to ./results_<timestamp>
#
# Output layout:
#   $BASE_DIR/util_<U>/<workload>/<prefix>_rrchecker_<NNN>.csv   (per iter)
#   $BASE_DIR/util_<U>/<workload>/<prefix>_updaterate_<NNN>.csv  (per iter)
#   $BASE_DIR/util_<U>/<workload>/<prefix>_lifetime.csv          (appended)
#   $BASE_DIR/util_<U>/<workload>/<prefix>_overhead.csv          (appended)

set -u

NUM_RUNS="${1:-100}"
BASE_DIR="${2:-./results_$(date +%Y%m%d_%H%M%S)}"
BIN="$(readlink -f ./statesimul.out)"

# initial read+write utilization values to sweep
UTILS=(0.1 0.15 0.2 0.25 0.3)

if [ ! -x "$BIN" ]; then
    echo "error: $BIN not found or not executable" >&2
    exit 1
fi

mkdir -p "$BASE_DIR"

for U in "${UTILS[@]}"; do
    UTIL_BASE="$BASE_DIR/util_$U"
    mkdir -p "$UTIL_BASE"/{baseline,dynamic,static,hybrid,wonly,LaWL-D,LaWL}
    echo "########## utilization = $U   (-> $UTIL_BASE) ##########"

    for i in $(seq 1 "$NUM_RUNS"); do
        IDX=$(printf "%03d" "$i")
        echo "=== [util=$U] iteration $IDX / $NUM_RUNS  ($(date '+%F %T')) ==="

        # 1) generate a fresh task set + workload for this iteration
        GENDIR="$UTIL_BASE/_gen_$IDX"
        mkdir -p "$GENDIR"
        if ! (
            cd "$GENDIR" && \
            "$BIN" NO NO NO TASKGEN 4 "$U" -1 0.05 0.95 0 > gen.log  2> gen.err && \
            "$BIN" NO NO NO WORKGEN 4 "$U" -1 0.05 0.95 0 >> gen.log 2>> gen.err
        ); then
            echo "  error: workload generation failed in util=$U iter $IDX; skipping" >&2
            rm -rf "$GENDIR"
            continue
        fi

        # 2) prepare per-workload workdirs (symlink shared generator outputs)
        WRITERANK_ABS="$(readlink -f ./writerank.csv 2>/dev/null || true)"
        make_workdir() {
            local wd="$UTIL_BASE/$1/_run_$IDX"
            mkdir -p "$wd"
            for f in "$GENDIR"/taskparam.csv "$GENDIR"/cyc.csv "$GENDIR"/loc.csv \
                     "$GENDIR"/wr_t*.csv     "$GENDIR"/rd_t*.csv; do
                [ -e "$f" ] || continue
                ln -sf "$(readlink -f "$f")" "$wd/$(basename "$f")"
            done
            # writerank.csv lives in the project root and is required by INVW workloads
            # (init.c::init_metadata reads it to allocate metadata->rank_bounds).
            if [ -n "$WRITERANK_ABS" ] && [ -e "$WRITERANK_ABS" ]; then
                ln -sf "$WRITERANK_ABS" "$wd/writerank.csv"
            fi
            echo "$wd"
        }
	WD_BASELINE=$(make_workdir baseline)
        WD_DYNAMIC=$(make_workdir  dynamic)
        WD_STATIC=$(make_workdir   static)
        WD_HYBRID=$(make_workdir   hybrid)
        WD_WONLY=$(make_workdir    wonly)
        WD_LAWLD=$(make_workdir    LaWL-D)
        WD_LAWL=$(make_workdir     LaWL)

        # 3) launch the 7 workloads in parallel, each in its own dir
        ( cd "$WD_BASELINE" && taskset -c 9  "$BIN" NO     NO        SKIPRR  nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid1=$!
        ( cd "$WD_DYNAMIC"  && taskset -c 10 "$BIN" NO     MOTIVALLY SKIPRR  nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid2=$!
        ( cd "$WD_STATIC"   && taskset -c 11 "$BIN" NO     NO        BASE005 nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid3=$!
        ( cd "$WD_HYBRID"   && taskset -c 12 "$BIN" NO     MOTIVALLY BASE005 nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid4=$!
        ( cd "$WD_WONLY"    && taskset -c 13 "$BIN" NO     INVW      SKIPRR  nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid5=$!
        ( cd "$WD_LAWLD"    && taskset -c 14 "$BIN" UTILGC INVW      SKIPRR  nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid6=$!
        ( cd "$WD_LAWL"     && taskset -c 15 "$BIN" UTILGC INVW      RR005   nogen 4 "$U" -1 0.05 0.95 0 > /dev/null 2> run.err ) &
        pid7=$!

        rc=0
        for pid in $pid1 $pid2 $pid3 $pid4 $pid5 $pid6 $pid7; do
            wait "$pid" || rc=$?
        done
        if [ "$rc" -ne 0 ]; then
            echo "  warn: at least one workload exited non-zero in util=$U iter $IDX" >&2
        fi

        # 4) collect per-workload logs
        collect() {
            local name="$1" prefix="$2" wd="$3"
            local dest="$UTIL_BASE/$name"

            # rrchecker & updaterate: keep per-iteration snapshot with numeric suffix
            for kind in rrchecker updaterate; do
                local src="$wd/${prefix}_${kind}.csv"
                [ -f "$src" ] && mv "$src" "$dest/${prefix}_${kind}_${IDX}.csv"
            done
            # lifetime & overhead: append to shared per-workload csv
            for kind in lifetime overhead; do
                local src="$wd/${prefix}_${kind}.csv"
                [ -f "$src" ] && cat "$src" >> "$dest/${prefix}_${kind}.csv"
            done
            if [ -s "$wd/run.err" ]; then
                mv "$wd/run.err" "$dest/run_${IDX}.err"
            fi
            rm -rf "$wd"
        }

        collect baseline Baseline "$WD_BASELINE"
        collect dynamic  Dynamic  "$WD_DYNAMIC"
        collect static   Static   "$WD_STATIC"
        collect hybrid   Hyb      "$WD_HYBRID"
        collect wonly    wonly    "$WD_WONLY"
        collect LaWL-D   LaWL_D   "$WD_LAWLD"
        collect LaWL     LaWL     "$WD_LAWL"

        # drop the per-iteration generated workload
        rm -rf "$GENDIR"
    done
done

echo "Done. Results in $BASE_DIR"

