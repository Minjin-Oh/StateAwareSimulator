#!/usr/bin/env bash
# Smoke test for Item 1: LaWL-S admit/skip counter.
# Runs 4 latency-mode variants in parallel for a fixed wall-clock budget,
# then inspects the resulting *_admit.csv / *_admit_summary.csv files.
set -eu

SIM=/mnt/d/source/repos/StateAwareSimulator/statesimul.out
OUT=${OUT:-$HOME/lawl_smoke}
BUDGET=${BUDGET:-120}   # seconds to let each sim run before killing

pkill -9 -f statesimul.out 2>/dev/null || true
sleep 1
mkdir -p "$OUT"
cd "$OUT"
rm -f *.csv *.csv.gz *.err *.log

echo "[SMOKE] Regenerating taskset & workload in $OUT..."
"$SIM" NO NO NO TASKGEN 4 0.2 -1 0.05 0.95 0 > taskgen.log 2>&1
echo "[SMOKE] taskgen rc=$?"
"$SIM" NO NO NO WORKGEN 4 0.2 -1 0.05 0.95 0 > workgen.log 2>&1
echo "[SMOKE] workgen rc=$?"
ls -la

echo "[SMOKE] Launching 4 LaWL variants (budget=${BUDGET}s)..."
"$SIM" UTILGC INVW RR005 nogen 4 0.2 -1 0.05 0.95 0 1500          > LaWL.log     2> LaWL.err     &
PID_STATE=$!
"$SIM" UTILGC INVW RR005 nogen 4 0.2 -1 0.05 0.95 0 1500 LAWL_OPT > LaWL_opt.log 2> LaWL_opt.err &
PID_OPT=$!
"$SIM" UTILGC INVW RR005 nogen 4 0.2 -1 0.05 0.95 0 1500 LAWL_AVG > LaWL_avg.log 2> LaWL_avg.err &
PID_AVG=$!
"$SIM" UTILGC INVW RR005 nogen 4 0.2 -1 0.05 0.95 0 1500 LAWL_PES > LaWL_pes.log 2> LaWL_pes.err &
PID_PES=$!

echo "[SMOKE] PIDs: state=$PID_STATE opt=$PID_OPT avg=$PID_AVG pes=$PID_PES"
sleep "$BUDGET"

echo "[SMOKE] Killing variants..."
kill -TERM "$PID_STATE" "$PID_OPT" "$PID_AVG" "$PID_PES" 2>/dev/null || true
sleep 2
kill -9 "$PID_STATE" "$PID_OPT" "$PID_AVG" "$PID_PES" 2>/dev/null || true
wait 2>/dev/null || true

# Locate the per-iter event trace for a scheme. Since smoke launches without
# SIM_ITER exported, the sim opens $tag_$kind.csv.gz (no _NNN suffix). Sweep
# runs get _001, _002, ... — smoke inspects only the first match via glob.
gz_path() {
    local tag="$1"; local kind="$2"
    local direct="${tag}_${kind}.csv.gz"
    if [ -f "$direct" ]; then
        echo "$direct"; return
    fi
    local first
    first=$(ls "${tag}_${kind}"_*.csv.gz 2>/dev/null | head -1)
    echo "$first"
}

echo
echo "[SMOKE] === Admit summaries ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" admit)
    if [ -n "$f" ] && [ -f "$f" ]; then
        total=$(zcat "$f" 2>/dev/null | wc -l)
        admit=$(zcat "$f" 2>/dev/null | awk -F, '$6==1{c++}END{print c+0}')
        skip_ns=$(zcat "$f" 2>/dev/null | awk -F, '$6==0{c++}END{print c+0}')
        skip_nv=$(zcat "$f" 2>/dev/null | awk -F, '$6==2{c++}END{print c+0}')
        rate=$(awk -v a="$admit" -v ns="$skip_ns" -v nv="$skip_nv" 'BEGIN{t=a+ns+nv; if(t>0)printf "%.4f",a/t; else print "0"}')
        printf "%-10s rows=%-6s admit=%-6s skip_ns=%-6s skip_nv=%-6s rate=%s\n" \
               "$tag" "$total" "$admit" "$skip_ns" "$skip_nv" "$rate"
    else
        printf "%-10s (no admit.csv.gz produced)\n" "$tag"
    fi
done

echo
echo "[SMOKE] === First 3 rows of each admit trace (cur_cp,wcu,rrutil,yng,old,dec,wcu_sh,rrutil_sh,dec_sh,div) ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" admit)
    if [ -n "$f" ] && [ -f "$f" ]; then
        echo "-- $tag --"
        zcat "$f" 2>/dev/null | head -3
    fi
done

echo
echo "[SMOKE] === Shadow-write summaries (cols: cur_cp,task,n_cand,feas_m,feas_sh,pick_m,pick_sh,y/o,div) ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" shadow_write)
    if [ -n "$f" ] && [ -f "$f" ]; then
        total=$(zcat "$f" 2>/dev/null | wc -l)
        zcat "$f" 2>/dev/null | awk -F, -v tag="$tag" -v total="$total" '
            { n += $3; fm += $4; fs += $5; if ($6 != $7) div++; }
            END {
                if (n>0) {
                    printf "%-10s rows=%-6d avg_n_cand=%-6.1f feas_ratio_mode=%.4f feas_ratio_shadow=%.4f div_rate=%.4f\n",
                           tag, total, n/NR, fm/n, fs/n, (NR>0)?div/NR:0;
                } else {
                    printf "%-10s rows=%-6d (no candidates)\n", tag, total;
                }
            }'
    else
        printf "%-10s (no shadow_write.csv.gz)\n" "$tag"
    fi
done

echo
echo "[SMOKE] === Shadow-gc summaries (cols: cur_cp,task,n_cand,n_tie_m,min_util,pick_m,pick_sh,div) ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" shadow_gc)
    if [ -n "$f" ] && [ -f "$f" ]; then
        total=$(zcat "$f" 2>/dev/null | wc -l)
        zcat "$f" 2>/dev/null | awk -F, -v tag="$tag" -v total="$total" '
            { n += $3; t += $4; if ($6 != $7) div++; }
            END {
                if (n>0) {
                    printf "%-10s rows=%-6d avg_n_cand=%-6.1f eu_tie_rate=%.4f div_rate=%.4f\n",
                           tag, total, n/NR, t/n, (NR>0)?div/NR:0;
                } else {
                    printf "%-10s rows=%-6d (no candidates)\n", tag, total;
                }
            }'
    else
        printf "%-10s (no shadow_gc.csv.gz)\n" "$tag"
    fi
done

echo
echo "[SMOKE] === Admit shadow divergence (col10 = div_admit) ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" admit)
    if [ -n "$f" ] && [ -f "$f" ]; then
        zcat "$f" 2>/dev/null | awk -F, -v tag="$tag" '
            { n++; if ($10 == 1) div++; }
            END {
                printf "%-10s rows=%-6d admit_div_rate=%.4f\n", tag, n, (n>0)?div/n:0;
            }'
    fi
done

echo
echo "[SMOKE] === Jobs summary (§C4 confusion matrix + Δ_unsafe) ==="
echo "cols: t_first_dl, t_first_inf, delta_unsafe, releases, feas, infeas, miss, FN, FP, TN, TP, FN_rate, FP_rate"
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    if [ -f "${tag}_jobs_summary.csv" ]; then
        row=$(tail -1 "${tag}_jobs_summary.csv")
        printf "%-10s %s\n" "$tag" "$row"
    else
        printf "%-10s (no jobs_summary.csv)\n" "$tag"
    fi
done

echo
echo "[SMOKE] === Per-job event trace row counts (first 3 rows) ==="
for tag in LaWL LaWL_opt LaWL_avg LaWL_pes; do
    f=$(gz_path "$tag" jobs)
    if [ -n "$f" ] && [ -f "$f" ]; then
        rows=$(zcat "$f" 2>/dev/null | wc -l)
        printf "%-10s jobs.csv.gz rows=%d\n" "$tag" "$rows"
        zcat "$f" 2>/dev/null | head -3 | sed 's/^/    /'
    fi
done

echo
echo "[SMOKE] Done."
