#pragma once
#include "stateaware.h"

int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
int find_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas);
int find_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas, int write_start_idx);
int find_write_taskfixed(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
int find_write_hotness(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head,int lpa);