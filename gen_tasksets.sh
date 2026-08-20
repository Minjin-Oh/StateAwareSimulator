#!/bin/bash
#
# For each (U, seed), run TASKGEN + WORKGEN in tasksets/U${U}_S${SP}/,
# tar.gz that folder immediately (so disk stays bounded to ~PARALLEL
# tasksets in flight at once), then delete the folder. Concurrency capped
# by PARALLEL.
#
# CPU pinning: wrap the invocation, e.g. `taskset -c 0-15 ./gen_tasksets.sh`.
#
# Env: TASKSET_DIR UTIL_LIST SEED_START SEED_END TASKNUM PARALLEL SIM

set -u

TASKSET_DIR=${TASKSET_DIR:-./tasksets}
UTIL_LIST=${UTIL_LIST:-"0.1 0.15 0.2 0.25 0.3"}
SEED_START=${SEED_START:-1}
SEED_END=${SEED_END:-100}
TASKNUM=${TASKNUM:-4}
PARALLEL=${PARALLEL:-4}
SIM=${SIM:-./statesimul.out}

[ -x "$SIM" ] || { echo "[FATAL] $SIM not executable"; exit 1; }
mkdir -p "$TASKSET_DIR"

trap 'kill $(jobs -pr) 2>/dev/null; exit 130' INT TERM

gen_one() {
    local U="$1" S="$2"
    local SP; SP=$(printf "%03d" "$S")
    local BUNDLE="U${U}_S${SP}"
    local TGZ="$TASKSET_DIR/${BUNDLE}.tar.gz"
    local DIR="$TASKSET_DIR/$BUNDLE"

    [ -f "$TGZ" ] && { echo "[SKIP] $BUNDLE"; return 0; }

    rm -rf "$DIR"; mkdir -p "$DIR"
    (
        cd "$DIR" &&
        "$SIM" NO NO NO TASKGEN "$TASKNUM" "$U" -1 0.05 0.95 0 0 STATE "$S" > taskgen.log 2>&1 &&
        "$SIM" NO NO NO WORKGEN "$TASKNUM" "$U" -1 0.05 0.95 0 0 STATE "$S" > workgen.log 2>&1
    ) || { echo "[FAIL] $BUNDLE"; return 1; }

    # Atomic tar: write .tmp then mv so an interrupted run can't leave a
    # partial .tar.gz that the [SKIP] check on next run would honor.
    tar -czf "${TGZ}.tmp" -C "$TASKSET_DIR" "$BUNDLE" && mv "${TGZ}.tmp" "$TGZ"
    rm -rf "$DIR"
    echo "[OK] $BUNDLE"
}

active=0
for U in $UTIL_LIST; do
    for S in $(seq "$SEED_START" "$SEED_END"); do
        [ "$active" -ge "$PARALLEL" ] && { wait -n; active=$((active - 1)); }
        gen_one "$U" "$S" &
        active=$((active + 1))
    done
done
wait
echo "[DONE]"
