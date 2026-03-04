#pragma once
#include "stateaware.h"

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_write_dynwl(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, 
                             bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long cur_cp, FILE* fpovhd_w_assign, FILE* w_assign_detail);