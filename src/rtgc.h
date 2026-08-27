// RTGC (Real-Time Garbage Collection, Chang/Kuo/Lo 2004, ACM TECS).
// Comparison scheme for the LaWL RTSS'26 paper. Kept in a separate
// compilation unit so the seven existing schemes remain untouched
// except at explicit rtgc_mode==1 branches in emul_main.c.
//
// Naming convention in this file follows the paper:
//   pi (PPB), alpha (MINRC), Theta (NOB*PPB), Phi (total_fp),
//   Lambda (live pages), Delta (invalid pages),
//   T_i / G_i (real-time task / paired garbage-collector task),
//   sigma_i = max(pT_i, pG_i) (meta-period),
//   rho / rho_free / rho_T / rho_G / rho_nr (tokens).
//
// The simulator splits each task into independent read/write periods,
// so all Eq.14-family formulas here use the r/w-split form
//   c_T/pT  -->  rn*t_r/rp + wn*t_w/wp
// and G_i's period comes from _gc_period() over the write period.

#pragma once
#include "stateaware.h"

// --- parsing / naming helpers (used by emul_main.c) --------------------
// Returns one of the RTGC_LAT_* enums from types.h, or defaults to
// RTGC_LAT_START (with stderr warning) if argv[12] is missing/unknown.
int  rtgc_parse_lat_mode(char* argv[]);

// Fills `out` with the log-file prefix (e.g. "RTGC_START",
// "RTGC_noWL_PECMAX"). Terminating NUL guaranteed.
void rtgc_log_prefix(int rrflag, int lat_mode, char* out, size_t n);

// --- token state machine ----------------------------------------------
typedef struct _rtgc_state {
    // ---- constants ----
    int   pi;                  // = PPB
    int   alpha;               // = MINRC
    int   tasknum;

    // ---- token counters (invariant: rho == rho_free + Σρ_T + Σρ_G + ρ_nr) --
    int   rho;
    int   rho_free;
    int   rho_init;
    int  *rho_T;               // len tasknum
    int  *rho_G;               // len tasknum
    int   rho_nr;              // non-RT (wear leveler); starts at pi (§3.4.1)

    // ---- meta-period tracking ----
    long *sigma;               // meta-period per task, µs
    long *next_sigma;          // absolute next meta-period boundary, µs
    long *pG;                  // GC period per task (cached), µs; 0 if wn==0

    // ---- WL scratch ----
    int   wl_scan_idx;
    long  wl_next_copy_time;
    int   wl_active_block;     // full-list block currently being drained (-1 = none)
    bhead* wl_write_head;      // dedicated single-block write pool for WL (D3)

    // ---- stats ----
    long  n_recycle;                    // GC recycles that actually erased a block
    long  n_token_from_free;            // GC calls that just harvested tokens from Phi
    long  n_write_deferred_by_token;    // must stay 0 per paper §4.4
    long  n_wl_copies;
    long  n_wl_skip_no_target;
    long  n_wl_skip_no_slack;           // reserved (RTGC WL has no slack gate)
} rtgc_state;

// pi/alpha computations (pure functions).
long rtgc_gc_period_pure(int wp, int wn, int alpha);   // matches _gc_period() semantics
long rtgc_sigma_pure   (int wp, int wn, int alpha);
int  rtgc_rhoT_init_pure(int wp, int wn, int alpha);

// Populate `st` from task set.  Does NOT run admission; caller must invoke
// rtgc_admission_check() and act on its return value.
void rtgc_init(rtgc_state* st, rttask* tasks, int tasknum, meta* metadata);
void rtgc_free(rtgc_state* st);

// Latency values fed into Eq.14 for a given lat_mode.  START/END are
// constants (never re-evaluated); PEC_MAX/PEC_AVG scan metadata->state.
void rtgc_get_latency(int lat_mode, meta* metadata,
                      float* tr_out, float* tw_out, float* te_out);

// Eq.14 (task-level, r/w-split).  Structure identical across all four
// lat_modes; only the injected (tr, tw, te) differ.
float rtgc_util_eq14(rttask* tasks, int tasknum, float tr, float tw, float te);

// Admission result codes (see also RTGC_EXIT_* below).
#define RTGC_ADMIT_PASS   0
#define RTGC_ADMIT_EQ12   1     // ρ_init exceeds Eq.12 upper bound
#define RTGC_ADMIT_EQ13   2     // Σ token demand > ρ_init  (should not happen under D5)
#define RTGC_ADMIT_EQ14   3     // Eq.14 utilization test fails at t=0

int rtgc_admission_check(rtgc_state* st, rttask* tasks, int tasknum,
                          meta* metadata, int lat_mode, FILE* admission_fp);

// --- runtime gates / bookkeeping (Fig.5) -------------------------------
// Meta-period boundary of task i: shed ρ_T[i] - w_T·σ_i/p_T[i] tokens
// (paper §3.3.2).  Called by emul_main when cur_cp >= next_sigma[i];
// caller must then advance next_sigma[i] += sigma[i].
void rtgc_task_metaperiod_boundary(rtgc_state* st, int taskidx, rttask* tasks);

// GC release decision (Fig.5).  Returns 1 if a real block recycle must
// be performed (caller then invokes gc_job_start_q), 0 if tokens were
// harvested from the free pool with no I/O.  In the 0-return case, the
// paper's Ti-supply + shed steps are executed immediately.
int  rtgc_gc_release(rtgc_state* st, int taskidx, meta* metadata,
                     long cur_cp, FILE* token_fp);

// After GCER completes, apply the "actual recycle" branch bookkeeping.
// nondead_copied = gc_valid_count from IO struct.
void rtgc_gc_after_recycle(rtgc_state* st, int taskidx, int nondead_copied,
                           long cur_cp, FILE* token_fp);

// Called before releasing a write job.  Returns 1 if ρ_T[taskidx] ≥ wn.
int  rtgc_write_can_release(const rtgc_state* st, int taskidx, int wn);

// Called on each finish_WR (one token per completed page write).
void rtgc_write_consume(rtgc_state* st, int taskidx, int n);

// Event logger for <prefix>_token.csv.  event is a short string tag
// ("init", "metaperiod", "gc_harvest", "gc_recycle", "write", "wl", ...).
void rtgc_log_tokens(const rtgc_state* st, meta* metadata, long cur_cp,
                     const char* event, int taskidx, FILE* fp);

// Whole-state invariant check.  Aborts on violation when RTGC_ASSERT_INV
// env var is set (opt-in because it is O(tasknum) per call).
void rtgc_assert_invariants(const rtgc_state* st, meta* metadata,
                            const char* where);

// Returns the earliest RTGC-only event time strictly after cur_cp:
// min of {next_sigma[i], wl_next_copy_time}.  __LONG_MAX__ if none.
long rtgc_next_event_time(const rtgc_state* st, long cur_cp);

// --- non-real-time wear leveler (§3.4.2) -------------------------------
// Tunables.  RTGC_WL_DELTA is the paper's "less than average by (2)"
// threshold; RTGC_WL_SLEEP_US is the mandatory pause between two
// consecutive live-page copies (§5.2 uses 50 ms).
#ifndef RTGC_WL_DELTA
#define RTGC_WL_DELTA     2
#endif
#ifndef RTGC_WL_SLEEP_US
#define RTGC_WL_SLEEP_US  50000L
#endif

// Try to enqueue one RTWL_COPY request.  Guarded by wl_next_copy_time
// (mandatory sleep) and by rr->head == NULL (only run when the WL queue
// is idle).  Never issues an erase — the paper "defrosts" cold blocks
// by moving their live pages away and letting the greedy GC eventually
// pick them.  Returns 1 if a copy was enqueued, 0 otherwise.
int rtgc_wl_release(rtgc_state* st, meta* metadata,
                    bhead* fblist_head, bhead* full_head,
                    IOhead* rr, long cur_cp, FILE* wl_fp);

// Called from finish_req when cur_IO->type == RTWL_COPY.  Applies the
// exact bookkeeping of finish_WR minus reserved_write (WL bypasses the
// RT task write pipeline).  If the source data was updated by an RT
// task in the meantime, the destination page is invalidated instead.
void rtgc_finish_wl_copy(IO* cur_IO, meta* metadata);

// --- 1 ms sampling + exit gate ----------------------------------------
// Runs at each 1 ms tick.  Computes all four RTGC latency modes plus
// the LaWL job-level utilization and writes one row to rrchecker.
// Returns the U value for the SELECTED lat_mode (used to decide whether
// to trigger RTGC_EXIT_RTGC_ANALYSIS_OVER).
float rtgc_analysis_sample(rtgc_state* st, rttask* tasks, int tasknum,
                            meta* metadata, int lat_mode,
                            long cur_cp, float lawl_total_u,
                            int oldest, int youngest, FILE* rrchecker_fp);

// Writes the terminal <prefix>_lifetime.csv row.  cur_cp is the exit
// timestamp (µs).  For RTGC_EXIT_ADMISSION_REJECT pass cur_cp = 0.
void rtgc_log_lifetime(FILE* fp, long cur_cp, int exit_reason, int lat_mode,
                       float u0, float u_exit_selected,
                       float u_exit_pecmax, float u_exit_lawl,
                       int oldest, int youngest);

// Exit reasons written into <prefix>_lifetime.csv.  Ordered so 0 is
// success and every other value is an early exit.
enum {
    RTGC_EXIT_RUNTIME_END        = 0,
    RTGC_EXIT_ADMISSION_REJECT   = 1,
    RTGC_EXIT_DEADLINE_MISS      = 2,
    RTGC_EXIT_RTGC_ANALYSIS_OVER = 3,
    RTGC_EXIT_MAXPE              = 4
};

// One-shot self-test that reproduces the paper's Table IV example
// (pi=32, alpha=16, T1=(wn=2,wp=20ms), T2=(wn=5,wp=200ms)) and prints
// results to stderr.  Return 0 on success, non-zero on mismatch.  Called
// from emul_main.c when env var RTGC_SELFTEST is set.
int rtgc_selftest(void);
