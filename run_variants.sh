#!/bin/bash
#
# For each U in UTIL_LIST:
#   For each U${U}_S${SP}.tar.gz in TASKSET_DIR (sequential):
#     Extract inputs into results/u_${U}/, run 5 variants in parallel
#     (each pinned to its own core), remove the extracted inputs so
#     u_${U}/ retains only sim outputs.
#   After all seeds for this U are done, tar.gz u_${U}/ into
#   results_u_${U}.tar.gz and drop the working dir.
#
# Two kinds of output collect in u_${U}/:
#   - Appended CSVs (*_lifetime.csv, *_overhead.csv, *_admit_summary.csv,
#     *_jobs_summary.csv) — one row per seed, accumulates across 100 seeds
#   - Per-seed gz traces suffixed via SIM_ITER=$SP
#     (*_utilization_${SP}.csv.gz, *_admit_${SP}.csv.gz, *_jobs_${SP}.csv.gz)
#
# Usage:
#   ./run_variants.sh                       # cores 0..4
#   ./run_variants.sh C1 C2 C3 C4 C5        # 5 cores, one per variant
#
# Env: TASKSET_DIR RESULT_DIR UTIL_LIST TASKNUM INIT_CYC SIM

set -u

TASKSET_DIR=${TASKSET_DIR:-./tasksets}
RESULT_DIR=${RESULT_DIR:-./results}
UTIL_LIST=${UTIL_LIST:-"0.1 0.15 0.2 0.25 0.3"}
TASKNUM=${TASKNUM:-4}
INIT_CYC=${INIT_CYC:-1500}
SIM=${SIM:-./statesimul.out}

if [ $# -ge 5 ]; then
    CORES=("$1" "$2" "$3" "$4" "$5")
else
    CORES=(0 1 2 3 4)
fi

# Resolve to absolute paths — the subshell below does `cd "$RESULT"`, which
# would break relative refs like "./statesimul.out" and "./tasksets".
case "$SIM"         in /*) ;; *) SIM="$PWD/${SIM#./}"                 ;; esac
case "$TASKSET_DIR" in /*) ;; *) TASKSET_DIR="$PWD/${TASKSET_DIR#./}" ;; esac
case "$RESULT_DIR"  in /*) ;; *) RESULT_DIR="$PWD/${RESULT_DIR#./}"   ;; esac

[ -x "$SIM" ] || { echo "[FATAL] $SIM not executable"; exit 1; }
[ -d "$TASKSET_DIR" ] || { echo "[FATAL] $TASKSET_DIR not found"; exit 1; }
mkdir -p "$RESULT_DIR"

trap 'kill $(jobs -pr) 2>/dev/null; exit 130' INT TERM

run_variant() {
    local core="$1" tag="$2" SP="$3"; shift 3
    taskset -c "$core" "$SIM" "$@" > "${tag}_${SP}.log" 2> "${tag}_${SP}.err"
}

for U in $UTIL_LIST; do
    RESULT_TGZ="$RESULT_DIR/results_u_${U}.tar.gz"
    [ -f "$RESULT_TGZ" ] && { echo "[SKIP] results_u_${U}.tar.gz exists"; continue; }

    RESULT="$RESULT_DIR/u_${U}"
    mkdir -p "$RESULT"

    for tgz in "$TASKSET_DIR"/U${U}_S*.tar.gz; do
        [ -f "$tgz" ] || continue
        BASE=$(basename "$tgz" .tar.gz)   # U0.1_S001
        SP=${BASE##*_S}                    # 001
        S=$((10#$SP))                      # 1
        echo "[INFO] ${BASE}"

        # Extract inputs directly into RESULT (strip the U${U}_S${SP}/ prefix
        # that gen_tasksets.sh's tar preserves).
        tar -xzf "$tgz" -C "$RESULT" --strip-components=1

        (
            cd "$RESULT" || exit 1
            export SIM_ITER="$SP"
            # argv: wflag gcflag rrflag nogen TASKNUM U -1 0.05 0.95 0 INIT_CYC LAT_MODE SEED
            run_variant "${CORES[0]}" baseline "$SP" \
                NO NO SKIPRR nogen "$TASKNUM" "$U" -1 0.05 0.95 0 "$INIT_CYC" STATE "$S" &
            run_variant "${CORES[1]}" LaWL "$SP" \
                UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 "$INIT_CYC" STATE "$S" &
            run_variant "${CORES[2]}" LaWL_opt "$SP" \
                UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 "$INIT_CYC" LAWL_OPT "$S" &
            run_variant "${CORES[3]}" LaWL_avg "$SP" \
                UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 "$INIT_CYC" LAWL_AVG "$S" &
            run_variant "${CORES[4]}" LaWL_pes "$SP" \
                UTILGC INVW RR005 nogen "$TASKNUM" "$U" -1 0.05 0.95 0 "$INIT_CYC" LAWL_PES "$S" &
            wait
        )

        # Drop the staged inputs so RESULT stays lean between seeds.
        rm -f "$RESULT"/taskparam.csv "$RESULT"/wr_t*.csv "$RESULT"/rd_t*.csv \
              "$RESULT"/loc.csv "$RESULT"/cyc.csv
    done

    echo "[INFO] archiving results_u_${U}"
    tar -czf "${RESULT_TGZ}.tmp" -C "$RESULT_DIR" "u_${U}" && mv "${RESULT_TGZ}.tmp" "$RESULT_TGZ"
    rm -rf "$RESULT"
    echo "[DONE] results_u_${U}.tar.gz"
done

echo "[DONE]"
