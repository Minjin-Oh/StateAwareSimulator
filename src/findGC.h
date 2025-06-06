#pragma once
#include "stateaware.h"
#include "findW.h"

int find_gcctrl(rttask* task, int taskidx,int tasknum, meta* metadata,bhead* full_head);
int find_gcctrl_greedy(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head);
int find_gcctrl_yng(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head);
int find_gcctrl_limit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head);
int find_gcweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head);
int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head);
int find_gc_test(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head);
block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head);