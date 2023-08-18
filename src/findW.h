#pragma once
#include "stateaware.h"

//internal functions
int _find_write_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util, int cur_b, int* w_lpas);
int __calc_invorder_mem(int pagenum, meta* metadata, long cur_lpa_timing, long workload_reset_time, int curfp);

//find writeblock functions
int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
int find_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas);
int find_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas, int write_start_idx);
int find_write_taskfixed(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
int find_write_hotness(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head,int* w_lpas,int idx);
int find_write_hotness_motiv(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int lpa, int policy);
int find_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, int flag);
int find_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long workload_reset_time);
