#pragma once
#include "stateaware.h"

void finish_WR(rttask* task, IO* cur_IO, meta* metadata);
void finish_GC(rttask* task, IO* cur_IO, meta* metadata);
void finish_GCER(rttask* task, IO* cur_IO, meta* metadata, 
                 bhead* fblist_head, bhead* rsvlist_head, GCblock* cur_GC);

//used by emul_main
void finish_req(rttask* task, IO* cur_IO, meta* metadata, 
                bhead* fblist_head, bhead* rsvlist_head, bhead* full_head,
                GCblock* cur_GC, RRblock* cur_RR);
long find_next_time(rttask* tasks, int tasknum, long cur_dl, long wl_dl, long cur_cp);
