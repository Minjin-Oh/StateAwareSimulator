#pragma once
#include "stateaware.h"
#include "findW.h"

int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head, FILE* gc_detail);
block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head);