#pragma once
#include "stateaware.h"

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_write_ctrl(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_write_greedy(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* lpas);
block* assign_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* lpas, int task_start_idx);
block* assign_writefixed(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_writehotness(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx);
block* assign_write_old(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b);
block* assign_writehot_motiv(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b, int lpa, int policy);
block* assign_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, 
                           bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx);
