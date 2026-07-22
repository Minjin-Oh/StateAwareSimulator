#pragma once
#include "stateaware.h"

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_write_dynwl(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b);
// aligned with the actual definition in assignW.c
block* assign_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata,
                               bhead* fblist_head, bhead* write_head, block* cur_b,
                               int* w_lpas, int idx, long cur_cp);
