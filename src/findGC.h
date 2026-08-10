#pragma once
#include "stateaware.h"
#include "findW.h"

block* find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head);
block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head);
/* [C2 SHADOW] Populates g_shadow_gc (see shadow_stats.h) with mode-lens tie
 * count and STATE-lens shadow pick. Call right after find_gc_utilsort. */
void compute_shadow_gc(rttask* task, int taskidx, int tasknum, meta* metadata,
                       bhead* full_head, bhead* rsvlist_head, bhead* write_head,
                       block* chosen_mode);