#!/bin/bash
# Sweep initial (read+write) utilization values and repeat the 7-workload
# parallel experiment NUM_RUNS times for each utilization.
#
# Usage: ./run_repeat.sh [NUM_RUNS] [BASE_DIR]
#   NUM_RUNS defaults to 100 (per utilization value)
#   BASE_DIR defaults to ./results_<timestamp>
#
# Output layout:
#   $BASE_DIR/util_<U>/<workload>/<prefix>_rrchecker_<NNN>.csv   (per iter)
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

# name : core : arg1 : arg2 : arg3 : rrchecker/lifetime/overhead prefix (from emul_main.c)
WORKLOADS=(
    "Baseline:9:NO:NO:SKIPRR:Baseline"
    "Dynamic:10:NO:MOTIVALLY:SKIPRR:Dynamic"
    "Static:11:NO:NO:BASE005:Static"
    "Hybrid:12:NO:MOTIVALLY:BASE005:Hyb"
    "wonly:13:NO:INVW:SKIPRR:wonly"
    "LaWL_D:14:UTILGC:INVW:SKIPRR:LaWL_D"
    "LaWL:15:UTILGC:INVW:RR005:LaWL"
)

mkdir -p "$BASE_DIR"

for U in "${UTILS[@]}"; do
    UTIL_BASE="$BASE_DIR/util_$U"
    echo "########## utilization = $U   (-> $UTIL_BASE) ##########"

    for wl in "${WORKLOADS[@]}"; do
        IFS=: read -r name _ _ _ _ _ <<< "$wl"
        mkdir -p "$UTIL_BASE/$name"
    done

    # arg bundles rebuilt per utilization
    TASKGEN_ARGS=(NO NO NO TASKGEN 4 "$U" -1 0.05 0.95 0)
    WORKGEN_ARGS=(NO NO NO WORKGEN 4 "$U" -1 0.05 0.95 0)
    COMMON_ARGS=(nogen 4 "$U" -1 0.05 0.95 0)

    for i in $(seq 1 "$NUM_RUNS"); do
        IDX=$(printf "%03d" "$i")
        echo "=== [util=$U] iteration $IDX / $NUM_RUNS  ($(date '+%F %T')) ==="

        # 1) generate a fresh workload for this iteration (the 7 experiments share it)
        GENDIR="$UTIL_BASE/_gen_$IDX"
        mkdir -p "$GENDIR"
        if ! (
            cd "$GENDIR" && \
            "$BIN" "${TASKGEN_ARGS[@]}" > gen.log  2> gen.err && \
            "$BIN" "${WORKGEN_ARGS[@]}" >> gen.log 2>> gen.err
        ); then
            echo "  error: workload generation failed in util=$U iter $IDX; skipping" >&2
            rm -rf "$GENDIR"
            continue
        fi

        # 2) launch the 7 experiments in parallel, each in its own dir
        pids=()
        workdirs=()
        names=()
        prefixes=()

        for wl in "${WORKLOADS[@]}"; do
            IFS=: read -r name core a1 a2 a3 prefix <<< "$wl"
            WORKDIR="$UTIL_BASE/$name/_run_$IDX"
            mkdir -p "$WORKDIR"

            # symlink generator outputs so the nogen experiment can read them from CWD
            for f in "$GENDIR"/taskparam.csv "$GENDIR"/cyc.csv "$GENDIR"/loc.csv "$GENDIR"/wr_t*.csv "$GENDIR"/rd_t*.csv; do
                [ -e "$f" ] || continue
                ln -sf "$(readlink -f "$f")" "$WORKDIR/$(basename "$f")"
            done

            # run each workload in its own directory so per-workload output files
            # (including *_rrchecker.csv) never collide with sibling workloads.
            (
                cd "$WORKDIR" && \
                taskset -c "$core" "$BIN" "$a1" "$a2" "$a3" "${COMMON_ARGS[@]}" \
                    > /dev/null 2> run.err
            ) &

            pids+=("$!")
            workdirs+=("$WORKDIR")
            names+=("$name")
            prefixes+=("$prefix")
        done

        # wait for all 7 to finish
        rc=0
        for pid in "${pids[@]}"; do
            wait "$pid" || rc=$?
        done
        if [ "$rc" -ne 0 ]; then
            echo "  warn: at least one workload exited non-zero in util=$U iter $IDX" >&2
        fi

        # collect per-workload logs
        for j in "${!names[@]}"; do
            name="${names[$j]}"
            prefix="${prefixes[$j]}"
            WORKDIR="${workdirs[$j]}"
            DEST="$UTIL_BASE/$name"

	    # rrchecker & updaterate: preserve per-iteration snapshot with numeric suffix
            for kind in rrchecker updaterate; do
                src="$WORKDIR/${prefix}_${kind}.csv"
                if [ -f "$src" ]; then
                    mv "$src" "$DEST/${prefix}_${kind}_${IDX}.csv"
                fi
            done

            # lifetime & overhead: append this iteration's content to the shared file
            for kind in lifetime overhead; do
                src="$WORKDIR/${prefix}_${kind}.csv"
                if [ -f "$src" ]; then
                    cat "$src" >> "$DEST/${prefix}_${kind}.csv"
                fi
            done

            # keep stderr if non-empty (helpful for debugging)
            if [ -s "$WORKDIR/run.err" ]; then
                mv "$WORKDIR/run.err" "$DEST/run_${IDX}.err"
            fi

            # drop the temp workdir (also removes updaterate / rr_prof / updateorder, etc.)
            rm -rf "$WORKDIR"
        done

        # drop the per-iteration generated workload
        rm -rf "$GENDIR"
    done
done

echo "Done. Results in $BASE_DIR"

