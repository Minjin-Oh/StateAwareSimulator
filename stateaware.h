#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

#define NOB 100
#define PPB 128
#define NOP NOB*PPB
#define OP 0.32

//locality param
#define SPLOCAL 0.05
#define TPLOCAL 0.95

//lifetime params
#define MAXPE 100
#define MARGIN 3 
#define THRESHOLD 10
#define OLD 0
#define YOUNG 1

//sorting
#define ASC 0
#define DES 1

//DEBUG and WL option toggle
//#define GCDEBUG
//#define DORELOCATE
//#define DOGCCONTROL
//profiles -- assume linear execution time change
#define STARTW 500
#define ENDW 350
#define STARTR 50
#define ENDR 200
#define STARTE 5000
#define ENDE 20000

#define STAMP 5
//structure definition
typedef struct _rttask{
    int idx;
    int rp;
    int rn;
    int wp;
    int wn;
    int gcp;
}rttask;


typedef struct _block{
    struct _block* next;
    struct _block* prev;
    int idx;
    int fpnum;
}block;

typedef struct _bhead{
    block* head;
    int blocknum;
}bhead;

typedef struct _meta{
    int pagemap[NOP];
    int rmap[NOP];
    int invnum[NOB];
    int invmap[NOP];
    int state[NOB];
    int access_window[NOB];
}meta;

//init_array
void init_metadata(meta* metadata);
bhead* init_blocklist(int start, int end);
void init_utillist(float* rutils, float* wutils, float* gcutils);
void init_task(rttask* task,int idx, int wp, int wn, int rp, int rn, int gcp);

//free
void free_metadata(meta* metadata);
void free_blocklist(bhead* head);

//simulator functions
block* write_simul(rttask task, meta* metadata, int* g_cur, 
                   bhead* fblist_head, bhead* write_head, bhead* full_head, 
                   block* cur_fb,int* total_fp, float* tracker);
void read_simul(rttask task, meta* metadata, float* tracker);

void gc_simul(rttask task, meta* metadata, 
              bhead* fblist_head, bhead* full_head, bhead* rsvlist_head,
              int* total_fp, float* tracker);

void wl_simul(meta* metadata, 
              bhead* fbhead, bhead* fullhead, bhead* hotlist, bhead* coldlist, 
              int vic1, int vic2, int* total_fp);

//utilization_calculate
float find_cur_util();
float find_worst_util(rttask* task, int tasknum, meta* metadata);
float find_SAworst_util(rttask* task, int tasknum, meta* metadata);
float calc_std(meta* metadata);
int util_check_main(); //test function for debugging(not used in simulation)

//expose some internal functions in util.c for other files
float w_exec(int cycle);
float r_exec(int cycle);
float e_exec(int cycle);
float __calc_gcmult(int wp, int wn, int MINRC);

//misc
int* add_checkpoints(int tasknum, rttask* tasks, int runtime, int* cps_size);

//linked-list
bhead* ll_init();
void ll_free(bhead* head);
block* ll_pop(bhead* head);
block* ll_append(bhead* head, block* new);
block* ll_remove(bhead* head, int tar);
block* ll_condremove(meta* metadata, bhead* head, int cond);
int idx_exist(bhead* head, int tar);

//hot cold seperation functions.
void build_hot_cold(meta* metadata, bhead* hotlist, bhead* coldlist, int max);
int get_blkidx_byage(meta* metadata, bhead* list, bhead* rsvlist_head, bhead* write_head, int param, int any);
int get_blockidx_meta(meta* metadata, int param);
int is_idx_in_list(bhead* head, int tar);

