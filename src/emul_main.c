#include "stateaware.h"     // contains
#include "init.h"           // contains init function for various structure params
#include "emul.h"           // contains request process functions for emulation
#include "findRR.h"         // contains block selection functions
#include "findW.h"          // compute_shadow_write() for §C1 shadow eval
#include "IOgen.h"          // contains random workload generation functions
#include "emul_logger.h"    // contains latency logger functions
#include "ovhd_stats.h"     // decision-overhead distribution instrumentation
#include "shadow_stats.h"   // g_shadow_write / g_shadow_gc for C1/C2 divergence
#include <unistd.h>         // sleep()
#include <math.h>           // sqrt() for end-of-run PEC-distribution stats
#include <zlib.h>           // gzFile / gzopen / gzprintf / gzclose — util_fp is gz-compressed

/* LaWL-S invocation period (Sec. VI-B: "LaWL-S is invoked every T_reloc").
 * Gates the WL admission evaluation so it is attempted once per T_reloc
 * instead of after every write-job completion (do_rr polling artifact).
 * 500 ms: same period as the clustering-boundary update. */
#define TRELOC 500000L

/* Force-background RR scheme (parse.c: argv[3]=="BGRR"). When set, LaWL-S
 * admission bypasses the slack check and unconditionally queues RR at the
 * lowest priority. Kept as an opt-in scheme; all other rrcond values run
 * the paper's slack-based admission (Eq. 13). Mirrors IOsim_q.c. */
#define RRCOND_BGRR 7

//globals
block* cur_fb = NULL;
int rrflag = 0;
/* Promoted from main-local so IOsim_q.c's RR_job_start_q can gate on it
 * (see RRCOND_BGRR = 7 = BGRR "force background" mode from parse.c). */
int rrcond = 0;
/* Cached extrema of metadata->state[] across all NOB blocks.
 * state[] is mutated only at emul.c:finish_GCER (state[vicidx]++), so the
 * cache is valid until the next GC erase completes. Cleared by finish_GCER
 * via state_cache_dirty and recomputed once per outer-loop iteration when
 * dirty. Replaces two per-iteration get_blockstate_meta() calls plus the
 * inline MAXPE scan (previously 3 * NOB=1000 int compares every tick). */
int state_cache_dirty = 1;
int cached_yngest = 0;
int cached_oldest = 0;
int cached_maxpe_idx = -1;   /* first idx observed at state[]>=MAXPE, else -1 */
int MINRC;              //minimum reclaimable page, assigned by set_exec_flags in parse.c
double OP;              //overprovisioning rate, assigned by set_exec_flags in parse.c
int THRES_COLD = 35;    //a global threshold for write-cold block determination, used in find_WR_target_simple() in findRR.c
int THRES_HOT = 300;
int prev_erase = 0;       //a flag for interval specification of threshold update, used in find_WR_target_simple() in findRR.c
int prev_mincyc = 0;      //a parameter for minimum cycle block comparison, used in find_WR_target_simple() in findRR.c
int prev_cyc[NOB] = {0, };

#ifdef EXECSTEP
    prof_exec exec_steps;
#endif

//FIXME::set these as global to expose to find_util_safe function
IOhead** wq;
IOhead** rq;
IOhead** gcq;

//FIXME:: set these as global to expose to find_write_gradient function
float sploc;
float tploc;
int offset;

//FIXME:: set these as global to expose proportion array to find_write_gradient function
double* w_prop;
double* r_prop;
double* gc_prop;

//FIXME:: set these as global to expose workloads to find_write_invalid function
FILE** w_workloads;
FILE** r_workloads;

//FIXME:: set these as global to expose log file pointers and system parameters
long cur_cp;
int max_valid_pg;
int tot_longlive_cnt = 0;
FILE **fps;
FILE *test_gc_writeblock[4];
FILE *updaterate_fp;
FILE *longliveratio_fp;
FILE *updateorder_fp;
FILE *getupdateorder_fp;
FILE *u_check;
FILE *gc_valid_fp;  // per-GC valid-copy log

//FIXME:: set these as global to expose global write block to assign_write_invalid function
bhead* glob_yb;
bhead* glob_ob;

//a space to store lpa update timing (memory)
long* lpa_update_timing[NOP];
int update_cnt[NOP];
int cur_length[NOP];
int init_length = 10;

int main(int argc, char* argv[]){
    //init params
    srand(time(NULL)); 
    bhead* fblist_head = NULL;                             //heads for block list
    bhead* rsvlist_head;
    bhead* full_head;
    bhead* write_head;
    bhead* hotlist;                                 //(for WL) overlaps previous blocklist
    bhead* coldlist;
    meta* newmeta = (meta*)malloc(sizeof(meta));    //metadata structure
    
    //initialize flag variables
    int gcflag = 0;
    int wflag = 0;
    /* rrcond is a file-scope global (set by set_scheme_flags below). */
    int tasknum;
    int genflag = 0;
    int taskflag = 0;
    int profflag = 0;
    int skewness;                    //skew of utilization(-1 = noskew, 0 = read-skewed, 1 = write-skewed)
    int skewnum;                     //number of skewed task
    int OPflag;
    int init_cyc = 0;
    int lat_mode = 0;                // argv[12]: 0=STATE 1=FIXED_S 2=FIXED_E 3=LAWL_OPT 4=LAWL_AVG 5=LAWL_PES
    float totutil;                   //a total utilization of current system
    //get flags
    set_scheme_flags(argv,
                     &gcflag, &wflag, &rrflag, &rrcond);
    set_exec_flags(argv, &tasknum, &totutil,
                   &genflag, &taskflag, &profflag,
                   &skewness, &sploc, &tploc, &skewnum,
                   &OPflag, &init_cyc, &OP, &MINRC, &lat_mode);
    // [FIXED-LATENCY] publish parsed mode to the util.c global consulted by
    // every _dec helper. Must happen before any decision-path call executes.
    latency_mode = lat_mode;
#ifdef EXECSTEP
    init_prof_exec(&(exec_steps));
#endif
    //initialize misc variables
    char IO_end_bwr_flag = 0;       //notify that cur_cp is end of I/O req
    char qempty_bwr_flag = 1;       //notify that other queue is empty
    char wr_end_bwr_flag = 0;       //notify that cur_cp is end of write job.
    char task_gen_success = 0;      //notify that task generation was successful.
    int g_cur = 0;                   //pointer for current writing page @ dummy write phase
    int wl_init = 0;                 //flag for wear-leveling initiation
    int rr_finished = 1;
    int hot_cold_list = 0;
    int do_rr = 0;
    long next_rr_check = 0;          //next simulated time WL admission may be evaluated (T_reloc gate)
    long cur_IO_end = __LONG_MAX__;  //absolute time when current req finishes
    int  util_overflow_active = 0;   //rising-edge guard: log once per >=1.0 episode
    long util_overflow_events = 0;   //cumulative episode count for end-of-run summary
    long dl_miss_events       = 0;   //cumulative deadline-miss count (per last-req IO)
    float WU;                        //worst-case utilization tracker.
    float rrutil;                    //a utilization allowed to data relocation job
    cur_cp = 0;                      //current checkpoint time
    
    //log file pointers — NULL-init so unmatched (wflag,gcflag,rrflag,latency_mode)
    //combos don't leak an uninitialized pointer into later fprintf calls.
    FILE* rr_profile = NULL;
    FILE *fp = NULL, *fplife = NULL, *fpwrite = NULL, *fpread = NULL, *fprr = NULL, *fpovhd = NULL;
    gzFile util_fp = NULL;           // per-IO-event trace, gzip-streamed to keep disk bounded
    /* [C3] per-invocation LaWL-S admission trace + one-shot end-of-run summary.
     * Opened only for rrflag != -1 schemes (LaWL family + Hybrid); Baseline /
     * LaWL-D never call the admission block so admit metrics are undefined.
     * Trace is gzip-compressed: hundreds of thousands of rows per taskset. */
    gzFile fpadmit = NULL;
    FILE  *fpadmit_sum = NULL;
    /* [C1/C2 SHADOW] per-decision shadow-evaluation traces. Opened for every
     * scheme that runs a write / GC decision (all except pure Baseline that
     * bypasses INVW+UTILGC). Shadow rows are only written when the mode's
     * decision hit the instrumented path (g_shadow_*.valid == 1). Gzipped:
     * one row per write/GC job, easily millions per taskset. */
    gzFile fpshadow_w = NULL;   /* §C1 write-block divergence */
    gzFile fpshadow_g = NULL;   /* §C2 GC-victim divergence */
    /* [C4] per-job release/completion trace + end-of-run summary.
     * Enables post-hoc computation of Δ_unsafe + FN/FP confusion matrix
     * (§C4 & §3.4(c)(d)). Opened for every scheme so LaWL / opt / avg / pes /
     * baseline / dyn / hybrid can be compared side-by-side. Gzipped. */
    gzFile fpjobs     = NULL;
    FILE  *fpjobs_sum = NULL;

    /* Per-taskset iteration index. Files that record per-event traces
     * (utilization, admit, shadow_*, jobs) must NOT be appended across
     * tasksets — each run generates a new event stream that overwrites the
     * previous if we open in "a"/"ab" mode. run_simul.sh exports SIM_ITER=$i
     * so we can suffix these files with _<iter> and open in "w"/"wb" mode.
     * Aggregate summaries (lifetime, overhead, *_summary) keep "a"/"ab"
     * because they record ONE ROW per iter and multi-iter accumulation is
     * the whole point. When SIM_ITER is unset (single-shot run) the suffix
     * is empty. */
    char iter_suffix[32] = "";
    {
        const char* __ie = getenv("SIM_ITER");
        if(__ie && __ie[0]){
            snprintf(iter_suffix, sizeof(iter_suffix), "_%s", __ie);
        }
    }
    /* Filename builder for per-iter event traces. Uses a local scratch
     * buffer (128 is safe for our names). Opens in "wb" so multi-iter
     * sweeps don't accidentally concatenate distinct taskset streams;
     * SIM_ITER differentiates them, and running without SIM_ITER (single-
     * shot mode) simply overwrites the same file on each run. All per-iter
     * traces are gzip-compressed — millions of rows per taskset make
     * plain-text CSV impractical (§C1 shadow_write, §C2 shadow_gc,
     * §C3 admit, §C4 jobs). */
    #define OPEN_ITER_GZ(fp, base) do { \
        char __p[128]; \
        snprintf(__p, sizeof(__p), "%s%s.csv.gz", (base), iter_suffix); \
        (fp) = gzopen(__p, "wb"); \
    } while(0)
    FILE* lat_log_w[tasknum];
    FILE* lat_log_r[tasknum];
    FILE* lat_log_gc[tasknum];
    //IO file pointer init
    w_workloads = (FILE**)malloc(sizeof(FILE*)*tasknum);
    r_workloads = (FILE**)malloc(sizeof(FILE*)*tasknum);
    
    block *cur_wb[tasknum];
    GCblock cur_GC[tasknum];
    RRblock cur_rr;

    rttask* rand_tasks = NULL;
    rttask* tasks = NULL;
    w_prop = (double*)malloc(sizeof(double)*tasknum*2);
    r_prop = (double*)malloc(sizeof(double)*tasknum*2);
    gc_prop = (double*)malloc(sizeof(double)*tasknum*2);
    
    //simulate I/O (init related params)
    float total_u;
    int oldest;
    int yngest;
    int over_avg = 0;

    IO* cur_IO = NULL;
    wq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    rq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    gcq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    IOhead* rr;
    IOhead* bwr;
    
    long next_w_release[tasknum];
    long next_r_release[tasknum];
    long next_gc_release[tasknum];
    char wjob_finished[tasknum];
    char rjob_finished[tasknum];
    char gcjob_finished[tasknum];
    char wjob_deferred[tasknum];
    long releasetime_deferred[tasknum];
    for(int i=0;i<tasknum;i++){
        wq[i] = ll_init_IO();
        rq[i] = ll_init_IO();
        gcq[i]= ll_init_IO();
        next_w_release[i] = 0;
        next_r_release[i] = 0;
        next_gc_release[i] = 0;
        wjob_finished[i] = 1;   //init job-finish as 1, since no job is scheduled at initial time.
        rjob_finished[i] = 1;
        gcjob_finished[i] = 1;
        wjob_deferred[i] = 0;
    }
    rr = ll_init_IO();
    bwr = ll_init_IO();
    //overhead tracker params
    //(decision timing now uses ovhd_now_us(); see ovhd_stats.h)
    struct timeval tot_start_time;
    struct timeval tot_end_time;
    long write_release_num = 0;
    long gc_release_num = 0;
    long rr_release_num = 0;
    long write_ovhd_sum = 0;
    long gc_ovhd_sum = 0;
    long rr_ovhd_sum = 0;
    /* [C3] LaWL-S admission accounting.
     *   admit          : slack budget positive AND a victim pair was found.
     *   skip_noslack   : slack budget non-positive under current latency lens
     *                    (Eq. 13 admission failed). This is the criterion-
     *                    driven skip — the metric §C3 targets.
     *   skip_novictim  : slack ok but find_WR/RR_target returned no pair.
     * Reported separately so admit_rate = admit / (admit + skip_noslack)
     * is not diluted by orthogonal skips. */
    long rr_admit_events         = 0;
    long rr_skip_noslack_events  = 0;
    long rr_skip_novictim_events = 0;

    /* [C4] Δ_unsafe + confusion-matrix accounting. All t_first_* timestamps
     * are captured on FIRST occurrence and remain -1 if the event never fires
     * during the run (interpret as "safe" / "conservative" in post-proc).
     * Confusion cells count per-job-completion pairs of (release-time
     * prediction, job-interval reality).
     *
     * t_first_util_overflow — first cur_cp where total_u crosses 1.0 (either
     * during 1M-cycle poll or right after a GC completion). Reference for
     * the utilization trace: gz rows carry a per-event is_overflow flag,
     * but this global gives the sweep-summary landmark without post-hoc
     * awk over million-row traces. */
    long t_first_dlmiss        = -1L;
    long t_first_infeasible    = -1L;
    long t_first_util_overflow = -1L;
    long n_releases   = 0, n_feasible = 0, n_infeasible = 0, n_miss = 0;
    long cnt_FN = 0, cnt_FP = 0, cnt_TN = 0, cnt_TP = 0;
    /* Per-task pending prediction: at release we stash (release_time,
     * deadline, prediction); at completion we look it up, tally the
     * confusion cell, and emit a jobs.csv row. `valid=0` while no job
     * of this (task, type) is in flight. */
    typedef struct {
        long release_time;
        long deadline;
        int  prediction;   /* 1=infeasible, 0=feasible under find_worst_util_assumed */
        int  valid;
    } pending_pred_t;
    pending_pred_t pending_w[tasknum];
    pending_pred_t pending_r[tasknum];
    pending_pred_t pending_gc[tasknum];
    for(int __k=0; __k<tasknum; __k++){
        pending_w[__k].valid  = 0;
        pending_r[__k].valid  = 0;
        pending_gc[__k].valid = 0;
    }
    long tot_runtime;
    double tot_runtime_readable;
    double write_ovhd_avg, gc_ovhd_avg, rr_ovhd_avg;
    
    //TEMPCODE::open file for invfull block check.

    //enable following two lines in a case to check util per cycle.
    //randtask_statechecker(tasknum,8000);
    //return;

    //add scheme flags for flexible write policy change
    printf("[ SCHEMES ] %d, %d, %d, %d\n",wflag,gcflag,rrflag,rrcond);
    printf("[EXEC-main] %d, %d\n",genflag,taskflag);
    printf("[EXEC-task] %d, %f\n",tasknum,totutil);
    printf("[EXEC-skew] %d, %f, %f, %d\n",skewness,sploc,tploc,skewnum);
    printf("[EXEC-OP  ] %d, %f, %d\n",OPflag, OP, MINRC);
    printf("[NOB MAXPE] : %d, %d\n",NOB,MAXPE);
    // [FIXED-LATENCY] echo which latency lens LaWL will use for decisions.
    // Ground-truth exec / overflow / MAXPE are always state-aware regardless.
    printf("[LAT MODE ] : %d (0=STATE 1=FIXED_S 2=FIXED_E 3=LAWL_OPT 4=LAWL_AVG 5=LAWL_PES)\n", latency_mode);
    //sleep(1);
   
    //MINRC is now a configurable value, which can be adjusted like OP
    //reference :: RTGC mechanism (2004, li pin chang et al.) 
    max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int expected_invalid = MINRC*(GCTHRESNOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d, maxvalid : %d\n",expected_invalid,expected_fp,max_valid_pg);
    
    //TASK GENERATOR CODE
    if(taskflag == 1){ //generate taskset and save
        float res = 1.0;
        task_gen_success = 0;
        while(task_gen_success == 0){
            if(skewness == -1){
                rand_tasks = generate_taskset(tasknum,totutil,max_valid_pg,&res,0);
            }
            else if (skewness == -2){ //manually assign value for taskset(hardcode). edit parameters for test.
                rand_tasks = generate_taskset_hardcode(tasknum,max_valid_pg,&res);
            }
            else if(skewness >= 0){
                rand_tasks = generate_taskset_skew2(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
            }
            else if(skewness == -3){ //manually assign w/r utilization for each task. edit parameters for test.
                rand_tasks = generate_taskset_fixed(max_valid_pg,&res);
            }
	    else if(skewness == -4){
		rand_tasks = generate_taskset_hardcode_motiv(tasknum,totutil,max_valid_pg,&res,0);
            }
            else if(skewness == -5){ //max-decision-rate stress config for controller-overhead measurement
                rand_tasks = generate_taskset_maxrate(tasknum,totutil,max_valid_pg,&res);
            }
            task_gen_success = 1; //mark flag as 1, and check edge cases.
            if(res > 1.0){//initial total utilization > 1.0
                task_gen_success = 0;
            }
            else{//period < 0 due to overflow
                for(int i=0;i<tasknum;i++){
                    if(rand_tasks[i].wp <= 0 || rand_tasks[i].rp <= 0 || rand_tasks[i].gcp <= 0){
                        task_gen_success = 0;
                    }
                }
            }
	    for(int i=0;i<tasknum;i++){
		if(rand_tasks[i].wp <= 0 || rand_tasks[i].rp <= 0 || rand_tasks[i].gcp <= 0){
		    task_gen_success = 0;
		}
	    }
            //if task gen fails, retry generation
            if(task_gen_success == 0){
                free(rand_tasks);
            }
        }
        FILE* taskparams = fopen("taskparam.csv","w");
        for(int i=0;i<tasknum;i++){
            printf("saving %d,%d,%d,%d,%d,%d,%d\n",rand_tasks[i].wn,rand_tasks[i].wp,rand_tasks[i].rn,rand_tasks[i].rp,rand_tasks[i].gcp,
                rand_tasks[i].addr_lb,rand_tasks[i].addr_ub);
            fprintf(taskparams,"%d,%d,%d,%d,%d,%d,%d\n",
                rand_tasks[i].wn,rand_tasks[i].wp,rand_tasks[i].rn,rand_tasks[i].rp,rand_tasks[i].gcp,
                rand_tasks[i].addr_lb,rand_tasks[i].addr_ub);
        }
        fprintf(taskparams,"%f\n",res);
        fflush(taskparams);
        fclose(taskparams);

        if(init_cyc == -1){
            FILE* init_cycs = fopen("cyc.csv","w");
            for(int i=0;i<NOB;i++){
                int temp = rand() % ((int)MAXPE/4);
                fprintf(init_cycs,"%d,",temp);
            }
            fflush(init_cycs);
            fclose(init_cycs);
        }
        return 0;
    }
    
    //WORKLOAD GENERATOR CODE  
    if(genflag == 1){//generate workload and save
        FILE* file_taskparam = fopen("taskparam.csv","r");
        rand_tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
        if(OPflag != 1){
            get_task_from_file(rand_tasks,tasknum,file_taskparam);
        } else if(OPflag == 1){
            get_task_from_file_recalc(rand_tasks,tasknum,file_taskparam,max_valid_pg);
        }
        offset = (int)((float)(rand_tasks[0].addr_ub - rand_tasks[0].addr_lb)*sploc/2.0);
        IOgen(tasknum,rand_tasks,WORKLOAD_LENGTH,offset,sploc,tploc);
        printf("workload generated!\n");
        fclose(file_taskparam);
        return 0;
    }

    //init task 
    tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    FILE* main_taskparam = fopen("taskparam.csv","r");
    FILE* locfile = fopen("loc.csv","r");
    get_task_from_file(tasks,tasknum,main_taskparam);
    get_loc_from_file(tasks,tasknum,locfile);
    fclose(main_taskparam);
    fclose(locfile);    

#ifdef TIMING_ON_MEM
    int prof_targ_lpa;
    int wn_count = 0;
    IO_open(tasknum,w_workloads,r_workloads);
    for(int i=0;i<max_valid_pg;i++){
        lpa_update_timing[i] = (long*)malloc(sizeof(long)*init_length);
        cur_length[i] = init_length;
        update_cnt[i] = 0;
    }
    for(int a=0;a<tasknum;a++){
        cur_cp = 0;
        wn_count = 0;
        while(EOF != fscanf(w_workloads[a],"%d,",&prof_targ_lpa)){
            //write on memory
            lpa_update_timing[prof_targ_lpa][update_cnt[prof_targ_lpa]] = cur_cp;
            //printf("lpa_update_timing[%d][%d] = %ld",prof_targ_lpa,update_cnt[prof_targ_lpa],cur_cp);
            update_cnt[prof_targ_lpa]++;
            //if malloc space is not enough, relocate the update timing records.
            if(cur_length[prof_targ_lpa] <= update_cnt[prof_targ_lpa]){
                long* temp_arr = (long*)malloc(sizeof(long)*(cur_length[prof_targ_lpa]+init_length));
                for(int b=0;b<cur_length[prof_targ_lpa];b++){
                    temp_arr[b] = lpa_update_timing[prof_targ_lpa][b];
                }
                free(lpa_update_timing[prof_targ_lpa]);
                lpa_update_timing[prof_targ_lpa] = temp_arr;
                cur_length[prof_targ_lpa] += init_length;
            }
            wn_count++;
            if(wn_count == tasks[a].wn){
                cur_cp += tasks[a].wp;
                wn_count = 0;
            }
        }
    }
    for(int a=0;a<update_cnt[0];a++){
        //printf("%ld\n",lpa_update_timing[0][a]);
    }
    for(int a=0;a<tasknum;a++){
        for(int b=tasks[a].addr_lb;b<tasks[a].addr_ub;b++){
            //printf("(%d)%d,%ld ",b,update_cnt[b],lpa_update_timing[b][0]);
        }
        //printf("\n");
    }
    IO_close(tasknum,w_workloads,r_workloads);
#endif


    //LPA PROFILE GENERATOR CODE
    //profile LPA invalidation pattern(per each address)
    if(profflag == 1){
        int prof_targ_lpa;
        int wn_count = 0;
        char name[30];
        FILE* write_targ_file;
        IO_open(tasknum,w_workloads,r_workloads);
        for(int a=0;a<tasknum;a++){
            cur_cp = 0;
            wn_count = 0;
            while(EOF != fscanf(w_workloads[a],"%d,",&prof_targ_lpa)){
                //write on file
                sprintf(name,"./timing/%d.csv",prof_targ_lpa);
                write_targ_file = fopen(name,"a");
                fprintf(write_targ_file,"%ld,",cur_cp);
                fclose(write_targ_file);
                wn_count++;
                if(wn_count == tasks[a].wn){
                    cur_cp += tasks[a].wp;
                    wn_count = 0;
                }
            }
        }
        return 0;
    }
    //LPA PROFILE GENERATOR CODE 2
    //profile LPA invalidation pattern in one file for plotting + profile GC pattern for plotting
    if(profflag == 2){
        //main flag :: generate a scatter plot of LPA update vs timestamp
        int wn_count[tasknum];
        int wn_before_GC = 0;
        long next_wp[tasknum];
        long next_cp;
        int next_task;
        int invalidation_count;
        long invalid_cumulative = 0;
        long reclaim_cumulative = 0;
        int fp_count;
        char name[30];
        char name2[30];
        FILE* IO_scatter_file;
        FILE* GC_scatter_file;
        IO_open(tasknum,w_workloads,r_workloads);
        sprintf(name,"scatter.csv");
        sprintf(name2,"GC_timing.csv");
        IO_scatter_file = fopen(name,"w");
        GC_scatter_file = fopen(name2,"w");
        for(int a=0;a<tasknum;a++){
            next_wp[a] = tasks[a].wp;
        }

        //start profiling assuming that dummy write is all done.
        cur_cp = 0;
        invalidation_count = 0;
        fp_count = PPB*NOB - max_valid_pg;
        while(cur_cp <= WORKLOAD_LENGTH){
            for(int a=0;a<tasknum;a++){
                if(cur_cp % tasks[a].wp == 0){
                    for(int b=0;b<tasks[a].wn;b++){
                        wn_before_GC++;
                        invalidation_count++;
                        invalid_cumulative++;
                        fp_count--;
                        fprintf(IO_scatter_file,"%ld, %ld, %d, %ld\n",cur_cp,(long)IOget(w_workloads[a]),a,invalid_cumulative);
                    }
                }
            }
            for(int a=0;a<tasknum;a++){
                if(cur_cp % tasks[a].gcp == 0){//GC timing reached
                    if(invalidation_count >= expected_invalid){
                    //if(1){
                        reclaim_cumulative += PPB;
                        fprintf(GC_scatter_file,"%ld, -1, %d, %d, %d, %d, %ld, %ld\n",cur_cp,wn_before_GC,invalidation_count,fp_count,a,invalid_cumulative,reclaim_cumulative);
                        wn_before_GC = 0;
                        invalidation_count -= PPB;
                        fp_count += PPB;
                    } else {
                        //do nothing, which means we skip GC.
                    }
                }
            }
            //find next checkpoint.
            next_cp = next_wp[0];
            for(int a=1;a<tasknum;a++){
                if(next_wp[a] < next_cp){
                    next_cp = next_wp[a];
                }
            }//checkpoint found.
            //update tasks' checkpoint if next_cp == next_wp[a].

            for(int a=0;a<tasknum;a++){
                if(next_wp[a] == next_cp){
                    next_wp[a] += tasks[a].wp;
                }
            }//checkpoint updated
            cur_cp = next_cp;
            printf("next checkpoint: %ld, cur_inv: %d, cur_fp: %d\n",cur_cp,invalidation_count,fp_count);
        }
        fclose(IO_scatter_file);
        fclose(GC_scatter_file);
        return 0;
    }

    //(deprecated)run gradient tests for write in offline, and assign offset value for WGRAD policy.
    offset = (int)((float)(tasks[0].addr_ub - tasks[0].addr_lb)*sploc/2.0);

    //init csv files
    // fps = open_file_pertask(gcflag,wflag,rrflag,tasknum);
    
    /* Pick a scheme prefix so results from different (wflag, gcflag, rrflag)
     * combos don't get mixed when they land in the same directory.
     *   Baseline : wflag=0  gcflag=0  rrflag=-1
     *   Hyb      : wflag=11 gcflag=0  rrflag=0
     *   LaWL_D   : wflag=14 gcflag=6  rrflag=-1
     *   LaWL fam : wflag=14 gcflag=6  rrflag=1
     *   Dyn      : anything else
     *
     * The LaWL family further splits by latency_mode into STATE / opt / avg /
     * pes (plus legacy fixedS/fixedE) — same decision path, different lens
     * for the AssumedLatencyModel. Each variant gets its own filename so the
     * shell can drop all four into the same directory without collision.
    */

    // baseline
    if (wflag == 0  && gcflag == 0 && rrflag == -1){
        fplife      = fopen("baseline_lifetime.csv",     "a");
        fpovhd      = fopen("baseline_overhead.csv",     "a");
        fpjobs_sum  = fopen("baseline_jobs_summary.csv", "a");
        OPEN_ITER_GZ (util_fp, "baseline_utilization");
        OPEN_ITER_GZ(fpjobs,  "baseline_jobs");
    }
    // dynamic WL
    else if (wflag == 11 && gcflag == 0 && rrflag == -1) {
        fplife      = fopen("dyn_lifetime.csv",     "a");
        fpovhd      = fopen("dyn_overhead.csv",     "a");
        fpjobs_sum  = fopen("dyn_jobs_summary.csv", "a");
        OPEN_ITER_GZ (util_fp, "dyn_utilization");
        OPEN_ITER_GZ(fpjobs,  "dyn_jobs");
    }
    // hybrid WL
    else if (wflag == 11 && gcflag == 0 && rrflag ==  0) {
        fplife      = fopen("hyb_lifetime.csv",     "a");
        fpovhd      = fopen("hyb_overhead.csv",     "a");
        fpadmit_sum = fopen("hyb_admit_summary.csv","a");
        fpjobs_sum  = fopen("hyb_jobs_summary.csv", "a");
        OPEN_ITER_GZ (util_fp, "hyb_utilization");
        OPEN_ITER_GZ(fpadmit, "hyb_admit");
        OPEN_ITER_GZ(fpjobs,  "hyb_jobs");
    }
    // LaWL_D
    else if (wflag == 14 && gcflag == 6 && rrflag == -1) {
        fplife      = fopen("LaWL_D_lifetime.csv",     "a");
        fpovhd      = fopen("LaWL_D_overhead.csv",     "a");
        fpjobs_sum  = fopen("LaWL_D_jobs_summary.csv", "a");
        OPEN_ITER_GZ (util_fp,    "LaWL_D_utilization");
        OPEN_ITER_GZ(fpshadow_w, "LaWL_D_shadow_write");
        OPEN_ITER_GZ(fpshadow_g, "LaWL_D_shadow_gc");
        OPEN_ITER_GZ(fpjobs,     "LaWL_D_jobs");
    }
    // LaWL
    else if (wflag == 14 && gcflag == 6 && rrflag ==  1){
        const char* prefix = NULL;
        switch(latency_mode){
            case LATENCY_MODE_LAWL_OPT: prefix = "LaWL_opt"; break;
            case LATENCY_MODE_LAWL_AVG: prefix = "LaWL_avg"; break;
            case LATENCY_MODE_LAWL_PES: prefix = "LaWL_pes"; break;
            case LATENCY_MODE_STATE:    prefix = "LaWL";     break;
            default:                    prefix = NULL;
        }
        if(prefix){
            char __path[128];
            snprintf(__path, sizeof(__path), "%s_lifetime.csv",     prefix); fplife      = fopen(__path, "a");
            snprintf(__path, sizeof(__path), "%s_overhead.csv",     prefix); fpovhd      = fopen(__path, "a");
            snprintf(__path, sizeof(__path), "%s_admit_summary.csv",prefix); fpadmit_sum = fopen(__path, "a");
            snprintf(__path, sizeof(__path), "%s_jobs_summary.csv", prefix); fpjobs_sum  = fopen(__path, "a");
            /* Per-iter event traces. Reuse a scratch buffer built by the
             * macros; the underlying files open in "w"/"wb" so multi-iter
             * sweeps don't concatenate distinct per-taskset streams. */
            char __base[80];
            snprintf(__base, sizeof(__base), "%s_utilization",  prefix); OPEN_ITER_GZ (util_fp,    __base);
            snprintf(__base, sizeof(__base), "%s_admit",        prefix); OPEN_ITER_GZ(fpadmit,    __base);
            snprintf(__base, sizeof(__base), "%s_shadow_write", prefix); OPEN_ITER_GZ(fpshadow_w, __base);
            snprintf(__base, sizeof(__base), "%s_shadow_gc",    prefix); OPEN_ITER_GZ(fpshadow_g, __base);
            snprintf(__base, sizeof(__base), "%s_jobs",         prefix); OPEN_ITER_GZ(fpjobs,     __base);
        }
    }

    /* Legacy trace files — disabled by default. Callers null-check before writing. */
    rr_profile     = NULL;
    updateorder_fp = NULL;
    u_check        = NULL;
    updaterate_fp  = NULL;
    gc_valid_fp    = NULL;

    /* Fail fast if no branch matched — otherwise the fprintf sites below
     * would dereference NULL fplife/fpovhd deep into the sim run. Cover
     * cases like FIXED_S/FIXED_E argv[12] flags (not in the LaWL switch),
     * or arbitrary (wflag,gcflag,rrflag) combos users try. */
    if(!fplife || !fpovhd){
        fprintf(stderr, "[FATAL] no log-open branch matched: "
                        "wflag=%d gcflag=%d rrflag=%d latency_mode=%d\n",
                        wflag, gcflag, rrflag, latency_mode);
        return 2;
    }

    /* util_fp header intentionally omitted — file opens in "a" mode and
     * many taskset iters share the same file; writing a header per run
     * would produce duplicate header rows.
     * Schema: cur_cp, total_u, is_overflow, is_dlmiss, response_time, src
     *   src=poll : 1M-cycle utilization poll (no cur_IO → response_time=-1, is_dlmiss=0)
     *   src=evt  : per IO event completion (response_time = cur_cp - IO_start_time;
     *                                        is_dlmiss=1 only when last-req of an
     *                                        RD/WR job breached its task period) */
    if(gc_valid_fp != NULL){
        fprintf(gc_valid_fp,"timestamp,taskidx,vic_idx,block_state,gc_valid_count\n");
    }

    IO_open(tasknum, w_workloads,r_workloads);
    //lat_open(gcflag, wflag, rrflag, tasknum, lat_log_w, lat_log_r, lat_log_gc);

    
    //  for(int i=0;i<tasknum;i++){
    //      fprintf(fps[i],"%s\n","timestamp,taskidx,WU,new_WU,noblock,w_util,r_util,g_util,old,yng,bidx,state,vp,w_idx,w_state,fb,w");
    //  }
    // fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum, init_cyc);

#ifdef GC_ON_WRITEBLOCK
    fblist_head = init_blocklist(0,NOB-1);
    rsvlist_head = init_blocklist(0,-1);
#endif
#ifndef GC_ON_WRITEBLOCK
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
#endif
    
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);
    glob_yb = init_blocklist(0,-1);
    glob_ob = init_blocklist(0, -1);
    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);
    
    //init data access & distribution tracker
    for(int i=0;i<tasknum;i++){
        cur_wb[i] = NULL;
        cur_GC[i].cur_vic = NULL;
        cur_GC[i].cur_rsv = NULL;
    }
    cur_rr.cur_vic1 = NULL;
    cur_rr.cur_vic2 = NULL;
    cur_rr.rrcheck = -1L;

    //do initial writing (validate all logical address)
    printf("total fp before dummy : %d\n",newmeta->total_fp);
    //!!!change logi value to increase/decrease dummy writes.
    //note that addresses which are not accessed during dummy write has no mapping info
    //int logi = (int)(NOP*(1-OP));
    int logi = 87040;
    cur_fb = ll_pop(fblist_head);
    for(int i=0;i<logi;i++){
        if (cur_fb->fpnum == 0){
            ll_append(full_head,cur_fb);
            cur_fb = ll_pop(fblist_head);
            g_cur = (cur_fb->idx+1)*PPB-cur_fb->fpnum;
        }
        newmeta->pagemap[g_cur] = i;
        newmeta->rmap[i] = g_cur;
        newmeta->total_fp--;
        for(int j=0;j<tasknum;j++){
            if(i < tasks[j].addr_ub && i >= tasks[j].addr_lb){
                newmeta->vmap_task[i] = j;
            }
        }
        cur_fb->fpnum--;
        g_cur++;
    }
    printf("total fp after dummy : %d\n",newmeta->total_fp);
    //return dummy task's block into blocklist.
    if(cur_fb->fpnum == 0){
        ll_append(full_head,cur_fb);
    } 
    else {
        ll_append(fblist_head,cur_fb);
    }
    //!!finish initialization
    printf("fblist sanity check :");
    block* test = fblist_head->head;
    while(test != NULL){ 
        printf("[%d]%d, ",test->idx,test->fpnum);
        test = test->next;
    }
    printf("\n");
    sleep(5);
    //run simulation
#ifdef utilsort_writecheck
    for(int i=0;i<4;i++){
        char testgcwriteblockname[20];
        sprintf(testgcwriteblockname,"wbtest_%d.csv",i);
        test_gc_writeblock[i] = fopen(testgcwriteblockname,"w");
    }
#endif

    gettimeofday(&(tot_start_time),NULL);
    //start of simulation
    while(cur_cp <= RUNTIME){
        /* Refresh state[] extrema cache only when GC completion has bumped a
         * P/E cycle. Single pass computes yngest, oldest, and MAXPE hit in
         * one sweep of NOB entries. */
        if(state_cache_dirty){
            int __y = newmeta->state[0];
            int __o = newmeta->state[0];
            int __mp = -1;
            for(int __i=0; __i<NOB; __i++){
                int __s = newmeta->state[__i];
                if(__s < __y) __y = __s;
                if(__s > __o) __o = __s;
                if(__mp == -1 && __s >= MAXPE) __mp = __i;
            }
            cached_yngest = __y;
            cached_oldest = __o;
            cached_maxpe_idx = __mp;
            state_cache_dirty = 0;
        }
        yngest = cached_yngest;
        oldest = cached_oldest;
        //flash state checker
        if(cur_cp % 1000000L == 0){
            total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
            /* Utilization overflow — log & continue. Experiment runs until
             * MAX PEC is reached (see the cached_maxpe_idx branch below);
             * overflow is a diagnostic signal, not a termination condition.
             * The util_fp trace records total_u anyway; here we also emit a
             * "poll" row with is_overflow=1 so grep -w 1 on util_fp lists
             * every episode. Rising-edge guard is stdout-only: printf once
             * per episode instead of once per 1M-cycle poll tick. */
            int is_overflow = (total_u >= 1.0) ? 1 : 0;
            /* poll-src rows have no cur_IO → response_time=-1, is_dlmiss=0 */
            if(util_fp) gzprintf(util_fp, "%ld,%f,%d,%d,%ld,poll\n",
                                 cur_cp, total_u, is_overflow, 0, (long)-1);
            if(is_overflow){
                if(!util_overflow_active){
                    util_overflow_active = 1;
                    util_overflow_events++;
                    if(t_first_util_overflow == -1L) t_first_util_overflow = cur_cp;
                    printf("[%ld] utilization overflow (poll), util : %f  (continuing)\n", cur_cp, total_u);
                }
            } else if(util_overflow_active){
                util_overflow_active = 0;  // recovered — arm guard for next episode
            }
        }
        if(cached_maxpe_idx >= 0){
            int idx = cached_maxpe_idx;
            {
                total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
                printf("[%ld]a block(idx=%d) reached maximum P/E, util : %f\n",cur_cp, idx, total_u);
                printf("[SUMMARY] util-overflow episodes during run: %ld\n", util_overflow_events);
                printf("[SUMMARY] deadline misses during run       : %ld\n", dl_miss_events);
                printf("[SUMMARY] first util>=1.0  timestamp       : %ld\n", t_first_util_overflow);
                printf("[SUMMARY] first deadline-miss timestamp    : %ld\n", t_first_dlmiss);
                printf("[SUMMARY] first infeasible timestamp       : %ld\n", t_first_infeasible);
                gettimeofday(&tot_end_time,NULL);                                                                     tot_runtime = tot_end_time.tv_sec * 1000000 + tot_end_time.tv_usec - tot_start_time.tv_sec * 1000000 - tot_start_time.tv_usec;
                tot_runtime_readable = (double)tot_runtime / 1000.0 / 1000.0 / 60.0 ;
                if(write_release_num != 0){
                    write_ovhd_avg = (double)write_ovhd_sum / (double)write_release_num;
                } else {
                    write_ovhd_avg = 0;
                }
                if(gc_release_num != 0){
                    gc_ovhd_avg = (double)gc_ovhd_sum / (double)gc_release_num;
                } else {
                    gc_ovhd_avg = 0;
                }
                if(rr_release_num != 0){
                    rr_ovhd_avg = (double)rr_ovhd_sum / (double)rr_release_num;
                } else {
                    rr_ovhd_avg = 0;
                }
                fprintf(fplife,"%ld,",cur_cp);
                fprintf(fpovhd,"%ld, %ld, %ld, ",write_release_num,gc_release_num,rr_release_num);
                fprintf(fpovhd,"%lf, %lf ,%lf, %lf\n",write_ovhd_avg,gc_ovhd_avg,rr_ovhd_avg,tot_runtime_readable);
                /* [C3] end-of-run admit summary + PEC-distribution stats.
                 * PEC dist quantifies how evenly relocation spread wear
                 * (§C3 last measurement bullet). */
                if(fpadmit_sum){
                    long __psum = 0, __psq = 0;
                    int  __pmax = newmeta->state[0], __pmin = newmeta->state[0];
                    for(int __i=0; __i<NOB; __i++){
                        int __s = newmeta->state[__i];
                        __psum += __s;
                        __psq  += (long)__s * (long)__s;
                        if(__s > __pmax) __pmax = __s;
                        if(__s < __pmin) __pmin = __s;
                    }
                    double __pmean = (double)__psum / (double)NOB;
                    double __pvar  = ((double)__psq  / (double)NOB) - __pmean * __pmean;
                    if(__pvar < 0) __pvar = 0;
                    double __pstd  = sqrt(__pvar);
                    long   __tot   = rr_admit_events + rr_skip_noslack_events + rr_skip_novictim_events;
                    double __arate = (__tot > 0) ? (double)rr_admit_events / (double)__tot : 0.0;
                    fprintf(fpadmit_sum,
                            "%ld,%ld,%ld,%ld,%f,%d,%d,%f,%f\n",
                            cur_cp, rr_admit_events,
                            rr_skip_noslack_events, rr_skip_novictim_events,
                            __arate, __pmax, __pmin, __pmean, __pstd);
                }
                /* [C4] End-of-run confusion-matrix summary.
                 *   delta_unsafe = t_first_infeasible − t_first_dlmiss
                 * Missing timestamps (event never fired) are emitted as -1
                 * and delta_unsafe is left at 0; post-proc must condition on
                 * both t_first_* being present before interpreting Δ.
                 * t_first_util_overflow appended at the end so downstream
                 * readers of the original 14-column schema still work. */
                if(fpjobs_sum){
                    long __delta = 0L;
                    if(t_first_dlmiss != -1L && t_first_infeasible != -1L)
                        __delta = t_first_infeasible - t_first_dlmiss;
                    long   __nn   = n_releases - n_miss;
                    double __fnr  = (n_miss > 0) ? (double)cnt_FN / (double)n_miss : 0.0;
                    double __fpr  = (__nn  > 0) ? (double)cnt_FP / (double)__nn   : 0.0;
                    fprintf(fpjobs_sum,
                            "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%f,%f,%ld\n",
                            cur_cp, t_first_dlmiss, t_first_infeasible, __delta,
                            n_releases, n_feasible, n_infeasible, n_miss,
                            cnt_FN, cnt_FP, cnt_TN, cnt_TP, __fnr, __fpr,
                            t_first_util_overflow);
                }
                /* Flush + write gz trailer. Without gzclose, the zlib
                 * stream is left mid-block and readers see EOF errors. */
                if(util_fp)     gzclose(util_fp);
                if(fplife)      fclose(fplife);
                if(fpovhd)      fclose(fpovhd);
                if(fpadmit)     gzclose(fpadmit);
                if(fpadmit_sum) fclose(fpadmit_sum);
                if(fpshadow_w)  gzclose(fpshadow_w);
                if(fpshadow_g)  gzclose(fpshadow_g);
                if(fpjobs)      gzclose(fpjobs);
                if(fpjobs_sum)  fclose(fpjobs_sum);
                sleep(1);
                return 1;
            }
        }
        //execution order must be (req completion --> job release --> req pick)
        
        //req completion logic
        if(cur_IO_end == cur_cp){
            if(cur_IO != NULL){
                //a logic to handle I/O to finish
                if(cur_IO->type == GCER){
                    //for(int i=0;i<4;i++){
                    //    print_hotdist_profile(fps[tasknum+i],tasks,cur_cp, newmeta,-1,i);
                    //}
                    //print_freeblock_profile(fps[tasknum+4],cur_cp,newmeta,fblist_head,write_head);
                    // log valid-copy count for this finished GC.
                    // block_state is the P/E cycle *before* the erase increment in finish_GCER.
                    print_gc_valid(gc_valid_fp,cur_cp,cur_IO->taskidx,cur_IO->vic_idx,
                                   newmeta->state[cur_IO->vic_idx],cur_IO->gc_valid_count);
                    total_u = print_profile(tasks,tasknum,cur_IO->taskidx,newmeta,u_check,yngest,oldest,cur_cp,
                                    cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                                    cur_wb[cur_IO->taskidx],fblist_head,write_head,
                                    newmeta->total_fp,cur_IO->gc_valid_count);
                    /* Utilization overflow observed right after a GC
                     * completion — same policy as the polling check: log &
                     * continue, don't terminate the sim. Row is written into
                     * util_fp at the end of this if-branch anyway (see the
                     * finish_req block below), so this site only handles the
                     * stdout printf + rising-edge episode counter. */
                    if(total_u > 1.0){
                        if(!util_overflow_active){
                            util_overflow_active = 1;
                            util_overflow_events++;
                            if(t_first_util_overflow == -1L) t_first_util_overflow = cur_cp;
                            printf("[%ld] utilization overflow (gc), util : %f  (continuing)\n", cur_cp, total_u);
                        }
                    }
                }
                
                /* Per-event dl-miss + response-time state, consumed by the
                 * util_fp write further down. Both are meaningful only for
                 * last-req IOs (job completions); non-terminal IOs get the
                 * defaults below. Sim never terminates on dl miss — MAX PEC
                 * is the sole terminator. */
                int  ev_is_dlmiss     = 0;
                long ev_response_time = -1;

                //if last req is finished, do the following
                if(cur_IO->last == 1){
                    ev_response_time = cur_cp - cur_IO->IO_start_time;
                    if(check_dl_violation(tasks,cur_IO,cur_cp) == 1){
                        ev_is_dlmiss = 1;
                        dl_miss_events++;
                        /* check_dl_violation already printed the per-task
                         * diagnostic; nothing else to do here — the util_fp
                         * evt row below flags is_dlmiss=1 for post-hoc grep. */
                    }
                    /* [C4] Job-completion trace + confusion-matrix tally.
                     * Look up the release-time prediction stashed at release,
                     * emit a jobs.csv row, and bump the appropriate FN/FP/
                     * TN/TP counter. Δ_unsafe: record first miss timestamp. */
                    if(fpjobs_sum){
                        pending_pred_t* pp = NULL;
                        const char*     tag = NULL;
                        if(cur_IO->type == WR){       pp = &pending_w[cur_IO->taskidx];  tag = "WR"; }
                        else if(cur_IO->type == RD){  pp = &pending_r[cur_IO->taskidx];  tag = "RD"; }
                        else if(cur_IO->type == GCER){pp = &pending_gc[cur_IO->taskidx]; tag = "GC"; }
                        if(pp && pp->valid){
                            int __am = ev_is_dlmiss;
                            if(__am){
                                n_miss++;
                                if(t_first_dlmiss == -1L) t_first_dlmiss = cur_cp;
                            }
                            /* Confusion cell: (prediction=feasible/infeasible)
                             * × (reality=miss/no-miss). Paper §3.4(d). */
                            if      (pp->prediction == 0 && __am == 1) cnt_FN++;  /* unsafe */
                            else if (pp->prediction == 0 && __am == 0) cnt_TN++;
                            else if (pp->prediction == 1 && __am == 1) cnt_TP++;
                            else /* pred=1 && am=0 */                  cnt_FP++;  /* over-conservative */
                            if(fpjobs){
                                gzprintf(fpjobs, "%s,%d,%ld,%ld,%d,%ld,%d\n",
                                         tag, cur_IO->taskidx,
                                         pp->release_time, pp->deadline,
                                         pp->prediction, cur_cp, __am);
                            }
                            pp->valid = 0;
                        }
                    }
                    //set finish flags for scheduler,
                    //and if current job is delayed, check if next release is possible.
                    if(cur_IO->type == WR){
                        wjob_finished[cur_IO->taskidx] = 1;
                        if(cur_cp > cur_IO->deadline){
                            next_w_release[cur_IO->taskidx] = cur_cp;
                        }
                    } else if (cur_IO->type == RD){
                        rjob_finished[cur_IO->taskidx] = 1;
                        if(cur_cp > cur_IO->deadline){
                            next_r_release[cur_IO->taskidx] = cur_cp;
                        }
                    } else if (cur_IO->type == GCER){
                        gcjob_finished[cur_IO->taskidx] = 1;
                        if(cur_cp > cur_IO->deadline){
                            next_gc_release[cur_IO->taskidx] = cur_cp;
                        }
                        for(int a=0;a<tasknum;a++){
                            if(wjob_deferred[a] == 1){
                                wjob_deferred[a] = 0;
                                //if current write is delayed, check if write release is possible
                                if(cur_cp >= cur_IO->deadline){
                                    next_w_release[a] = cur_cp;
                                }
                            }
                        }
                    }
                }
                //set finish flags of current rr to shedule new WL
                if(rr->reqnum == 0){
                    rr_finished = 1;
                }
                //set start flags to schedule new WL
                if(cur_IO->type == WR && cur_IO->islastreq == 1){
                    do_rr = 1;
                    wr_end_bwr_flag = 1;
                }
                //finish request
                finish_req(tasks, cur_IO, newmeta,
                           fblist_head, rsvlist_head, full_head,
                           &(cur_GC[cur_IO->taskidx]),&(cur_rr));

                if(util_fp != NULL){
                    float __log_u = find_cur_util(tasks, tasknum, newmeta,
                                                  get_blockstate_meta(newmeta, OLD));
                    int __log_ov = (__log_u >= 1.0f) ? 1 : 0;
                    gzprintf(util_fp,"%ld,%f,%d,%d,%ld,evt\n",
                             cur_cp, __log_u, __log_ov, ev_is_dlmiss, ev_response_time);
                }

                //reset I/O pointer and IO end time tracker
                free(cur_IO);
                cur_IO = NULL;
                cur_IO_end = __LONG_MAX__;
                IO_end_bwr_flag = 1; //notify that IO is ended
            }
        }

        //job release logic
        //release I/O task jobs
        for(int j=0;j<tasknum;j++){
            if(newmeta->total_fp < newmeta->reserved_write + tasks[j].wn){
                printf("%d task write deferred\n",j);
                wjob_deferred[j] = 1;
            }
            if(cur_cp == next_w_release[j] && wjob_finished[j] == 1 && wjob_deferred[j] == 0){
                long __wt0 = ovhd_now_us();
                cur_wb[j] = write_job_start_q(tasks, j, tasknum, newmeta,
                                              fblist_head, full_head, write_head,
                                              w_workloads[j], wq[j], cur_wb[j], wflag, cur_cp);
                write_release_num++;
                /* [C1 SHADOW] Standalone feasibility-count evaluator over
                 * write_head + fblist_head. Populates g_shadow_write, which
                 * the block below flushes to fpshadow_w. Runs unconditionally
                 * for LaWL / LaWL-D so §C1's feasible_ratio is always
                 * measurable regardless of which internal allocator path
                 * write_job_start_q took. */
                if(fpshadow_w){
                    compute_shadow_write(tasks, j, tasknum, newmeta,
                                         fblist_head, write_head);
                }
                /* [C4] Stash release-time prediction for this WR job. The
                 * matching completion (last==1) block looks this up to tally
                 * the (prediction, reality) confusion cell. */
                if(fpjobs_sum){
                    float __wcu_p = find_worst_util_assumed(tasks, tasknum, newmeta);
                    int   __pred  = (__wcu_p > 1.0f) ? 1 : 0;
                    pending_w[j].release_time = cur_cp;
                    pending_w[j].deadline     = cur_cp + (long)tasks[j].wp;
                    pending_w[j].prediction   = __pred;
                    pending_w[j].valid        = 1;
                    n_releases++;
                    if(__pred){
                        n_infeasible++;
                        if(t_first_infeasible == -1L) t_first_infeasible = cur_cp;
                    } else {
                        n_feasible++;
                    }
                }
                {
                    long __d = ovhd_now_us() - __wt0;
                    int  __had_cl = 0;
                    /* excluded time = clustering computation + simulator-
                     * oracle trace reads recorded inside this decision */
                    long __ex = ovhd_take_pending_excl(&__had_cl);
                    long __pure = (__d > __ex ? __d - __ex : 0);
                    write_ovhd_sum += __d;
                    if(__had_cl){
                        /* raw view (diagnostic): decision that included a
                         * cluster-boundary update */
                        ovhd_record(OVHD_WRITE_CL, __d, cur_cp);
                    }
                    /* pure per-request write decision cost */
                    ovhd_record(OVHD_WRITE, __pure, cur_cp);
                }
                /* [C1 SHADOW] Flush any shadow-write stats populated by
                 * assign_write_invalid()'s fblist path during this call. */
                if(fpshadow_w && g_shadow_write.valid){
                    gzprintf(fpshadow_w,"%ld,%d,%d,%d,%d,%d,%d,%d,%d\n",
                             g_shadow_write.cur_cp,
                             g_shadow_write.task,
                             g_shadow_write.n_candidates,
                             g_shadow_write.n_feasible_mode,
                             g_shadow_write.n_feasible_shadow,
                             g_shadow_write.chosen_mode,
                             g_shadow_write.chosen_shadow,
                             g_shadow_write.young_or_old,
                             (g_shadow_write.chosen_mode != g_shadow_write.chosen_shadow) ? 1 : 0);
                    g_shadow_write.valid = 0;
                }
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
                wjob_finished[j] = 0;
            }
            else if (cur_cp == next_w_release[j] && wjob_finished[j] == 0){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            } 
            else if (cur_cp == next_w_release[j] && wjob_deferred[j] == 1){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            }

            if(cur_cp == next_r_release[j] && rjob_finished[j] == 1){
                //printf("next r : %ld, cur cp : %ld, rjob : %d\n",next_r_release[j],cur_cp,rjob_finished[j]);
                read_job_start_q(tasks,j,newmeta,
                                 r_workloads[j],rq[j], cur_cp);
                /* [C4] release-time prediction for RD. Same schema as WR. */
                if(fpjobs_sum){
                    float __wcu_p = find_worst_util_assumed(tasks, tasknum, newmeta);
                    int   __pred  = (__wcu_p > 1.0f) ? 1 : 0;
                    pending_r[j].release_time = cur_cp;
                    pending_r[j].deadline     = cur_cp + (long)tasks[j].rp;
                    pending_r[j].prediction   = __pred;
                    pending_r[j].valid        = 1;
                    n_releases++;
                    if(__pred){
                        n_infeasible++;
                        if(t_first_infeasible == -1L) t_first_infeasible = cur_cp;
                    } else {
                        n_feasible++;
                    }
                }
                next_r_release[j] = cur_cp + (long)tasks[j].rp;
                rjob_finished[j] = 0;
            }
            else if (cur_cp == next_r_release[j] && rjob_finished[j] == 0){
                next_r_release[j] = cur_cp + (long)tasks[j].rp;
            }

            if(cur_cp == next_gc_release[j] && gcjob_finished[j] == 1){          
                if(newmeta->total_fp <= expected_fp){
                    //printf("total_invalid : %d,expected_invalid : %d\n",newmeta->total_invalid,expected_invalid);
                    //printf("total_fp : %d, expected_fp : %d\n",newmeta->total_fp,expected_fp);
                    //printf("blocknum : %d, %d, %d\n",fblist_head->blocknum,full_head->blocknum,write_head->blocknum);
                    long __gt0 = ovhd_now_us();
                    gc_job_start_q(tasks, j, tasknum, newmeta,
                               fblist_head, full_head, rsvlist_head, write_head, 0,
                               gcq[j], &(cur_GC[j]), gcflag, cur_cp);
                    gc_release_num++;
                    {
                        long __d = ovhd_now_us() - __gt0;
                        gc_ovhd_sum += __d;
                        ovhd_record(OVHD_GC, __d, cur_cp);
                    }
                    /* [C2 SHADOW] Flush shadow-gc stats populated by
                     * compute_shadow_gc during this call. */
                    if(fpshadow_g && g_shadow_gc.valid){
                        gzprintf(fpshadow_g,"%ld,%d,%d,%d,%f,%d,%d,%d\n",
                                 g_shadow_gc.cur_cp,
                                 g_shadow_gc.task,
                                 g_shadow_gc.n_candidates,
                                 g_shadow_gc.n_tie_mode,
                                 g_shadow_gc.min_gcutil_mode,
                                 g_shadow_gc.chosen_mode,
                                 g_shadow_gc.chosen_shadow,
                                 (g_shadow_gc.chosen_mode != g_shadow_gc.chosen_shadow) ? 1 : 0);
                        g_shadow_gc.valid = 0;
                    }
                    /* [C4] release-time prediction for GC. GC has no per-job
                     * deadline in check_dl_violation (returns 0 for GCER),
                     * so completions always tally into the "no miss" columns.
                     * The paper's confusion matrix is meaningful mostly for
                     * WR/RD; GC entries add background TN/FP mass. */
                    if(fpjobs_sum){
                        float __wcu_p = find_worst_util_assumed(tasks, tasknum, newmeta);
                        int   __pred  = (__wcu_p > 1.0f) ? 1 : 0;
                        pending_gc[j].release_time = cur_cp;
                        pending_gc[j].deadline     = cur_cp + (long)tasks[j].gcp;
                        pending_gc[j].prediction   = __pred;
                        pending_gc[j].valid        = 1;
                        n_releases++;
                        if(__pred){
                            n_infeasible++;
                            if(t_first_infeasible == -1L) t_first_infeasible = cur_cp;
                        } else {
                            n_feasible++;
                        }
                    }
                    gcjob_finished[j] = 0;
                    next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                }
                else {
                    next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                    gcjob_finished[j] = 1;
                }
            } 
            else if (cur_cp == next_gc_release[j] && gcjob_finished[j] == 0){
                next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
            }
        }
        //release WL jobs
        if(oldest-yngest >= THRESHOLD){//wl start signal
            wl_init = 1;
        }
        if((do_rr == 1) && (cur_cp >= next_rr_check) && (rr_finished == 1) && (rr->head == NULL) && (rrflag != -1) && (wl_init == 1)){
            if(hot_cold_list == 0){
                build_hot_cold(newmeta,hotlist,coldlist);
                hot_cold_list = 1;
            }
            // [C3 / SLACK-BASED + FIXED-LATENCY] LaWL relocation budget = foreground slack.
            // find_worst_util_assumed auto-dispatches on latency_mode (argv[12]):
            //   STATE    -> state-aware WCU (identical to legacy find_worst_util)
            //   LAWL_OPT -> WCU under fresh-block lens   (small WCU -> rrutil high, admit)
            //   LAWL_AVG -> WCU under mid-PEC lens
            //   LAWL_PES -> WCU under worn-block lens    (large WCU -> rrutil <= 0, skip)
            //   FIXED_S/FIXED_E -> legacy fixed lens
            // BGRR (rrcond==7) is a distinct opt-in scheme that requests
            // unconditional background relocation — preserve legacy override.
            float __wcu = find_worst_util_assumed(tasks, tasknum, newmeta);
            if (rrcond == RRCOND_BGRR){
                rrutil = -1.0;  //BGRR: skip admission check, run RR at background priority
            } else {
                rrutil = 1.0 - (double)__wcu;
            }
            /* [C3 SHADOW] What WOULD dyn (STATE-lens) have decided at the
             * same state? Compute WCU under STATE and record the pair for
             * post-hoc divergence analysis. No-op when we already ARE STATE. */
            float  __wcu_shadow    = __wcu;
            double __rrutil_shadow = rrutil;
            if(latency_mode != LATENCY_MODE_STATE){
                int __saved = latency_mode;
                latency_mode = LATENCY_MODE_STATE;
                __wcu_shadow = find_worst_util_assumed(tasks, tasknum, newmeta);
                latency_mode = __saved;
                __rrutil_shadow = 1.0 - (double)__wcu_shadow;
            }

            long __rt0 = ovhd_now_us();
            /* decision:
             *   0 = skip_noslack  (rrutil <= 0 under current lens; BGRR exempt)
             *   1 = admit         (rrutil > 0 AND victim pair found)
             *   2 = skip_novictim (rrutil > 0 but find_WR/RR_target returned no pair)
             * For BGRR: always take the RR_job_start_q path; classify as admit/novictim. */
            int __decision;
            if (rrcond != RRCOND_BGRR && rrutil <= 0.0){
                __decision = 0;
                rr_skip_noslack_events++;
            } else {
                RR_job_start_q(tasks, tasknum, newmeta, fblist_head, full_head, hotlist, coldlist,
                                rr,&(cur_rr),(double)rrutil,cur_cp);
                if(rr->reqnum != 0){
                    rr_release_num++;
                    rr_admit_events++;
                    __decision = 1;
                } else {
                    rr_skip_novictim_events++;
                    __decision = 2;
                }
            }
            {
                long __d = ovhd_now_us() - __rt0;
                rr_ovhd_sum += __d;
                ovhd_record(OVHD_RR, __d, cur_cp);
            }
            if(rr->reqnum != 0){
                rr_finished = 0;
            }
            else {
                rr_finished = 1;
            }
            /* [C3] per-invocation admit trace. Post-processed into the
             * lifetime-binned admit_rate profile that C3 needs.
             * Extended columns record the shadow (STATE-lens) rrutil so
             * divergence can be computed post-hoc:
             *   would_admit_actual = (decision == 1 || decision == 2)
             *   would_admit_shadow = (rrutil_shadow > 0)
             *   divergent = (would_admit_actual != would_admit_shadow)  */
            if(fpadmit){
                int __would_actual = (__decision == 1 || __decision == 2) ? 1 : 0;
                int __would_shadow = (__rrutil_shadow > 0.0) ? 1 : 0;
                int __div_admit    = (__would_actual != __would_shadow) ? 1 : 0;
                gzprintf(fpadmit, "%ld,%f,%f,%d,%d,%d,%f,%f,%d,%d\n",
                         cur_cp, (double)__wcu, rrutil,
                         cached_yngest, cached_oldest, __decision,
                         (double)__wcu_shadow, __rrutil_shadow,
                         __would_shadow, __div_admit);
            }
            next_rr_check = cur_cp + TRELOC; //advance gate regardless of admit outcome (skip-counter semantics, Eq. 13)
            do_rr = 0;
        }
        /*
        //release BWR jobs if possible
        //check if every queue is empty
        qempty_bwr_flag = 1;
        for(int k=0;k<tasknum;k++){
            if(wq[k]->reqnum != 0 || rq[k]->reqnum != 0 || gcq[k]->reqnum != 0){
                qempty_bwr_flag = 0;
                break;
            }
        }
        if(rr->reqnum != 0){
            qempty_bwr_flag = 0;
        }
        
        if(qempty_bwr_flag == 1 && IO_end_bwr_flag == 1 && wr_end_bwr_flag == 1){
            printf("[bwr]queuestat : %c, IO_end_bwr_flag : %c, wr_end_bwr_flag : %c\n",qempty_bwr_flag, IO_end_bwr_flag, wr_end_bwr_flag);
            printf("[bwr]cur_cp : %ld\n",cur_cp);
            //release BWR job

            BWR_job_start_q(tasks,tasknum,newmeta,fblist_head,full_head,write_head,bwr,cur_cp);
            //reset qempty flag and IO end flag
            qempty_bwr_flag = 1;
            IO_end_bwr_flag = 0;
            wr_end_bwr_flag = 0;
        }
        */
        //req pick logic
        if(cur_IO == NULL){
            //init params
            long cur_dl = __LONG_MAX__;
            int target_task = -1;
            int target_type = -1;

            //iterate through per-task queues and pick the I/O with earliest deadline.
            //operation priority is RR < GC < W < R.
            //note that deadline is updated when dl is "less than " cur_dl
            if(rr->head != NULL){
                if(rr->head->deadline <= cur_dl){
                    target_type = RR;
                    cur_dl = rr->head->deadline;
                }
            }
            if(bwr->head != NULL){
                if(bwr->head->deadline <= cur_dl){
                    target_type = BWR;
                    cur_dl = bwr->head->deadline;
                }
            }
            for(int k=0;k<tasknum;k++){
                if(gcq[k]->head != NULL){
                    if(gcq[k]->head->deadline < cur_dl){
                        target_task = k;
                        target_type = GC;
                        cur_dl = gcq[k]->head->deadline;
                    }
                }
            }
            for(int k=0;k<tasknum;k++){
                if(wq[k]->head != NULL){
                    if(wq[k]->head->deadline < cur_dl){
                        target_task = k;
                        target_type = WR;
                        cur_dl = wq[k]->head->deadline;
                    }
                }
            }
            for(int k=0;k<tasknum;k++){
                if(rq[k]->head != NULL){
                    if(rq[k]->head->deadline < cur_dl){
                        target_task = k;
                        target_type = RD;
                        cur_dl = rq[k]->head->deadline;
                    }
                }
            }
            //pop IO from target task's queue
            if(target_type == RD){
                cur_IO = ll_pop_IO(rq[target_task]);  
            }
            if(target_type == WR){
                cur_IO = ll_pop_IO(wq[target_task]);
            }
            else if (target_type == GC){
                cur_IO = ll_pop_IO(gcq[target_task]);
            }
            else if (target_type == RR){
                cur_IO = ll_pop_IO(rr);
            }
            else if (target_type == BWR){
                cur_IO = ll_pop_IO(bwr);
                printf("[BWR]pop BWR, %ld\n",cur_cp);
            }

            //if something's popped out, update cur_IO_end
            if(cur_IO != NULL){
                cur_IO_end = cur_cp + cur_IO->exec;
            }
            else {
                cur_IO_end = __LONG_MAX__;
            }
        }
        
        //go to the next checkpoint
        /* Pass the actual RR admission wake-up gate as wl_next. In SKIPRR
         * (rrflag==-1) the RR block never runs and next_rr_check stays at 0,
         * so find_next_time's past-time guard drops it → no clamp. */
        cur_cp = find_next_time(tasks,tasknum,cur_IO_end,next_rr_check,cur_cp,
                                next_w_release,next_r_release,next_gc_release);
        //printf("[fnt res]next_time : %ld\n",cur_cp);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    /* [C3] end-of-run admit summary + PEC-distribution stats — RUNTIME
     * fallthrough path. Mirrors the MAX-PEC exit above so the summary CSV
     * gets one row per taskset iter regardless of which end fires first. */
    if(fpadmit_sum){
        long __psum = 0, __psq = 0;
        int  __pmax = newmeta->state[0], __pmin = newmeta->state[0];
        for(int __i=0; __i<NOB; __i++){
            int __s = newmeta->state[__i];
            __psum += __s;
            __psq  += (long)__s * (long)__s;
            if(__s > __pmax) __pmax = __s;
            if(__s < __pmin) __pmin = __s;
        }
        double __pmean = (double)__psum / (double)NOB;
        double __pvar  = ((double)__psq  / (double)NOB) - __pmean * __pmean;
        if(__pvar < 0) __pvar = 0;
        double __pstd  = sqrt(__pvar);
        long   __tot   = rr_admit_events + rr_skip_noslack_events + rr_skip_novictim_events;
        double __arate = (__tot > 0) ? (double)rr_admit_events / (double)__tot : 0.0;
        fprintf(fpadmit_sum,
                "%ld,%ld,%ld,%ld,%f,%d,%d,%f,%f\n",
                cur_cp, rr_admit_events,
                rr_skip_noslack_events, rr_skip_novictim_events,
                __arate, __pmax, __pmin, __pmean, __pstd);
    }
    /* [C4] End-of-run confusion-matrix summary — RUNTIME fallthrough. */
    if(fpjobs_sum){
        long __delta = 0L;
        if(t_first_dlmiss != -1L && t_first_infeasible != -1L)
            __delta = t_first_infeasible - t_first_dlmiss;
        long   __nn   = n_releases - n_miss;
        double __fnr  = (n_miss > 0) ? (double)cnt_FN / (double)n_miss : 0.0;
        double __fpr  = (__nn  > 0) ? (double)cnt_FP / (double)__nn   : 0.0;
        fprintf(fpjobs_sum,
                "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%f,%f,%ld\n",
                cur_cp, t_first_dlmiss, t_first_infeasible, __delta,
                n_releases, n_feasible, n_infeasible, n_miss,
                cnt_FN, cnt_FP, cnt_TN, cnt_TP, __fnr, __fpr,
                t_first_util_overflow);
    }
    if(util_fp)     gzclose(util_fp);
    if(fplife)      fclose(fplife);
    if(fpovhd)      fclose(fpovhd);
    if(fpadmit)     gzclose(fpadmit);
    if(fpadmit_sum) fclose(fpadmit_sum);
    if(fpshadow_w)  gzclose(fpshadow_w);
    if(fpshadow_g)  gzclose(fpshadow_g);
    if(fpjobs)      gzclose(fpjobs);
    if(fpjobs_sum)  fclose(fpjobs_sum);
    sleep(1);
    return 0;
}
