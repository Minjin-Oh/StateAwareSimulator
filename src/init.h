#pragma once
#include "stateaware.h"

void init_metadata(meta* metadata, int tasknum, int cycle);
bhead* init_blocklist(int start, int end);
void init_utillist(float* rutils, float* wutils, float* gcutils);
void init_task(rttask* task,int idx, int wp, int wn, int rp, int rn, int gcp, int lb, int ub);
void free_metadata(meta* metadata);
void free_blocklist(bhead* head);