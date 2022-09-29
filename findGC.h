#pragma once
#include "stateaware.h"

int find_gcctrl(rttask* task, int taskidx,int tasknum, meta* metadata,bhead* full_head);
int find_gcctrl_greedy(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head);
int find_gcctrl_yng(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head);

int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
int find_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas);