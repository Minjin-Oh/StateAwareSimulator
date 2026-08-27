#!/bin/bash
# Parallel sweep of nine schemes including the RTGC comparison scheme.
# Companion to run_simul.sh; keeps it untouched so pre-existing RTSS
# experiments continue to work unchanged.
#
# Usage: ./run_simul_rtgc.sh BASE_DIR CORE_LIST
#   BASE_DIR   -- directory produced by gen_simul.sh (auto-extracts
#                 $BASE_DIR/workloads.tar.gz if only the tarball is left).
#   CORE_LIST  -- comma-separated core ids, exactly NINE entries.
#                 The i-th core is bound to the i-th scheme below.
#
# CAUTION: This script and run_simul.sh both write to the shared
# {baseline,dynamic,static,hybrid,LaWL}/*_lifetime.csv files.  Running
# both scripts against the same BASE_DIR will DOUBLE-COUNT the shared
# schemes.  Use a fresh BASE_DIR when combining RTGC runs with the
# original RTSS sweep.
#
# Layout produced (same convention as run_simul.sh, plus the four RTGC
# variants and the RTGC-only per-run logs):
#   $BASE_DIR/util_<U>/<scheme_dir>/<prefix>_rrchecker_<NNN>.csv
#   $BASE_DIR/util_<U>/<scheme_dir>/<prefix>_updaterate_<NNN>.csv
#   $BASE_DIR/util_<U>/<scheme_dir>/<prefix>_lifetime.csv
#   $BASE_DIR/util_<U>/<scheme_dir>/<prefix>_overhead.csv
#   $BASE_DIR/util_<U>/<RTGC_*>/<prefix>_token_<NNN>.csv
#   $BASE_DIR/util_<U>/<RTGC_*>/<prefix>_wl_<NNN>.csv
#   $BASE_DIR/util_<U>/<RTGC_*>/<prefix>_admission_<NNN>.csv
#
# The RTGC command line requires argv[11] (INITCYC) — we pass "0" (or
# whatever aligns with the rest of the sweep) to keep the position of
# argv[12] (RTGC latency model) stable.

set -u

if [ $# -ne 2 ]; then
    cat >&2 <<USAGE
usage: $0 BASE_DIR CORE_LIST

  BASE_DIR   directory produced by gen_simul.sh
  CORE_LIST  comma-separated core ids, exactly 9 entries
             (one core per scheme; parallel execution)

example: $0 ./results_rtgc "9,10,11,12,13,14,15,16,17"
USAGE
    exit 2
fi

BASE_DIR="$1"
CORE_LIST="$2"
BIN="$(readlink -f ./statesimul.out)"

UTILS=(0.1 0.15 0.2 0.25 0.3)

# Scheme table.  Index i selects (dir, prefix, args) and CORES[i].
# The nine schemes below must match §4.8 of the spec exactly and produce
# log-file prefixes the emul_main.c file-open branches already emit.
SCHEME_DIRS=(baseline dynamic static hybrid RTGC-START RTGC-END RTGC-PECMAX RTGC-PECAVG LaWL)
SCHEME_PREFIX=(Baseline Dynamic Static Hyb RTGC_START RTGC_END RTGC_PECMAX RTGC_PECAVG LaWL)
# argv[1..N] for each scheme.  Note: RTGC needs argv[11]=INITCYC
# (present in argv position 11) and argv[12]=latency model.
SCHEME_ARGS=(
    "NO     NO        SKIPRR nogen 4 __UTIL__ -1 0.05 0.95 0"               # baseline
    "NO     MOTIVALLY SKIPRR nogen 4 __UTIL__ -1 0.05 0.95 0"               # dynamic
    "NO     NO        BASE005 nogen 4 __UTIL__ -1 0.05 0.95 0"              # static
    "NO     MOTIVALLY BASE005 nogen 4 __UTIL__ -1 0.05 0.95 0"              # hybrid
    "RTGC   NO        NRTWL   nogen 4 __UTIL__ -1 0.05 0.95 0 0 START"      # RTGC-START
    "RTGC   NO        NRTWL   nogen 4 __UTIL__ -1 0.05 0.95 0 0 END"        # RTGC-END
    "RTGC   NO        NRTWL   nogen 4 __UTIL__ -1 0.05 0.95 0 0 PEC_MAX"    # RTGC-PECMAX
    "RTGC   NO        NRTWL   nogen 4 __UTIL__ -1 0.05 0.95 0 0 PEC_AVG"    # RTGC-PECAVG
    "UTILGC INVW      RR005   nogen 4 __UTIL__ -1 0.05 0.95 0"              # LaWL
)
# writerank.csv is only meaningful for wflag==14 (LaWL family).  Linking
# it into RTGC / baseline / dynamic / static / hybrid work-dirs is
# harmless (init.c open()s it with an NULL check) but we skip it for
# RTGC to keep the reject signal clean and match §4.8.
SCHEME_NEED_WRITERANK=(0 0 0 0 0 0 0 0 1)

NUM_SCHEMES=${#SCHEME_DIRS[@]}

# ---- input validation -------------------------------------------------
if [ ! -x "$BIN" ]; then
    echo "error: $BIN not found or not executable" >&2
    exit 1
fi

IFS=',' read -r -a CORES <<< "$CORE_LIST"
if [ ${#CORES[@]} -ne "$NUM_SCHEMES" ]; then
    echo "error: CORE_LIST must contain exactly $NUM_SCHEMES core ids (got ${#CORES[@]})" >&2
    exit 1
fi
NCPU=$(nproc)
for core in "${CORES[@]}"; do
    if ! [[ "$core" =~ ^[0-9]+$ ]] || [ "$core" -ge "$NCPU" ]; then
        echo "error: invalid core id '$core' (nproc=$NCPU)" >&2
        exit 1
    fi
done

# ---- workload extraction ---------------------------------------------
WORKLOADS_DIR="$BASE_DIR/workloads"
if [ ! -d "$WORKLOADS_DIR" ] && [ -f "$BASE_DIR/workloads.tar.gz" ]; then
    echo "[INFO] extracting $BASE_DIR/workloads.tar.gz"
    tar xzf "$BASE_DIR/workloads.tar.gz" -C "$BASE_DIR"
fi
if [ ! -d "$WORKLOADS_DIR" ]; then
    echo "error: $WORKLOADS_DIR not found. Run gen_simul.sh first." >&2
    exit 1
fi

WRITERANK_ABS="$(readlink -f ./writerank.csv 2>/dev/null || true)"
overall_rc=0

# ---- per-utilization / per-iteration loop -----------------------------
for U in "${UTILS[@]}"; do
    UTIL_SRC="$WORKLOADS_DIR/util_$U"
    UTIL_BASE="$BASE_DIR/util_$U"
    if [ ! -d "$UTIL_SRC" ]; then
        echo "  warn: no workloads for util=$U at $UTIL_SRC; skipping" >&2
        continue
    fi
    for d in "${SCHEME_DIRS[@]}"; do
        mkdir -p "$UTIL_BASE/$d"
    done
    echo "########## utilization = $U   (-> $UTIL_BASE) ##########"

    for GENDIR in "$UTIL_SRC"/*/; do
        GENDIR="${GENDIR%/}"
        [ -f "$GENDIR/.done" ] || continue
        IDX=$(basename "$GENDIR")
        echo "=== [util=$U] iteration $IDX  ($(date '+%F %T')) ==="

        # Build one work-dir per scheme and spawn all schemes in parallel.
        pids=()
        for i in $(seq 0 $((NUM_SCHEMES-1))); do
            local_wd="$UTIL_BASE/${SCHEME_DIRS[$i]}/_run_$IDX"
            mkdir -p "$local_wd"
            for f in "$GENDIR"/taskparam.csv "$GENDIR"/cyc.csv "$GENDIR"/loc.csv \
                     "$GENDIR"/wr_t*.csv     "$GENDIR"/rd_t*.csv; do
                [ -e "$f" ] || continue
                ln -sf "$(readlink -f "$f")" "$local_wd/$(basename "$f")"
            done
            if [ "${SCHEME_NEED_WRITERANK[$i]}" = "1" ] && \
               [ -n "$WRITERANK_ABS" ] && [ -e "$WRITERANK_ABS" ]; then
                ln -sf "$WRITERANK_ABS" "$local_wd/writerank.csv"
            fi

            args_expanded="${SCHEME_ARGS[$i]//__UTIL__/$U}"
            (
                cd "$local_wd" && \
                taskset -c "${CORES[$i]}" "$BIN" $args_expanded \
                    > /dev/null 2> run.err
            ) &
            pids+=($!)
        done

        rc=0
        for pid in "${pids[@]}"; do
            wait "$pid" || rc=$?
        done
        if [ "$rc" -ne 0 ]; then
            echo "  warn: at least one workload exited non-zero in util=$U iter $IDX" >&2
            overall_rc=$rc
        fi

        # Collect per-run and cumulative logs.  Extends run_simul.sh's
        # collect() with three RTGC-only per-run files (token/wl/admission)
        # that only exist for gcflag==8 runs.
        collect() {
            local scheme_dir="$1"
            local prefix="$2"
            local wd="$UTIL_BASE/$scheme_dir/_run_$IDX"
            local dest="$UTIL_BASE/$scheme_dir"

            # per-iteration files (rename with iteration suffix)
            for kind in rrchecker updaterate token wl admission; do
                local src="$wd/${prefix}_${kind}.csv"
                [ -f "$src" ] && mv "$src" "$dest/${prefix}_${kind}_${IDX}.csv"
            done
            # append-to-shared files
            for kind in lifetime overhead; do
                local src="$wd/${prefix}_${kind}.csv"
                [ -f "$src" ] && cat "$src" >> "$dest/${prefix}_${kind}.csv"
            done
            if [ -s "$wd/run.err" ]; then
                mv "$wd/run.err" "$dest/run_${IDX}.err"
            fi
            rm -rf "$wd"
        }

        for i in $(seq 0 $((NUM_SCHEMES-1))); do
            collect "${SCHEME_DIRS[$i]}" "${SCHEME_PREFIX[$i]}"
        done
    done
done

# Compress workloads on clean run so re-invocations don't waste disk.
if [ "$overall_rc" -eq 0 ]; then
    echo "[INFO] compressing workloads -> $BASE_DIR/workloads.tar.gz"
    tar czf "$BASE_DIR/workloads.tar.gz" -C "$BASE_DIR" workloads && rm -rf "$WORKLOADS_DIR"
else
    echo "[WARN] some runs failed; leaving $WORKLOADS_DIR uncompressed for debugging" >&2
fi

echo "Done. Results in $BASE_DIR"
