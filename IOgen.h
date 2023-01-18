#pragma once
#include "stateaware.h"

//IO microbenchmark generation function
void IOgen(int tasknum, rttask* task, long runtime, int offset,float _splocal,float _tplocal);
void analyze_IO(FILE* fp);
int IOget(FILE* fp);
void IO_open(int tasknum, FILE** wfpp, FILE** rfpp);
void lat_open(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp);
void lat_close(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp);