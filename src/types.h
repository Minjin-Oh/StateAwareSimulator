#define WR 1
#define RD 2
#define GC 3
#define RR 4
#define GCER 5
#define RRRE 6
#define RRWR 7
#define RRER 8
#define BWR 9

//differentiate GCTHRESNOB and NOB
//normally, GCTHRESNOB == NOB
//to trigger GC earlier, GCTHRESNOB < NOB
#define GCTHRESNOB 1000
#define NOB 1000
#define PPB 128
#define NOP NOB*PPB
#define _OP 0.32        //!!!deprecated. MINRC and oprate is defined as variable in parse.c!!!

//locality param
#define SPLOCAL 0.05
#define TPLOCAL 0.95

//workload duration and simulation maximum runtime (in microsecond)
#define WORKLOAD_LENGTH 160000000000L
#define RUNTIME 10000000000000L

//set number of operation types (used in execution time functions)
//currently 3 (write,read,erase) 
#define OPTYPENUM 3


//lifetime params
#define _MINRC 35       //!!!deprecated. MINRC and oprate is defined as variable in parse.c!!!
#define MAXPE 2000
#define MARGIN 3
#define THRESHOLD 10
#define GCGROUP_THRES 40
#define OLD 0
#define YOUNG -1
#define INITCYC 0
#define LIFESPAN_WINDOW 3000
//sorting
#define ASC 0
#define DES 1

//GC and GCDEBUG option toggle  
//#define GCDEBUG                // DEBUG print option

//maybe deprecated?
#define DOGC
#define DORELOCATE

//scheme option toggle
#define IOTIMING                    //updates timing for each LPA at init & write req init
#define TIMING_ON_MEM               //loads update timing info on memory
//#define TASKGEN_IGNORE_UTILOVER     //generates taskset with WCutil over 1.0
#define UUNIFAST
//#define GC_ON_WRITEBLOCK          //redirects GC on write block & removes rsv block
//#define MAXINVALID_RANK_FIXED     //assign write pages strictly aligned with invalidation order
//#define MAXINVALID_RANK_STAT      //assign write pages w.r.t offline profile (use K-means in offline)
#define MAXINVALID_RANK_DYN         //assign write pages w.r.t dynamically changing criteria.
#define UTILSORT_BEST               //make find_gc_utilsort function to pick a block with shortest exec time
#define EXECSTEP                    //make execution time to increase in step function
//deprecated params
//#define DOGCCONTROL
//#define GCBASE
//#define DOWCONTROL
//#define WRITEBASE
//#define DORRCONTROL
//#define RRBASE
//#define DOWRCONTROL
//#define DOGCNOTHRES
//#define FORCEDCONTROL
//#define FORCEDNOTHRES

//profiles -- assume linear execution time change
#define STARTW 713
#define ENDW 663
#define STARTR 280
#define ENDR 600
#define STARTE 3500
#define ENDE 14000

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
    float sploc;
    float tploc;
}rttask;


typedef struct _block{
    struct _block* next;
    struct _block* prev;
    int idx;
    int fpnum;
    int wb_rank;
    long lb;
    long ub;
}block;

typedef struct _IO{
    struct _IO* next;
    struct _IO* prev;
    int type;           //IO type
    int taskidx;
    int lpa;            //write or read info
    int ppa;
    int gc_old_lpa;     //address information for GC
    int gc_vic_ppa;
    int gc_tar_ppa;
    int rr_old_lpa;     //address information for RR
    int rr_vic_ppa;
    int rr_tar_ppa;
    int vmap_task;      //record of I/O origin(task number)
    long deadline;      //I/O deadline
    long exec;          //exec time of req
    int vic_idx;        //update this for GC req & WL req
    int rsv_idx;        //update this for GC req
    int tar_idx;        //update this for WL req
    int gc_valid_count; //carries number of valid page for GC
    int rr_valid_count; //carries number of valid page for WL
    int islastreq;      //checks if this is final req of current job
    int isrrfinish;     //checks if this is final req for RR;
    char init;          //set this 1 if req is initial I/O req (WR, RD)
    char last;          //set this 1 if req is last I/O req (WR, RD)
    long IO_start_time;
    long IO_end_time;
    int order;
    block* IO_dest_block;
}IO;

typedef struct _IOhead{
    IO* head;
    block* last;
    int reqnum;
}IOhead;

typedef struct _GCBlock{
    block* cur_vic;
    block* cur_rsv;
}GCblock;

typedef struct _RRBlock{
    block* cur_vic1;
    block* cur_vic2;
    float execution_time;
    int vic1_acc;
    int vic2_acc;
    long cur;
    long period;
    long rrcheck;         //emul_main use this value as absolute deadline
}RRblock;

typedef struct _bhead{
    block* head;
    block* last;
    int blocknum;
}bhead;

typedef struct _currankinfo{
    int* cur_left_write;
    int* tot_ranked_write;
    int** ranks_for_write;
    long** timings_for_write;
}currankinfo;

typedef struct _meta{
    int pagemap[NOP];
    int rmap[NOP];
    int invmap[NOP];
    int read_cnt[NOP];
    int write_cnt[NOP];
    int write_cnt_per_cycle[NOP];
    long avg_update[NOP];
    long recent_update[NOP];
    long next_update[NOP];
    char vmap_task[NOP];
    int invnum[NOB];
    int state[NOB];
    int access_window[NOB];             //number of read counts per physical flash block.
    int invalidation_window[NOB];       //number of update(invalidation) counts per physical flash block.
    int EEC[NOB];
    long GC_locktime[NOB];
    int* read_cnt_task;
    int tot_read_cnt;
    int* write_cnt_task;
    int tot_write_cnt;
    int tot_gc_cnt;
    int total_invalid;
    int total_fp;
    int reserved_write;
    float** runutils;
    char** access_tracker;
    int* cur_read_worst;
    long* rewind_time_per_task;
    int ranknum;
    long* rank_bounds;
    currankinfo cur_rank_info;
    int left_rankwrite_num;
    int lifespan_record_num;
    int* rank_write_count;
    long data_lifespan[LIFESPAN_WINDOW];
}meta;

typedef struct _prof_exec{
    int* pe_steps;
    int** pe_values;
    int** pe_thres;
}prof_exec;
