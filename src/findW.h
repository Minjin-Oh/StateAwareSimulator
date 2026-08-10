#pragma once
#include "stateaware.h"

//internal functions
int _find_write_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util, int cur_b, int* w_lpas);
int __calc_invorder_mem(int pagenum, meta* metadata, long cur_lpa_timing, long workload_reset_time, int curfp);

//find writeblock functions
block* find_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long workload_reset_time);
/* [C1 SHADOW] Populates g_shadow_write (shadow_stats.h) with feasibility
 * counts under the current latency_mode vs LATENCY_MODE_STATE, iterating
 * write_head + fblist_head candidates. Read-only; safe to call after any
 * write_job_start_q return. Chosen-block divergence is not measured. */
void compute_shadow_write(rttask* task, int taskidx, int tasknum, meta* metadata,
                          bhead* fblist_head, bhead* write_head);