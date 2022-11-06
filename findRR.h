#pragma once
#include "stateaware.h"
void find_RR_dualpool(rttask* task, int tasknum, meta* metadata, 
                      bhead* full_head, bhead* hotlist, bhead* coldlist, int* res1, int* res2);
void find_RR_target(rttask* tasks, int tasknum, meta* metadata, 
                    bhead* fblist_head, bhead* full_head, int* res1, int* res2);
void find_RR_target_util(rttask* tasks, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* full_head, int* res1, int* res2);
long find_RR_period(int v1, int v2, int vp1_cnt, int vp2_cnt, double rrutil, meta* metadata);
