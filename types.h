#define WR 1
#define RD 2
#define GC 3
#define RR 4
#define GCER 5
#define RRRE 6
#define RRWR 7
#define RRER 8

#define NOB 100
#define PPB 128
#define NOP NOB*PPB
#define _OP 0.32

//locality param
#define SPLOCAL 0.05
#define TPLOCAL 0.95

//lifetime params
#define _MINRC 35
#define MAXPE 100
#define MARGIN 3
#define THRESHOLD 10
#define OLD 0
#define YOUNG -1
#define INITCYC 0
//sorting
#define ASC 0
#define DES 1

//GC and GCDEBUG option toggle  
//#define GCDEBUG                // DEBUG print option

//scheme option toggle
#define DOGC
#define DORELOCATE

//baseline vs new scheme(deprecated)
//#define DOGCCONTROL
//#define GCBASE
//#define DOWCONTROL
//#define WRITEBASE
//#define DORRCONTROL
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

typedef struct _meta{
    int pagemap[NOP];
    int rmap[NOP];
    int invnum[NOB];
    int invmap[NOP];
    int state[NOB];
    int access_window[NOB];
    int read_cnt[NOP];
    int* read_cnt_task;
    int tot_read_cnt;
    int write_cnt[NOP];
    int* write_cnt_task;
    int tot_write_cnt;
    int EEC[NOB];
    int tot_gc_cnt;
    int total_invalid;
    int total_fp;
    float** runutils;
    char** access_tracker;
    int* cur_read_worst;
    char vmap_task[NOP];

}meta;
