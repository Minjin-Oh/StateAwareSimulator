// RTGC (Chang/Kuo/Lo 2004) — comparison scheme implementation.
// See rtgc.h for scope and naming conventions.

#include "rtgc.h"
#include "assignW.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

// Globals defined in emul_main.c; assigned by set_exec_flags() in parse.c.
extern int    MINRC;
extern double OP;

// ----------------------------------------------------------------------
// Argument / naming helpers
// ----------------------------------------------------------------------

// argv[12]: "START" | "END" | "PEC_MAX" | "PEC_AVG".
// RTGC mode requires argv[11] (INITCYC) to be present so argv[12] lands
// at the right position (see parse.c:set_exec_flags).
int rtgc_parse_lat_mode(char* argv[]){
    if (argv[12] == NULL){
        fprintf(stderr,
            "[RTGC] warning: argv[12] (latency model) missing; defaulting to START. "
            "Note: argv[11] (INITCYC) must be present for argv[12] to be parsed.\n");
        return RTGC_LAT_START;
    }
    if (strcmp(argv[12], "START")   == 0) return RTGC_LAT_START;
    if (strcmp(argv[12], "END")     == 0) return RTGC_LAT_END;
    if (strcmp(argv[12], "PEC_MAX") == 0) return RTGC_LAT_PECMAX;
    if (strcmp(argv[12], "PEC_AVG") == 0) return RTGC_LAT_PECAVG;
    fprintf(stderr,
        "[RTGC] warning: unknown latency model '%s'; defaulting to START.\n",
        argv[12]);
    return RTGC_LAT_START;
}

// Prefix must match run_simul_rtgc.sh's SCHEME_PREFIX array exactly.
void rtgc_log_prefix(int rrflag, int lat_mode, char* out, size_t n){
    const char* wl_tag = (rrflag == -1) ? "noWL_" : "";
    const char* mode_tag;
    switch (lat_mode){
        case RTGC_LAT_START:  mode_tag = "START";  break;
        case RTGC_LAT_END:    mode_tag = "END";    break;
        case RTGC_LAT_PECMAX: mode_tag = "PECMAX"; break;
        case RTGC_LAT_PECAVG: mode_tag = "PECAVG"; break;
        default:              mode_tag = "UNKNOWN"; break;
    }
    snprintf(out, n, "RTGC_%s%s", wl_tag, mode_tag);
}

// ----------------------------------------------------------------------
// Pure paper-formula computations
// ----------------------------------------------------------------------
//
// _gc_period() in util.c uses float division for the wn>alpha branch:
//   return (int)(task->wp * (float)1 / (float)mult);
// where mult = ceil((float)wn / (float)min_reclaim).
// Reproduce the same value here (matches tasks[i].gcp already stored in
// the task struct) so admission and cached pG never disagree.
long rtgc_gc_period_pure(int wp, int wn, int alpha){
    if (wn <= 0){
        return 0;
    }
    if (alpha >= wn){
        int mult = alpha / wn;                        // floor
        return (long)wp * (long)mult;
    } else {
        int mult = (wn + alpha - 1) / alpha;          // ceil
        // Match util.c semantics: (int)(wp * 1.0f / mult).
        return (long)((double)wp / (double)mult);
    }
}

long rtgc_sigma_pure(int wp, int wn, int alpha){
    long pG = rtgc_gc_period_pure(wp, wn, alpha);
    long wpL = (long)wp;
    return (pG > wpL) ? pG : wpL;
}

int rtgc_rhoT_init_pure(int wp, int wn, int alpha){
    if (wn <= 0 || wp <= 0) return 0;
    long sigma = rtgc_sigma_pure(wp, wn, alpha);
    // ρ_T = w_T * σ / p_T   (integer division truncation matches paper Fig.4 usage)
    return (int)((long)wn * sigma / (long)wp);
}

// ----------------------------------------------------------------------
// State init / teardown
// ----------------------------------------------------------------------
void rtgc_init(rtgc_state* st, rttask* tasks, int tasknum, meta* metadata){
    (void)metadata;
    st->pi        = PPB;
    st->alpha     = MINRC;
    st->tasknum   = tasknum;

    st->rho       = 0;
    st->rho_free  = 0;
    st->rho_init  = 0;
    st->rho_nr    = 0;

    st->rho_T     = (int*)  calloc((size_t)tasknum, sizeof(int));
    st->rho_G     = (int*)  calloc((size_t)tasknum, sizeof(int));
    st->sigma     = (long*) calloc((size_t)tasknum, sizeof(long));
    st->next_sigma= (long*) calloc((size_t)tasknum, sizeof(long));
    st->pG        = (long*) calloc((size_t)tasknum, sizeof(long));

    for (int i = 0; i < tasknum; i++){
        st->pG[i]         = rtgc_gc_period_pure(tasks[i].wp, tasks[i].wn, st->alpha);
        st->sigma[i]      = rtgc_sigma_pure   (tasks[i].wp, tasks[i].wn, st->alpha);
        st->next_sigma[i] = st->sigma[i];                    // first boundary at σ_i
    }

    st->wl_scan_idx        = 0;
    st->wl_next_copy_time  = 0;
    st->wl_active_block    = -1;
    st->wl_write_head      = ll_init();

    st->n_recycle                = 0;
    st->n_token_from_free        = 0;
    st->n_write_deferred_by_token= 0;
    st->n_wl_copies              = 0;
    st->n_wl_skip_no_target      = 0;
    st->n_wl_skip_no_slack       = 0;
}

void rtgc_free(rtgc_state* st){
    if (!st) return;
    if (st->rho_T)      free(st->rho_T);
    if (st->rho_G)      free(st->rho_G);
    if (st->sigma)      free(st->sigma);
    if (st->next_sigma) free(st->next_sigma);
    if (st->pG)         free(st->pG);
    if (st->wl_write_head){ ll_free(st->wl_write_head); st->wl_write_head = NULL; }
    st->rho_T = st->rho_G = NULL;
    st->sigma = st->next_sigma = st->pG = NULL;
}

// ----------------------------------------------------------------------
// Latency injection for Eq.14
// ----------------------------------------------------------------------
void rtgc_get_latency(int lat_mode, meta* metadata,
                      float* tr_out, float* tw_out, float* te_out){
    if (lat_mode == RTGC_LAT_START){
        // Fixed low-PEC values — reference macros directly so an EXECSTEP
        // build cannot silently substitute a different curve.
        *tr_out = (float)STARTR;
        *tw_out = (float)STARTW;
        *te_out = (float)STARTE;
        return;
    }
    if (lat_mode == RTGC_LAT_END){
        *tr_out = (float)ENDR;
        *tw_out = (float)ENDW;
        *te_out = (float)ENDE;
        return;
    }
    if (lat_mode == RTGC_LAT_PECMAX){
        int oldest   = get_blockstate_meta(metadata, OLD);
        int youngest = get_blockstate_meta(metadata, YOUNG);
        // Write latency is a decreasing function of PEC, so its worst
        // (largest) value is at the youngest block.
        *tr_out = r_exec(oldest);
        *tw_out = w_exec(youngest);
        *te_out = e_exec(oldest);
        return;
    }
    if (lat_mode == RTGC_LAT_PECAVG){
        long sum = 0;
        for (int i = 0; i < NOB; i++) sum += metadata->state[i];
        int avg = (int)((sum + NOB/2) / NOB);   // rounded
        *tr_out = r_exec(avg);
        *tw_out = w_exec(avg);
        *te_out = e_exec(avg);
        return;
    }
    // Unknown mode: fall back to START so callers can't NaN their sums.
    *tr_out = (float)STARTR;
    *tw_out = (float)STARTW;
    *te_out = (float)STARTE;
}

// ----------------------------------------------------------------------
// Eq.14 utilization (task-level, r/w-split extension)
// ----------------------------------------------------------------------
float rtgc_util_eq14(rttask* tasks, int tasknum, float tr, float tw, float te){
    float total = 0.0f;
    for (int i = 0; i < tasknum; i++){
        // c_T_i / p_T_i  →  rn·t_r/rp + wn·t_w/wp
        if (tasks[i].rp > 0 && tasks[i].rn > 0){
            total += (float)tasks[i].rn * tr / (float)tasks[i].rp;
        }
        if (tasks[i].wp > 0 && tasks[i].wn > 0){
            total += (float)tasks[i].wn * tw / (float)tasks[i].wp;
        }
        // c_G_i / p_G_i  (only tasks with wn>0 have a paired GC)
        if (tasks[i].wn > 0){
            long pG = rtgc_gc_period_pure(tasks[i].wp, tasks[i].wn, MINRC);
            if (pG > 0){
                float c_G = (float)(PPB - MINRC) * (tr + tw) + te;
                total += c_G / (float)pG;
            }
        }
    }
    // Blocking factor: worst-case erase / shortest active period.
    int min_p = _find_min_period(tasks, tasknum);
    if (min_p > 0){
        total += te / (float)min_p;
    }
    return total;
}

// ----------------------------------------------------------------------
// Admission
// ----------------------------------------------------------------------
int rtgc_admission_check(rtgc_state* st, rttask* tasks, int tasknum,
                          meta* metadata, int lat_mode, FILE* fp){
    (void)metadata;

    // --- Eq.12 upper bound on ρ_init ---
    // ρ_init < ((1 − (α−1)/π)·Θ − Λ − α + 1 − 2π) / 2
    // Λ (live pages) is bounded conservatively by max_valid_pg = ⌈(1−OP)·Θ⌉.
    double Theta  = (double)PPB * (double)NOB;
    double Lambda = (1.0 - OP) * Theta;                    // matches max_valid_pg
    double numer  = (1.0 - (double)(st->alpha - 1) / (double)st->pi) * Theta
                    - Lambda
                    - (double)st->alpha + 1.0
                    - 2.0 * (double)st->pi;
    double eq12_bound_d = numer / 2.0;
    long   eq12_bound   = (long)floor(eq12_bound_d);       // largest safe ρ_init

    // --- token demand (Eq.13 LHS) ---
    // Σ_i (w_i·σ_i/wp_i + (π-α))  +  ρ_nr (=π)
    long demand = 0;
    for (int i = 0; i < tasknum; i++){
        int rT = rtgc_rhoT_init_pure(tasks[i].wp, tasks[i].wn, st->alpha);
        if (tasks[i].wn > 0){
            demand += rT + (st->pi - st->alpha);           // T_i + G_i
        }
    }
    demand += st->pi;                                       // ρ_nr

    // --- pick ρ_init per D5 ---
    long half     = (long)floor(eq12_bound_d / 2.0);
    long rho_init = (demand > half) ? demand : half;

    int reason = RTGC_ADMIT_PASS;
    int pass   = 1;

    // Eq.12: hard reject if even demand alone breaches the bound.
    if (demand >= eq12_bound_d){
        reason = RTGC_ADMIT_EQ12;
        pass   = 0;
    }
    // Eq.13: with D5's max() rule this can only fail if we picked
    // ρ_init = half < demand (which max() prevents), so treat as invariant.
    else if (rho_init < demand){
        reason = RTGC_ADMIT_EQ13;
        pass   = 0;
    }

    // Populate the state whether or not admission passes (so overhead
    // fields still have sane values in the reject case).
    st->rho_init = (int)rho_init;
    st->rho_free = st->rho_init;
    st->rho      = st->rho_init;

    if (pass){
        // Give initial tokens to each T_i and G_i, plus ρ_nr = π.
        for (int i = 0; i < tasknum; i++){
            if (tasks[i].wn <= 0) continue;
            int rT = rtgc_rhoT_init_pure(tasks[i].wp, tasks[i].wn, st->alpha);
            st->rho_T[i] = rT;
            st->rho_G[i] = st->pi - st->alpha;
            st->rho_free -= (rT + (st->pi - st->alpha));
        }
        st->rho_nr    = st->pi;
        st->rho_free -= st->pi;
    }

    // Eq.14 — task-level utilization under the chosen lat_mode.
    float tr, tw, te;
    rtgc_get_latency(lat_mode, metadata, &tr, &tw, &te);
    float U = rtgc_util_eq14(tasks, tasknum, tr, tw, te);
    if (pass && U > 1.0f){
        reason = RTGC_ADMIT_EQ14;
        pass   = 0;
    }

    if (fp){
        fprintf(fp,
            "cur_cp,verdict,reason,lat_mode,pi,alpha,Theta,Lambda,eq12_bound,demand,rho_init,U_eq14\n");
        const char* reason_str;
        switch (reason){
            case RTGC_ADMIT_PASS: reason_str = "PASS"; break;
            case RTGC_ADMIT_EQ12: reason_str = "EQ12"; break;
            case RTGC_ADMIT_EQ13: reason_str = "EQ13"; break;
            case RTGC_ADMIT_EQ14: reason_str = "EQ14"; break;
            default:              reason_str = "?";    break;
        }
        fprintf(fp,
            "0,%s,%s,%d,%d,%d,%.0f,%.0f,%ld,%ld,%d,%.6f\n",
            pass ? "PASS" : "REJECT",
            reason_str, lat_mode,
            st->pi, st->alpha, Theta, Lambda,
            eq12_bound, demand, st->rho_init, U);
        fflush(fp);
    }

    return reason;
}

// ----------------------------------------------------------------------
// Runtime gates and bookkeeping — Fig.5 of the paper.
//
// Invariants we maintain (verified by rtgc_assert_invariants):
//   (a) rho == rho_free + Σρ_T + Σρ_G + ρ_nr
//   (b) rho_G[i] == pi - alpha at the *end* of every Gi() invocation
//       (during a recycle it dips as copies consume tokens, but between
//       calls it is always restored).  We only assert (b) after
//       rtgc_gc_after_recycle() completes.
//   (c) rho <= Phi (no more tokens outstanding than physical free pages).
//       Enforced softly: violations warn rather than abort because the
//       paper's proof relies on Phi seen at Gi entry, not at every
//       instant; the check exists to catch policy bugs.
// ----------------------------------------------------------------------

// Log a single token snapshot into <prefix>_token.csv.
void rtgc_log_tokens(const rtgc_state* st, meta* metadata, long cur_cp,
                     const char* event, int taskidx, FILE* fp){
    if (!fp) return;
    // Emit header once (detected by a zero-byte file at open time by
    // caller if desired; here we just write rows).
    fprintf(fp, "%ld,%s,%d,%d,%d,%d,%d",
            cur_cp, event ? event : "?", taskidx,
            st->rho, st->rho_free,
            metadata ? metadata->total_fp : -1,
            st->rho_nr);
    for (int i = 0; i < st->tasknum; i++) fprintf(fp, ",%d", st->rho_T[i]);
    for (int i = 0; i < st->tasknum; i++) fprintf(fp, ",%d", st->rho_G[i]);
    fprintf(fp, "\n");
}

void rtgc_assert_invariants(const rtgc_state* st, meta* metadata,
                            const char* where){
    if (!st) return;
    int sum = st->rho_free + st->rho_nr;
    for (int i = 0; i < st->tasknum; i++) sum += st->rho_T[i] + st->rho_G[i];
    if (sum != st->rho){
        fprintf(stderr,
            "[RTGC-INV] rho mismatch at %s: rho=%d, buckets=%d\n",
            where ? where : "?", st->rho, sum);
        if (getenv("RTGC_ASSERT_INV")) abort();
    }
    if (metadata && st->rho > metadata->total_fp){
        fprintf(stderr,
            "[RTGC-INV] rho (%d) > Phi (%d) at %s (informational).\n",
            st->rho, metadata->total_fp, where ? where : "?");
    }
}

void rtgc_task_metaperiod_boundary(rtgc_state* st, int taskidx, rttask* tasks){
    if (tasks[taskidx].wn <= 0) return;
    int quota = rtgc_rhoT_init_pure(tasks[taskidx].wp, tasks[taskidx].wn, st->alpha);
    int excess = st->rho_T[taskidx] - quota;
    if (excess > 0){
        st->rho_T[taskidx] -= excess;
        st->rho             -= excess;   // permanently shed (paper §3.3.2)
    }
}

int rtgc_gc_release(rtgc_state* st, int taskidx, meta* metadata,
                    long cur_cp, FILE* token_fp){
    if (metadata->total_fp - st->rho >= st->alpha){
        // Virtual harvest (Fig.5 "if" branch): free-page pool has enough
        // slack, create α tokens directly.
        st->rho_G[taskidx] += st->alpha;
        st->rho            += st->alpha;

        // Immediately execute the paper's Ti-supply + shed steps.
        // Transfer α from ρ_G to ρ_T (bucket move, ρ unchanged).
        st->rho_T[taskidx] += st->alpha;
        st->rho_G[taskidx] -= st->alpha;
        // Shed residual over (π - α).
        int z = st->rho_G[taskidx] - (st->pi - st->alpha);
        if (z > 0){
            st->rho_G[taskidx] -= z;
            st->rho            -= z;
        }
        st->n_token_from_free++;
        if (token_fp) rtgc_log_tokens(st, metadata, cur_cp,
                                       "gc_harvest", taskidx, token_fp);
        return 0;   // no actual recycle
    }
    // Physical recycle required.  The token bookkeeping for the "else"
    // branch happens after GCER finishes in rtgc_gc_after_recycle().
    return 1;
}

void rtgc_gc_after_recycle(rtgc_state* st, int taskidx, int nondead_copied,
                           long cur_cp, FILE* token_fp){
    (void)nondead_copied;   // §4.4 uses it in the safety proof, not Fig.5.
    // Post-erase net token effect exactly mirrors Fig.5's else branch:
    //   ρ_G += π; ρ += π       # new tokens from erase (net-of-copies)
    //   ρ_T += α; ρ_G -= α    # supply Ti (bucket move, ρ unchanged)
    //   z = ρ_G - (π - α); ρ_G -= z; ρ -= z    # shed residual
    // Starting from ρ_G[i] = π - α (steady-state invariant), the net
    // Δρ per recycle is exactly +α, identical to the virtual-harvest
    // branch.  This is what preserves ρ ≤ ρ_max (Eq.7).
    st->rho_G[taskidx] += st->pi;
    st->rho            += st->pi;

    // Supply Ti with α tokens.
    st->rho_T[taskidx] += st->alpha;
    st->rho_G[taskidx] -= st->alpha;

    // Shed residual over (π - α).
    int z = st->rho_G[taskidx] - (st->pi - st->alpha);
    if (z > 0){
        st->rho_G[taskidx] -= z;
        st->rho            -= z;
    }
    st->n_recycle++;
    if (token_fp) rtgc_log_tokens(st, NULL, cur_cp, "gc_recycle",
                                   taskidx, token_fp);
    // NOTE: caller is responsible for calling rtgc_assert_invariants
    // if RTGC_ASSERT_INV is set — we avoid the O(tasknum) scan here
    // because this is a hot path.
}

int rtgc_write_can_release(const rtgc_state* st, int taskidx, int wn){
    return (st->rho_T[taskidx] >= wn) ? 1 : 0;
}

void rtgc_write_consume(rtgc_state* st, int taskidx, int n){
    st->rho_T[taskidx] -= n;
    st->rho            -= n;
}

long rtgc_next_event_time(const rtgc_state* st, long cur_cp){
    long best = __LONG_MAX__;
    for (int i = 0; i < st->tasknum; i++){
        long t = st->next_sigma[i];
        if (t > cur_cp && t < best) best = t;
    }
    if (st->wl_next_copy_time > cur_cp && st->wl_next_copy_time < best){
        best = st->wl_next_copy_time;
    }
    return best;
}

// ----------------------------------------------------------------------
// Non-real-time wear leveler (§3.4.2, §5.2)
// ----------------------------------------------------------------------
//
// Behaviour per the paper:
//   - Runs at background priority; deadline = LONG_MAX so any real-time
//     job pre-empts at operation boundaries (SRP).
//   - Circularly scans blocks for a full-list block with erase count
//     below (avg - RTGC_WL_DELTA) that still holds live pages.
//   - Copies exactly one live page per invocation, then sleeps for
//     RTGC_WL_SLEEP_US before considering the next copy.
//   - Never issues an erase.  As dead pages accumulate on the cold
//     block, the greedy GC eventually picks it and levels wear.
//   - Uses a dedicated single-block write pool (st->wl_write_head) so
//     WL copies never share a page with any RT task's current write
//     block (D3).
int rtgc_wl_release(rtgc_state* st, meta* metadata,
                    bhead* fblist_head, bhead* full_head,
                    IOhead* rr, long cur_cp, FILE* wl_fp){
    // 1. Ensure at least one token for the destination write.  §1.2
    //    non-RT rules: if Phi > rho we can freely create pi tokens.
    if (st->rho_nr < 1){
        if (metadata->total_fp > st->rho){
            st->rho_nr += st->pi;
            st->rho    += st->pi;
        } else {
            st->n_wl_skip_no_slack++;
            return 0;
        }
    }

    // 2. Compute avg erase count and threshold.
    long sum = 0;
    for (int i = 0; i < NOB; i++) sum += (long)metadata->state[i];
    double avg = (double)sum / (double)NOB;
    // "less than the average by RTGC_WL_DELTA" -> strict less-than test.
    // A block qualifies if state[b] + RTGC_WL_DELTA < avg.
    double thresh_d = avg - (double)RTGC_WL_DELTA;

    // 3. Circular scan for a cold block that is currently full and has
    //    live pages.  st->wl_scan_idx remembers where we left off.
    int found_bidx = -1;
    for (int step = 0; step < NOB; step++){
        int b = (st->wl_scan_idx + step) % NOB;
        if ((double)metadata->state[b] >= thresh_d) continue;
        if (!is_idx_in_list(full_head, b))         continue;
        int live = PPB - metadata->invnum[b];
        if (live <= 0)                              continue;
        found_bidx = b;
        st->wl_scan_idx = (b + 1) % NOB;
        break;
    }
    if (found_bidx < 0){
        st->n_wl_skip_no_target++;
        // Even if nothing qualifies now, advance the sleep timer so we
        // don't busy-poll every microsecond.
        st->wl_next_copy_time = cur_cp + RTGC_WL_SLEEP_US;
        return 0;
    }

    // 4. Pick the first live page in the source block.
    int src_offset = found_bidx * PPB;
    int src_ppa    = -1;
    for (int p = src_offset; p < src_offset + PPB; p++){
        if (metadata->invmap[p] == 0 && metadata->rmap[p] != -1){
            src_ppa = p;
            break;
        }
    }
    if (src_ppa < 0){
        st->n_wl_skip_no_target++;
        st->wl_next_copy_time = cur_cp + RTGC_WL_SLEEP_US;
        return 0;
    }

    // 5. Allocate a destination page through WL's dedicated write pool.
    //    assign_write_FIFO() only reads write_head->head and pops from
    //    fblist_head if empty; it does not consult taskidx.
    block* dst = assign_write_FIFO(NULL, 0, 0, metadata,
                                   fblist_head, st->wl_write_head, NULL);
    if (dst == NULL){
        st->n_wl_skip_no_target++;
        st->wl_next_copy_time = cur_cp + RTGC_WL_SLEEP_US;
        return 0;
    }
    int dst_offset = dst->idx * PPB;
    int dst_ppa    = dst_offset + (PPB - dst->fpnum);
    dst->fpnum--;

    // If the WL's dedicated block just filled up, move it to full_head.
    if (dst->fpnum == 0){
        block* moved = ll_remove(st->wl_write_head, dst->idx);
        if (moved) ll_append(full_head, moved);
    }

    // 6. Build the RTWL_COPY request.  Reuse the RR_-family fields so
    //    a future finish path can share code with existing relocations
    //    if needed.  deadline = LONG_MAX -> background.
    IO* req = (IO*)malloc(sizeof(IO));
    memset(req, 0, sizeof(IO));
    req->type          = RTWL_COPY;
    req->taskidx       = -1;
    req->rr_old_lpa    = metadata->rmap[src_ppa];
    req->rr_vic_ppa    = src_ppa;
    req->rr_tar_ppa    = dst_ppa;
    req->vic_idx       = found_bidx;
    req->tar_idx       = dst->idx;
    req->IO_start_time = cur_cp;
    req->deadline      = LONG_MAX;
    req->exec = (long)floor((double)r_exec(metadata->state[found_bidx]))
              + (long)floor((double)w_exec(metadata->state[dst->idx]));
    req->last      = 1;
    req->islastreq = 1;
    req->init      = 1;
    ll_append_IO(rr, req);

    // 7. Consume one ρ_nr token now (matches page-write semantics).
    st->rho_nr -= 1;
    st->rho    -= 1;

    // 8. Schedule next copy after the mandatory pause and log.
    st->wl_next_copy_time = cur_cp + RTGC_WL_SLEEP_US;
    st->n_wl_copies++;
    if (wl_fp){
        fprintf(wl_fp, "%ld,%d,%d,%.2f,%d,%d,%d\n",
                cur_cp, found_bidx, metadata->state[found_bidx], avg,
                dst->idx, PPB - metadata->invnum[found_bidx] - 1,
                st->rho_nr);
    }
    return 1;
}

// finish_req branch for RTWL_COPY.  Mirrors finish_WR but does NOT
// touch reserved_write (WL bypasses the RT write pipeline) and does
// not erase.  Stale-copy race: if an RT task updated the same LPA
// between rtgc_wl_release() and now, the source may already be
// invalid — in that case we invalidate the destination page instead.
void rtgc_finish_wl_copy(IO* cur_IO, meta* metadata){
    int old_lpa   = cur_IO->rr_old_lpa;
    int old_ppa   = cur_IO->rr_vic_ppa;
    int new_ppa   = cur_IO->rr_tar_ppa;
    int old_block = old_ppa / PPB;
    int new_block = new_ppa / PPB;

    if (metadata->pagemap[old_lpa] == old_ppa){
        // Fresh copy: invalidate old, validate new.
        metadata->rmap[old_ppa]      = -1;
        metadata->invmap[old_ppa]    = 1;
        metadata->vmap_task[old_ppa] = -1;
        metadata->invnum[old_block] += 1;
        metadata->invalidation_window[old_block] += 1;
        metadata->total_invalid++;

        metadata->pagemap[old_lpa]  = new_ppa;
        metadata->rmap[new_ppa]     = old_lpa;
        metadata->invmap[new_ppa]   = 0;
        metadata->vmap_task[new_ppa] = -1;   // WL-owned; no RT task claim
    } else {
        // Stale: the destination write is wasted, mark it invalid.
        metadata->rmap[new_ppa]     = -1;
        metadata->invmap[new_ppa]   = 1;
        metadata->vmap_task[new_ppa] = -1;
        metadata->invnum[new_block] += 1;
        metadata->total_invalid++;
    }

    // The destination page was already reserved via dst->fpnum-- in
    // rtgc_wl_release, so we account for its consumption here.
    metadata->total_fp--;
}

// ----------------------------------------------------------------------
// 1 ms sampling and terminal lifetime logging
// ----------------------------------------------------------------------
//
// Every millisecond of sim time (identical cadence to LaWL's
// print_profile_timestamp) we recompute the RTGC Eq.14 utilization
// under all four latency models and emit one row to <prefix>_rrchecker.
// This gives a single log with four analysis curves superimposed, so
// (START vs PECMAX) reveals the effect of latency drift and
// (PECMAX vs LaWL) reveals the effect of job-level precision.
//
// STARTS and END are latched at t=0 by rtgc_admission_check() and never
// re-evaluated per D-Cadence.  PEC_MAX / PEC_AVG scan current block
// state each call.
//
// The invariant U_START ≤ U_PECAVG ≤ U_PECMAX ≤ U_END is enforced only
// implicitly by the underlying latency curves; if it ever breaks, the
// row still gets written so post-hoc analysis can catch the anomaly.
static float _rtgc_u_start_cached = -1.0f;
static float _rtgc_u_end_cached   = -1.0f;

float rtgc_analysis_sample(rtgc_state* st, rttask* tasks, int tasknum,
                            meta* metadata, int lat_mode,
                            long cur_cp, float lawl_total_u,
                            int oldest, int youngest, FILE* rrchecker_fp){
    (void)st;

    // START / END are PEC-independent constants, latch once.
    if (_rtgc_u_start_cached < 0.0f){
        _rtgc_u_start_cached =
            rtgc_util_eq14(tasks, tasknum,
                            (float)STARTR, (float)STARTW, (float)STARTE);
    }
    if (_rtgc_u_end_cached < 0.0f){
        _rtgc_u_end_cached =
            rtgc_util_eq14(tasks, tasknum,
                            (float)ENDR, (float)ENDW, (float)ENDE);
    }

    // PEC_MAX / PEC_AVG re-scan block state each call.
    float trM, twM, teM, trA, twA, teA;
    rtgc_get_latency(RTGC_LAT_PECMAX, metadata, &trM, &twM, &teM);
    rtgc_get_latency(RTGC_LAT_PECAVG, metadata, &trA, &twA, &teA);
    float U_pecmax = rtgc_util_eq14(tasks, tasknum, trM, twM, teM);
    float U_pecavg = rtgc_util_eq14(tasks, tasknum, trA, twA, teA);

    // Compute PEC-diversity stats identical to print_profile_timestamp.
    long   sum = 0;
    double avg = 0.0, var = 0.0;
    for (int i = 0; i < NOB; i++) sum += (long)metadata->state[i];
    avg = (double)sum / (double)NOB;
    for (int i = 0; i < NOB; i++){
        double d = (double)metadata->state[i] - avg;
        var += d * d;
    }
    var = sqrt(var / (double)NOB);

    if (rrchecker_fp){
        fprintf(rrchecker_fp,
            "%ld,%f,%f,%f,%f,%f,%d,%d,%f,%f\n",
            cur_cp,
            _rtgc_u_start_cached, _rtgc_u_end_cached,
            U_pecmax, U_pecavg,
            lawl_total_u,
            oldest, youngest, avg, var);
    }

    switch (lat_mode){
        case RTGC_LAT_START:  return _rtgc_u_start_cached;
        case RTGC_LAT_END:    return _rtgc_u_end_cached;
        case RTGC_LAT_PECMAX: return U_pecmax;
        case RTGC_LAT_PECAVG: return U_pecavg;
        default:              return _rtgc_u_start_cached;
    }
}

void rtgc_log_lifetime(FILE* fp, long cur_cp, int exit_reason, int lat_mode,
                       float u0, float u_exit_selected,
                       float u_exit_pecmax, float u_exit_lawl,
                       int oldest, int youngest){
    if (!fp) return;
    const char* reason_str;
    switch (exit_reason){
        case RTGC_EXIT_RUNTIME_END:        reason_str = "RUNTIME_END";       break;
        case RTGC_EXIT_ADMISSION_REJECT:   reason_str = "ADMISSION_REJECT";  break;
        case RTGC_EXIT_DEADLINE_MISS:      reason_str = "DEADLINE_MISS";     break;
        case RTGC_EXIT_RTGC_ANALYSIS_OVER: reason_str = "RTGC_ANALYSIS_OVERFLOW"; break;
        case RTGC_EXIT_MAXPE:              reason_str = "MAXPE";             break;
        default:                            reason_str = "UNKNOWN";           break;
    }
    fprintf(fp,
        "%ld,%s,%d,%f,%f,%f,%f,%d,%d\n",
        cur_cp, reason_str, lat_mode,
        u0, u_exit_selected, u_exit_pecmax, u_exit_lawl,
        oldest, youngest);
    fflush(fp);
}

// ----------------------------------------------------------------------
// Self-test — Table IV from the paper
// ----------------------------------------------------------------------
int rtgc_selftest(void){
    // Override alpha and pi to match paper example.
    const int alpha = 16;
    const int pi    = 32;

    struct { int wp; int wn; long exp_pG; long exp_sigma; int exp_rhoT; } cases[] = {
        // T1: wn=2, wp=20ms  →  pG=160ms, sigma=160ms, rho_T=16
        { 20000,  2, 160000L, 160000L, 16 },
        // T2: wn=5, wp=200ms →  pG=600ms, sigma=600ms, rho_T=15
        {200000,  5, 600000L, 600000L, 15 },
    };
    int expected_rhoG = pi - alpha;                        // = 16

    int fail = 0;
    fprintf(stderr, "[RTGC-SELFTEST] alpha=%d, pi=%d\n", alpha, pi);
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++){
        long pG   = rtgc_gc_period_pure(cases[i].wp, cases[i].wn, alpha);
        long sig  = rtgc_sigma_pure    (cases[i].wp, cases[i].wn, alpha);
        int  rT   = rtgc_rhoT_init_pure(cases[i].wp, cases[i].wn, alpha);
        int  rG   = pi - alpha;
        int  ok   = (pG == cases[i].exp_pG)
                 && (sig == cases[i].exp_sigma)
                 && (rT  == cases[i].exp_rhoT)
                 && (rG  == expected_rhoG);
        fprintf(stderr,
            "  T%d (wp=%d, wn=%d): pG=%ld/%ld sigma=%ld/%ld rho_T=%d/%d rho_G=%d/%d  %s\n",
            i+1, cases[i].wp, cases[i].wn,
            pG, cases[i].exp_pG,
            sig, cases[i].exp_sigma,
            rT, cases[i].exp_rhoT,
            rG, expected_rhoG,
            ok ? "OK" : "FAIL");
        if (!ok) fail = 1;
    }
    fprintf(stderr, "[RTGC-SELFTEST] %s\n", fail ? "FAILED" : "PASSED");
    return fail ? -1 : 0;
}
