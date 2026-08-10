#ifndef _SHADOW_STATS_H_
#define _SHADOW_STATS_H_

/* [C1/C2/C3 SHADOW EVALUATION]
 * Per-decision comparison of the mode's actual pick against what the dynamic
 * (state-aware) criterion would have picked under the same state. See §3.4:
 *     chosen  = select(candidates, criterion=mode)   // real behavior
 *     shadow  = select(candidates, criterion=dyn)    // read-only
 *     divergence += (chosen != shadow)
 *
 * Owners:
 *   g_shadow_write  populated in assignW.c: assign_write_invalid()
 *   g_shadow_gc     populated in findGC.c : find_gc_utilsort()
 *   g_shadow_admit  populated in emul_main.c LaWL-S block (§C3 already logs
 *                   admit rate; this adds the shadow rrutil comparison)
 *
 * Reader: emul_main.c, right after write_job_start_q / gc_job_start_q / the
 * LaWL-S admit block. `valid=1` means the last decision hit the instrumented
 * path (fblist fallback for write, full_head loop for gc). The reader must
 * clear `valid` after logging so a subsequent unrelated call doesn't get
 * mis-attributed. */

typedef struct {
    long  cur_cp;
    int   task;
    int   n_candidates;      /* fblist blocks iterated */
    int   n_feasible_mode;   /* passed _find_write_safe under latency_mode */
    int   n_feasible_shadow; /* passed _find_write_safe under LATENCY_MODE_STATE */
    int   chosen_mode;       /* block idx picked by mode-lens (-1 if none) */
    int   chosen_shadow;     /* block idx picked by STATE-lens (-1 if none) */
    int   young_or_old;      /* 0=young (min-state), 1=old (max-state) */
    int   valid;             /* 1 after populated; reader clears */
} shadow_stat_write;

typedef struct {
    long  cur_cp;
    int   task;
    int   n_candidates;      /* full_head blocks iterated */
    int   n_tie_mode;        /* candidates sharing min gc_util under mode-lens */
    float min_gcutil_mode;   /* the min gc_util value under mode-lens */
    int   chosen_mode;       /* victim idx picked by mode-lens (-1 if none) */
    int   chosen_shadow;     /* victim idx picked by STATE-lens (-1 if none) */
    int   valid;
} shadow_stat_gc;

extern shadow_stat_write g_shadow_write;
extern shadow_stat_gc    g_shadow_gc;

#endif /* _SHADOW_STATS_H_ */
