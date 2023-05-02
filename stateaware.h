#pragma once
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "types.h"

int util_check_main(); //test function for debugging(not used in simulation)

//expose internal functions in util.c for other files
float w_exec(int cycle);
float r_exec(int cycle);
float e_exec(int cycle);
int __calc_gcmult(int wp, int wn, int _minrc);
float __calc_wu(rttask* task, int scale_w);
float __calc_ru(rttask* task, int scale_r);
float __calc_gcu(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e);
int _gc_period(rttask* task,int _minrc);

//flag getting functions
void set_scheme_flags(char* argv[], 
                      int *gcflag, int *wflag, int *rrflag, int *rrcond);
void set_exec_flags(char* argv[], int *tasknum, float *totutil, 
                    int *genflag, int* taskflag,
                    int *skewness, float* sploc, float* tploc, int* skewnum,
                    int *OPflag, int *cyc, double *OP, int *MINRC);


//simulator functions(deprecated)
block* write_simul(rttask task, meta* metadata, int* g_cur, 
                   bhead* fblist_head, bhead* write_head, bhead* full_head, 
                   block* cur_fb,int* total_fp, float* tracker, FILE* fp_w, int write_limit);
void read_simul(rttask task, meta* metadata, float* tracker, int offset, FILE* fp_r);

void gc_simul(rttask task, int tasknum, meta* metadata, 
              bhead* fblist_head, bhead* full_head, bhead* rsvlist_head,
              int* total_fp, float* tracker, int gc_limit, int write_limit, int* targetblockhistory);

void wl_simul(meta* metadata, int tasknum,
              bhead* fbhead, bhead* fullhead, bhead* hotlist, bhead* coldlist, 
              int vic1, int vic2, int* total_fp);

//utilization_calculate
float find_worst_util(rttask* task, int tasknum, meta* metadata);
float find_cur_util(rttask* tasks, int tasknum, meta* metadata, int old);
int find_util_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util);
float find_SAworst_util(rttask* task, int tasknum, meta* metadata);
int _find_min_period(rttask* task,int tasknum);
float calc_std(meta* metadata);

//misc
int* add_checkpoints(int tasknum, rttask* tasks, long runtime, int* cps_size);

//linked-list
bhead* ll_init();
void ll_free(bhead* head);
block* ll_pop(bhead* head);
block* ll_append(bhead* head, block* new);
block* ll_remove(bhead* head, int tar);
block* ll_findidx(bhead* head, int tar);
block* ll_find(meta* metadata, bhead* head, int cond);
block* ll_condremove(meta* metadata, bhead* head, int cond);
int idx_exist(bhead* head, int tar);

//linked-list(IOqueue)
IOhead* ll_init_IO();
void ll_free_IO(IOhead* head);
void ll_append_IO(IOhead* head, IO* new);
IO* ll_pop_IO(IOhead* head);

//hot cold seperation functions.
void build_hot_cold(meta* metadata, bhead* hotlist, bhead* coldlist);
int get_blkidx_byage(meta* metadata, bhead* list, bhead* full_head, int param, int any);
int get_blockstate_meta(meta* metadata, int param);
int is_idx_in_list(bhead* head, int tar);


//lpsolver
int find_writectrl_lp(rttask* tasks, int tasknum, meta* metadata, double margin,int low, int high);

//write block assignment functions 

//refactored simulation functions
block* write_job_start(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IO* IOqueue, block* cur_target, int wflag);
void write_job_end(rttask task, meta* metadata, IO* IOqueue, int* total_fp);
void read_job_start(rttask task, meta* metadata, FILE* fp_r, IO* IOqueue);
void read_job_end(rttask task,meta* metadata, IO* IOqueue);
void gc_job_start(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, 
                  int write_limit, IO* IOqueue, GCblock* cur_GC,int gcflag);
void gc_job_end(rttask* tasks, int taskidx, int tasknum, meta* metadata, IO* IOqueue,
                bhead* fblist_head, bhead* rsvlist_head,
                GCblock* cur_GC, int* total_fp);
void RR_job_start(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, 
                  IO* IOqueue, RRblock* cur_RR);
void RR_job_end(meta* metadata, bhead* fblist_head, bhead* full_head, 
                  IO* IOqueue, RRblock* cur_RR, int* total_fp);

//emulation functions
void read_job_start_q(rttask* task, int taskidx, meta* metadata, FILE* fp_r, IOhead* rq, long cur_cp);
block* write_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp);
void gc_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, bhead* write_head,
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag, long cur_cp);
void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, bhead* hotlist, bhead* coldlist,
                  IOhead* rrq, RRblock* cur_RR, double rrutil, long cur_cp);

//file open 
FILE* open_file_bycase(int gcflag, int wflag, int rrflag);
FILE* open_file_pertask(int gcflag, int wflag, int rrflag, int tasknum);
void open_files_misc(FILE* fplife, FILE* fpwrite, FILE* fpread, FILE* fprr);
void update_read_worst(meta* metadata, int tasknum);

//expected value calculator for each design
float calc_readlatency(rttask* tasks, meta* metadata, int taskidx);
float calib_readlatency(meta* metadata, int taskidx, float cur_exp_lat, int old_ppa, int new_ppa);
float calc_weightedread(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas);
float calc_weightedgc(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas, int w_start_idx, float OP);

//profiler
float get_totutil(rttask* tasks, int tasknum, int taskidx, meta* metadata, int old);
float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state, block* cur_wb, bhead* fblist_head, bhead* write_head, int getfp,int gcvalidcount);
float print_profile_best(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state);
void check_profile(float tot_u, meta* metadata, rttask* tasks, int tasknum, long cur_cp, FILE* fp, FILE* fplife);
void check_block(float tot_u, meta* metadata, rttask* tasks, int tasknum, long cur_cp, FILE* fp, FILE* fplife);
void print_hotdist_profile(FILE* fp, rttask* tasks, int cur_cp, meta* metadata, int taskidx, int hotness_rw);
void print_freeblock_profile(FILE* fp, int cur_cp, meta* metadata, bhead* fblist_head, bhead* write_head);
void print_fullblock_profile(FILE* fp, long cur_cp, meta* metadata, bhead* full_head);
void print_invalid_profile(FILE* fp, int cur_cp, meta* metadata);
void print_writeblock_profile(FILE* fp, long cur_cp, meta* metadata, bhead* fblist_head, bhead* write_head, int write_idx, int target_idx, int type, int rank, float proportion, int bidx, int candnum);
void print_profile_updaterate(meta* metadata, FILE* updaterate_fp);
float print_profile_timestamp(rttask* tasks, int tasknum, meta* metadata, FILE* fp, int yng, int old,long cur_cp);

//gen_task
rttask* generate_taskset(int tasknum, float util, int addr, float* result_util, int cycle);
rttask* generate_taskset_skew(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle);
rttask* generate_taskset_skew2(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle);
rttask* generate_taskset_hardcode(int tasknum, int addr);
rttask* generate_taskset_fixed(int addr, float* result_util);
void get_task_from_file(rttask* tasks, int tasknum, FILE* taskfile);
void get_loc_from_file(rttask* tasks, int tasknum, FILE* locfile);
void randtask_statechecker(int tasknum,int addr);

//a proportion profiler for lpas
void _find_rank_lpa(rttask* tasks, int tasknum);

//util.c functions
double find_max_double(double a, double b, double c);