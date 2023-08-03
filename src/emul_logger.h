#pragma once
#include "stateaware.h"

int check_latency(FILE** lat_log_w, FILE** lat_log_r, FILE** lat_log_gc, IO* cur_IO, long cur_cp);
int check_dl_violation(rttask* tasks, IO* cur_IO, long cur_cp);