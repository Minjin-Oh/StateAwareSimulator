#pragma once
#include "stateaware.h"

//IO microbenchmark generation function
void IOgen(int tasknum, rttask* task, long runtime, int offset,float _splocal,float _tplocal);
void analyze_IO(FILE* fp);
int IOget(FILE* fp);
void IO_open(int tasknum, FILE** wfpp, FILE** rfpp);
void lat_open(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp);
void lat_close(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp);
void reset_IO_update(meta* metadata, int lpa_lb, int lpa_ub, long IO_offset);
void IO_timing_update(meta* metadata, int lpa, int wcount, long offset);