#pragma once
#include "stateaware.h"
void find_RR_dualpool(rttask* task, int tasknum, meta* metadata, 
                      bhead* full_head, bhead* hotlist, bhead* coldlist, int* res1, int* res2);
void find_RR_target(rttask* tasks, int tasknum, meta* metadata, 
                    bhead* fblist_head, bhead* full_head, int* res1, int* res2);
void find_RR_target_util(rttask* tasks, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* full_head, int* res1, int* res2);
void find_RR_target_simple(rttask* tasks, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* full_head, int* res1, int* res2);
void find_WR_target_simple(rttask* tasks, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* full_head, int* res1, int* res2);
void find_BWR_victim_updatetiming(rttask* tasks, int tasknum, meta* metadata, 
                                 bhead* fblist_head, bhead* full_head, int* res);
void find_BWR_target_updatetiming(rttask* tasks, int tasknum, meta* metadata,
                                 bhead* write_head, int* res);
long find_RR_period(int v1, int v2, int vp1_cnt, int vp2_cnt, double rrutil, meta* metadata);

