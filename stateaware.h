#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

#define WR 1
#define RD 2
#define GC 3
#define RR 4

#define NOB 100
#define PPB 128
#define NOP NOB*PPB
#define OP 0.32

//locality param
#define SPLOCAL 0.05
#define TPLOCAL 0.95

//lifetime params
#define MINRC 36
#define MAXPE 100
#define MARGIN 3 
#define THRESHOLD 10
#define OLD 0
#define YOUNG -1

//sorting
#define ASC 0
#define DES 1

//GC and GCDEBUG option toggle  
//#define GCDEBUG                // DEBUG print option

//scheme option toggle
#define DOGC
//#define DORELOCATE

//baseline vs new scheme
#define DOGCCONTROL
//#define GCBASE

//#define DOWCONTROL
//#define WRITEBASE


#define DORRCONTROL
//#define RRBASE

//deprecated params
//#define DOWRCONTROL
//#define DOGCNOTHRES
//#define FORCEDCONTROL
//#define FORCEDNOTHRES

//profiles -- assume linear execution time change
#define STARTW 500
#define ENDW 350
#define STARTR 50
#define ENDR 200
#define STARTE 5000
#define ENDE 20000

#define STAMP 1
//structure definition
typedef struct _rttask{
    int idx;
    int rp;
    int rn;
    int wp;
    int wn;
    int gcp;
    int addr_lb;
    int addr_ub;
}rttask;


typedef struct _block{
    struct _block* next;
    struct _block* prev;
    int idx;
    int fpnum;
}block;

typedef struct _IO{
    int type;
    int lpa;
    int ppa;
    int gc_old_lpa;
    int gc_vic_ppa;
    int gc_tar_ppa;
    int rr_old_lpa;
    int rr_vic_ppa;
    int rr_tar_ppa;
    int vmap_task;
}IO;

typedef struct _GCBlock{
    block* cur_vic;
    block* cur_rsv;
}GCblock;

typedef struct _RRBlock{
    block* cur_vic1;
    block* cur_vic2;
    float execution_time;
}RRblock;

typedef struct _bhead{
    block* head;
    block* last;
    int blocknum;
}bhead;

typedef struct _meta{
    int pagemap[NOP];
    int rmap[NOP];
    int invnum[NOB];
    int invmap[NOP];
    int state[NOB];
    int access_window[NOB];
    int total_invalid;
    float** runutils;
    char** access_tracker;
    char vmap_task[NOP];
}meta;

//IO microbenchmark generating function.
void IOgen(int tasknum, rttask* task, int runtime, int offset);
int IOget(FILE* fp);
void IO_open(int tasknum, FILE** wfpp, FILE** rfpp);

//init_array
void init_metadata(meta* metadata, int tasknum);
bhead* init_blocklist(int start, int end);
void init_utillist(float* rutils, float* wutils, float* gcutils);
void init_task(rttask* task,int idx, int wp, int wn, int rp, int rn, int gcp, int lb, int ub);

//free
void free_metadata(meta* metadata);
void free_blocklist(bhead* head);

//simulator functions
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
float find_cur_util();
float find_worst_util(rttask* task, int tasknum, meta* metadata);
float find_SAworst_util(rttask* task, int tasknum, meta* metadata);
int find_gcctrl(rttask* task, int taskidx,int tasknum, meta* metadata,bhead* full_head);
int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head);
float calc_std(meta* metadata);
int util_check_main(); //test function for debugging(not used in simulation)

//expose some internal functions in util.c for other files
float w_exec(int cycle);
float r_exec(int cycle);
float e_exec(int cycle);
float __calc_gcmult(int wp, int wn, int _minrc);
float __calc_wu(rttask* task, int scale_w);
float __calc_ru(rttask* task, int scale_r);
float __calc_gcu(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e);

//misc
int* add_checkpoints(int tasknum, rttask* tasks, int runtime, int* cps_size);

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

//hot cold seperation functions.
void build_hot_cold(meta* metadata, bhead* hotlist, bhead* coldlist, int max);
int get_blkidx_byage(meta* metadata, bhead* list, bhead* rsvlist_head, bhead* write_head, int param, int any);
int get_blockstate_meta(meta* metadata, int param);
int is_idx_in_list(bhead* head, int tar);
void find_RR_target(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2);

//lpsolver
int find_writectrl_lp(rttask* tasks, int tasknum, meta* metadata, double margin,int low, int high);

//refactored simulation functions
block* write_job_start(rttask* tasks, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, bhead* write_head,
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

//checker & profiler
FILE* open_file_bycase(int gcflag, int wflag, int rrflag);
float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                    int yng, int old,int cur_cp,int cur_gc_idx,int cur_gc_state);
void check_profile(float tot_u, meta* metadata, rttask* tasks, int tasknum, int cur_cp, FILE* fp, FILE* fplife);