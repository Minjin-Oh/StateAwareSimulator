#pragma once
#include "stateaware.h"
#include "findW.h"

block* find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head);
block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head);

// [WAO-GC] greedy victim selector: fewest valid pages first, ties broken by
// lowest P/E cycle (embedded wear-leveler). See findGC.c for the definition.
block* find_gc_waogc(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head);