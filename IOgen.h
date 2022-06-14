#pragma once
#include "stateaware.h"

//IO microbenchmark generation function
void IOgen(int tasknum, rttask* task, long runtime, int offset,float _splocal,float _tplocal);
int IOget(FILE* fp);
void IO_open(int tasknum, FILE** wfpp, FILE** rfpp);