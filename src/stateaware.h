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

/* ============================================================================
 * PhysicalLatencyModel — ground truth latency, PEC-dependent per Fig. 2.
 * ============================================================================
 * Callers: simulation engine only (IOsim_q.c, rrsim_q.c setting req->exec;
 * runutils[] accumulation of admitted cost; state-aware overflow check in
 * logger.c). NEVER call these from controller decision paths — that would
 * leak physics into the model the controller is not supposed to see.
 */
float w_exec_phys(int cycle);
float r_exec_phys(int cycle);
float e_exec_phys(int cycle);

int __calc_gcmult(int wp, int wn, int _minrc);
int _gc_period(rttask* task,int _minrc);

/* Legacy state-aware helpers: __calc_wu/ru/gcu use PhysicalLatencyModel
 * directly. Retained for (a) find_worst_util diagnostic snapshot in
 * logger.c and (b) gen_task.c task-generation planning at cycle 0.
 * Do NOT wire into controller decision code — use *_assumed variants. */
float __calc_wu(rttask* task, int scale_w);
float __calc_ru(rttask* task, int scale_r);
float __calc_gcu(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e);

/* ============================================================================
 * AssumedLatencyModel — controller-facing exec times.
 * ============================================================================
 * Dispatches on latency_mode:
 *   LATENCY_MODE_STATE      : identical to PhysicalLatencyModel  (LaWL, proposal)
 *   LATENCY_MODE_FIXED_BOL  : t_op(cycle) collapsed to t_op(0)   (fresh-block)
 *   LATENCY_MODE_FIXED_EOL  : t_op(cycle) collapsed to t_op(MAXPE) (worn-block)
 *   LATENCY_MODE_LAWL_OPT/AVG/PES : Table II constants @ PEC=0 / 1000 / 2000
 * Callers: controller / admission logic ONLY (findGC.c, findW.c, findRR.c,
 * assignW.c). NEVER call these from sim engine paths that model actual
 * completion time.
 */
#define LATENCY_MODE_STATE      0
#define LATENCY_MODE_FIXED_BOL  1
#define LATENCY_MODE_FIXED_EOL  2
#define LATENCY_MODE_LAWL_OPT   3
#define LATENCY_MODE_LAWL_AVG   4
#define LATENCY_MODE_LAWL_PES   5
extern int latency_mode;

float w_exec_assumed(int cycle);
float r_exec_assumed(int cycle);
float e_exec_assumed(int cycle);
float __calc_wu_assumed(rttask* task, int scale_w);
float __calc_ru_assumed(rttask* task, int scale_r);
float __calc_gcu_assumed(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e);
float find_worst_util_assumed(rttask* task, int tasknum, meta* metadata);
float find_cur_util_assumed(rttask* tasks, int tasknum, meta* metadata, int old);
int   find_util_safe_assumed(rttask* tasks, int tasknum, meta* metadata, int old,
                             int taskidx, int type, float util);
/* ============================================================================ */

//flag getting functions
void set_scheme_flags(char* argv[],
                      int *gcflag, int *wflag, int *rrflag, int *rrcond);
// [FIXED-LATENCY] added lat_mode out-param at tail (parses argv[12])
void set_exec_flags(char* argv[], int *tasknum, float *totutil,
                    int *genflag, int* taskflag, int* profflag,
                    int *skewness, float* sploc, float* tploc, int* skewnum,
                    int *OPflag, int *cyc, double *OP, int *MINRC, int *lat_mode);

//utilization_calculate
float find_worst_util(rttask* task, int tasknum, meta* metadata);
float find_cur_util(rttask* tasks, int tasknum, meta* metadata, int old);
int find_util_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util);
float find_SAworst_util(rttask* task, int tasknum, meta* metadata);
int _find_min_period(rttask* task,int tasknum);
float calc_std(meta* metadata);

//linked-list
bhead* ll_init();
block* ll_pop(bhead* head);
block* ll_append(bhead* head, block* new);
block* ll_remove(bhead* head, int tar);
block* ll_findidx(bhead* head, int tar);
block* ll_find(meta* metadata, bhead* head, int cond);
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


//emulation functions
void read_job_start_q(rttask* task, int taskidx, meta* metadata, FILE* fp_r, IOhead* rq, long cur_cp);
block* write_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp);
void gc_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, bhead* write_head,
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag, long cur_cp);
// signature aligned with the actual definition in IOsim_q.c and the sole
// call site in emul_main.c (the earlier declaration listed skewnum/T_reloc/
// U_slack params that no longer exist in the implementation).
void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, bhead* hotlist, bhead* coldlist,
                    IOhead* rrq, RRblock* cur_RR, double rrutil, long cur_cp);

//file open 
// FILE* open_file_bycase(int gcflag, int wflag, int rrflag);
FILE* open_file_bycase(int gcflag, int wflag, int rrflag, const char* log_dir);
FILE* open_file_pertask(int gcflag, int wflag, int rrflag, int tasknum);
void open_files_misc(FILE* fplife, FILE* fpwrite, FILE* fpread, FILE* fprr);
void update_read_worst(meta* metadata, int tasknum);

//expected value calculator for each design
float calc_readlatency(rttask* tasks, meta* metadata, int taskidx);
float calib_readlatency(meta* metadata, int taskidx, float cur_exp_lat, int old_ppa, int new_ppa);
float calc_weightedread(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas);
float calc_weightedgc(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas, int w_start_idx, float OP);

//profiler
float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state, block* cur_wb, bhead* fblist_head, bhead* write_head, int getfp,int gcvalidcount);
void print_profile_updaterate(meta* metadata, FILE* updaterate_fp);
float print_profile_timestamp(rttask* tasks, int tasknum, meta* metadata, FILE* fp, int yng, int old,long cur_cp);
void print_gc_valid(FILE* fp, long cur_cp, int taskidx, int vic_idx, int block_state, int gc_valid_count);

//gen_task
rttask* generate_taskset(int tasknum, float util, int addr, float* result_util, int cycle);
rttask* generate_taskset_skew(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle);
rttask* generate_taskset_skew2(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle);
rttask* generate_taskset_hardcode(int tasknum, int addr, float* result_util);
rttask* generate_taskset_hardcode_motiv(int tasknu, float tot_util, int addr, float* result_util, int cycle);
rttask* generate_taskset_fixed(int addr, float* result_util);
void get_task_from_file(rttask* tasks, int tasknum, FILE* taskfile);
void get_loc_from_file(rttask* tasks, int tasknum, FILE* locfile);

//a proportion profiler for lpas
void _find_rank_lpa(rttask* tasks, int tasknum);

//util.c functions
double find_max_double(double a, double b, double c);
long get_gc_locktime(meta* metadata, int blockidx);
void print_blocklist_info(bhead* head, meta* metadata);
void print_fullblock_info(meta* metadata, bhead* head, long cur_cp, FILE* fp);
void print_maxinvalidation_block(meta* metadata, int blockidx);
int find_block_in_list(meta* metadata, bhead* head, int cond);

//misc.c functions
int compare(const void *a, const void *b);



//////////////////DEPRECATED//////////////////////////////////

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

//lpsolver
int find_writectrl_lp(rttask* tasks, int tasknum, meta* metadata, double margin,int low, int high);
