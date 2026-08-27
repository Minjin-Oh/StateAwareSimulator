#include "stateaware.h"     // contains
#include "init.h"           // contains init function for various structure params
#include "emul.h"           // contains request process functions for emulation
#include "findRR.h"         // contains block selection functions
#include "IOgen.h"          // contains random workload generation functions
#include "emul_logger.h"    // contains latency logger functions
#include "rtgc.h"           // RTGC comparison scheme (Chang et al. 2004)

// globals
block* cur_fb = NULL;
int rrflag = 0;
int MINRC;                  // minimum reclaimable page, assigned by set_exec_flags in parse.c : 35
double OP;                  // overprovisioning rate, assigned by set_exec_flags in parse.c : 0.32

// RTGC (Chang/Kuo/Lo 2004) mode selection.  gcflag==8 activates the
// comparison scheme; rtgc_lat_mode selects which latency values feed
// the Eq.14 schedulability test (analysis-only; runtime tokens/GC/WL
// never consult this).
int rtgc_mode = 0;
int rtgc_lat_mode = RTGC_LAT_NONE;
int THRES_COLD = 35;        // a global threshold for write-cold block determination, used in find_WR_target_simple() in findRR.c
int THRES_HOT = 300;
int prev_erase = 0;         // a flag for interval specification of threshold update, used in find_WR_target_simple() in findRR.c
int prev_mincyc = 0;        // a parameter for minimum cycle block comparison, used in find_WR_target_simple() in findRR.c ((이거 주석처리 되어있음. 더 이상 사용하지 않는 변수인가?)
int prev_cyc[NOB] = {0, };

#ifdef EXECSTEP
    prof_exec exec_steps;
#endif

// FIXME::set these as global to expose to find_util_safe function
IOhead** wq;
IOhead** rq;
IOhead** gcq;

// FIXME:: set these as global to expose to find_write_gradient function
float sploc;
float tploc;
int offset;

// FIXME:: set these as global to expose proportion (ratio) array to find_write_gradient function in findW.c
double* w_prop;
double* r_prop;
double* gc_prop;

// FIXME:: set these as global to expose workloads to find_write_invalid function
FILE** w_workloads;
FILE** r_workloads;

// FIXME:: set these as global to expose log file pointers and system parameters
long cur_cp;
int max_valid_pg;
int tot_longlive_cnt = 0;
FILE **fps;
FILE *test_gc_writeblock[4];
FILE *updaterate_fp;
FILE *longliveratio_fp;
FILE *updateorder_fp;
FILE *getupdateorder_fp;

// FIXME:: set these as global to expose global write block to assign_write_invalid function
bhead* glob_yb;
bhead* glob_ob;

// a space to store lpa update timing (memory)
long* lpa_update_timing[NOP];
int update_cnt[NOP];
int cur_length[NOP];
int init_length = 10;

int main(int argc, char* argv[]){
    int exit_code = 0;

    // init params
    srand(time(NULL)); 
    bhead* fblist_head = NULL;                      // heads for block list
    bhead* rsvlist_head;
    bhead* full_head;
    bhead* write_head;
    bhead* hotlist;                                 // (for WL) overlaps previous blocklist
    bhead* coldlist;
    meta* newmeta = (meta*)malloc(sizeof(meta));    // metadata structure
    
    // initialize flag variables
    int gcflag = 0;
    int wflag = 0;
    int rrcond = 0; 
    int tasknum;
    int genflag = 0;
    int taskflag = 0;
    int profflag = 0;
    int skewness;                    // skew of utilization(-1 = noskew, 0 = read-skewed, 1 = write-skewed)
    int skewnum;                     // number of skewed task
    int OPflag;
    int init_cyc = 0;
    float totutil;                   // a total utilization of current system

    // get flags
    set_scheme_flags(argv,
                     &gcflag, &wflag, &rrflag, &rrcond);
    set_exec_flags(argv, &tasknum, &totutil,
                   &genflag, &taskflag, &profflag,
                   &skewness, &sploc, &tploc, &skewnum,
                   &OPflag, &init_cyc, &OP, &MINRC);

    // RTGC mode selection.  argv[12] is only consulted when gcflag==8;
    // for all other schemes rtgc_mode stays 0 and rtgc_lat_mode is unused.
    if (gcflag == 8) {
        rtgc_mode = 1;
        rtgc_lat_mode = rtgc_parse_lat_mode(argv);
    }
    // Optional Table IV self-test (env RTGC_SELFTEST=1); runs even for
    // non-RTGC schemes so it can be exercised without changing argv.
    if (getenv("RTGC_SELFTEST") != NULL){
        int st_rc = rtgc_selftest();
        if (getenv("RTGC_SELFTEST_EXIT") != NULL){
            // Convenience for CI: exit after self-test.
            return (st_rc == 0) ? 0 : 2;
        }
    }

#ifdef EXECSTEP
    init_prof_exec(&(exec_steps));
#endif

    // initialize misc variables
    char IO_end_bwr_flag = 0;        // notify that cur_cp is end of I/O req
    char qempty_bwr_flag = 1;        // notify that other queue is empty
    char wr_end_bwr_flag = 0;        // notify that cur_cp is end of write job.
    char task_gen_success = 0;       // notify that task generation was successful.
    int g_cur = 0;                   // pointer for current writing page @ dummy write phase
    int wl_init = 0;                 // flag for wear-leveling initiation
    int rr_finished = 1;
    int hot_cold_list = 0;
    int do_rr = 0;
    long cur_IO_end = __LONG_MAX__;  // absolute time when current req finishes
    float WU;                        // worst-case utilization tracker.
    float rrutil;                    // a utilization allowed to data relocation job
    cur_cp = 0;                      // current checkpoint time
    
    // log file pointers
    FILE* rr_profile;
    FILE *fp, *fplife, *fpwrite, *fpread, *fprr, *fpovhd;
    FILE *fpovhd_gc, *fpovhd_w, *fpovhd_rr, *fpovhd_gc_utilsort, *fpovhd_gc_detail, *fpovhd_w_detail, *fpovhd_w_process, *fpovhd_rr_detail, *fpovhd_rr_detail_process;
    FILE* lat_log_w[tasknum];
    FILE* lat_log_r[tasknum];
    FILE* lat_log_gc[tasknum];
    // RTGC-only extra logs (opened only when rtgc_mode==1).
    FILE *rtgc_token_fp = NULL;
    FILE *rtgc_wl_fp = NULL;
    FILE *rtgc_admission_fp = NULL;

    // IO file pointer init
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
    
    // simulate I/O (init related params)
    float total_u;
    int oldest;
    int yngest;
    int over_avg = 0;
    long rr_check = (long)100000;

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
    for(int i=0;i<tasknum;i++){         // 1 (yes), 0 (no)
        wq[i] = ll_init_IO();
        rq[i] = ll_init_IO();
        gcq[i]= ll_init_IO();
        next_w_release[i] = 0;
        next_r_release[i] = 0;
        next_gc_release[i] = 0;
        wjob_finished[i] = 1;           // init job-finish as 1, since no job is scheduled at initial time.
        rjob_finished[i] = 1;
        gcjob_finished[i] = 1;
        wjob_deferred[i] = 0;
    }
    rr = ll_init_IO();
    bwr = ll_init_IO();

    // overhead tracker params
    struct timeval algo_start_time;
    struct timeval algo_end_time;
    struct timeval tot_start_time;
    struct timeval tot_end_time;
    long write_release_num = 0;
    long gc_release_num = 0;
    long rr_release_num = 0;
    long write_ovhd_sum = 0;
    long gc_ovhd_sum = 0;
    long rr_ovhd_sum = 0;
    long tot_runtime;
    double tot_runtime_readable;
    double write_ovhd_avg, gc_ovhd_avg, rr_ovhd_avg;
    
    // TEMPCODE::open file for invfull block check.
    // longliveratio_fp = fopen("longliveratio.csv","w");

    // enable following two lines in a case to check util per cycle.
    // randtask_statechecker(tasknum,8000);
    // return;

    // add scheme flags for flexible write policy change
    printf("[ SCHEMES ] %d, %d, %d, %d\n",wflag,gcflag,rrflag,rrcond);
    printf("[EXEC-main] %d, %d\n",genflag,taskflag);
    printf("[EXEC-task] %d, %f\n",tasknum,totutil);
    printf("[EXEC-skew] %d, %f, %f, %d\n",skewness,sploc,tploc,skewnum);
    printf("[EXEC-OP  ] %d, %f, %d\n",OPflag, OP, MINRC);
    printf("[NOB MAXPE] : %d, %d\n",NOB,MAXPE);
    if(rtgc_mode){
        char rtgc_prefix_dbg[64];
        rtgc_log_prefix(rrflag, rtgc_lat_mode, rtgc_prefix_dbg, sizeof(rtgc_prefix_dbg));
        printf("[RTGC     ] mode=1, lat=%d, prefix=%s\n", rtgc_lat_mode, rtgc_prefix_dbg);
    }
    // sleep(1);
   
    // MINRC is now a configurable value, which can be adjusted like OP
    // reference :: RTGC mechanism (2004, li pin chang et al.) 
    max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int expected_invalid = MINRC*(GCTHRESNOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d, maxvalid : %d\n",expected_invalid,expected_fp,max_valid_pg);
    
    // TASK GENERATOR CODE
    if(taskflag == 1){                  // generate taskset and save
        float res = 1.0;
        task_gen_success = 0;
        while(task_gen_success == 0){
            if(skewness == -1){         // UUNIFAST algorithm
                rand_tasks = generate_taskset(tasknum,totutil,max_valid_pg,&res,0);
            }
            else if(skewness >= 0){
                // skew2: skewnum = number of write-intensive tasks (rest are read-intensive).
                // Callers wanting "N read-intensive tasks" must pass argv[10] = tasknum - N.
                rand_tasks = generate_taskset_skew2(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
            }

            task_gen_success = 1;       // mark flag as 1, and check edge cases.
            if(res > 1.0){              // initial total utilization > 1.0
                task_gen_success = 0;
            }
            else{                       // period < 0 due to overflow
                for(int i=0;i<tasknum;i++){
                    if(rand_tasks[i].wp <= 0 || rand_tasks[i].rp <= 0 || rand_tasks[i].gcp <= 0){
                        task_gen_success = 0;
                    }
                }
            }

            // if task gen fails, retry generation
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
        exit_code = 0;
        goto CLEANUP;
    }
    
    // WORKLOAD GENERATOR CODE  
    if(genflag == 1){       // generate workload and save
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
        exit_code = 0;
        goto CLEANUP;
    }

    // init task 
    tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    FILE* main_taskparam = fopen("taskparam.csv","r");
    FILE* locfile = fopen("loc.csv","r");
    if (locfile == NULL){
        fprintf(stderr, "Error: Failed to open file %s\n", "loc.csv");
	perror("fopen");
	exit(EXIT_FAILURE);
    }
    get_task_from_file(tasks,tasknum,main_taskparam);
    get_loc_from_file(tasks,tasknum,locfile);
    fclose(main_taskparam);
    fclose(locfile);    

// save the update timing and frequency of LPA
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
            // write on memory
            lpa_update_timing[prof_targ_lpa][update_cnt[prof_targ_lpa]] = cur_cp;
            // printf("lpa_update_timing[%d][%d] = %ld",prof_targ_lpa,update_cnt[prof_targ_lpa],cur_cp);
            update_cnt[prof_targ_lpa]++;

            // if malloc space is not enough, relocate the update timing records.
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
    // The prefetch loop above accumulates cur_cp per task and resets to 0
    // only at each task's start.  After the last task, cur_cp is left at
    // task_{n-1}'s simulated end-time.  For non-RTGC schemes this is
    // masked because find_next_time pulls cur_cp back to 0 in the first
    // iteration (next_w_release[j] == 0 for all j), but RTGC's
    // metaperiod while-loop fires spuriously many times before that
    // correction.  Reset explicitly here.
    cur_cp = 0;
#endif

    // (deprecated) run gradient tests for write in offline, and assign offset value for WGRAD policy.
    offset = (int)((float)(tasks[0].addr_ub - tasks[0].addr_lb)*sploc/2.0);

    // init csv files
    // fps = open_file_pertask(gcflag,wflag,rrflag,tasknum);

    FILE* u_check = NULL;
    if(wflag == 0 && gcflag == 0 && rrflag == -1){           // Baseline
        u_check = fopen("Baseline_rrchecker.csv","w");
        updaterate_fp = fopen("Baseline_updaterate.csv","w");
        // rr_profile = fopen("Baseline_rr_prof.csv","w");
        // updateorder_fp = fopen("Baseline_updateorder.csv", "w");
        fplife = fopen("Baseline_lifetime.csv","a");
        fpovhd = fopen("Baseline_overhead.csv","a");
    }
    else if(wflag == 11 && gcflag ==0 && rrflag == -1){        // Dynamic WL
        u_check = fopen("Dynamic_rrchecker.csv","w");
        updaterate_fp = fopen("Dynamic_updaterate.csv","w");
        // rr_profile = fopen("Dynamic_rr_prof.csv","w");
        // updateorder_fp = fopen("Dynamic_updateorder.csv", "w");
        fplife = fopen("Dynamic_lifetime.csv","a");
        fpovhd = fopen("Dynamic_overhead.csv","a");
    }
    else if(wflag == 0 && gcflag ==0 && rrflag == 0){        // Static WL
        u_check = fopen("Static_rrchecker.csv","w");
        updaterate_fp = fopen("Static_updaterate.csv","w");
        // rr_profile = fopen("Static_rr_prof.csv","w");
        // updateorder_fp = fopen("Static_updateorder.csv", "w");
        fplife = fopen("Static_lifetime.csv","a");
        fpovhd = fopen("Static_overhead.csv","a");
    }
    else if(wflag == 11 && gcflag == 0 && rrflag ==  0){     // Hybrid WL
        u_check = fopen("Hyb_rrchecker.csv","w");
        updaterate_fp = fopen("Hyb_updaterate.csv","w");
        // rr_profile = fopen("Hyb_rr_prof.csv","w");
        // updateorder_fp = fopen("Hyb_updateorder.csv", "w");
        fplife = fopen("Hyb_lifetime.csv","a");
        fpovhd = fopen("Hyb_overhead.csv","a");
    }
    else if(wflag == 14 && gcflag == 0 && rrflag == -1){     // LaWL-D (write only)
        u_check = fopen("wonly_rrchecker.csv","w");
        updaterate_fp = fopen("wonly_updaterate.csv","w");
        // rr_profile = fopen("wonly_rr_prof.csv","w");
        // updateorder_fp = fopen("wonly_updateorder.csv", "w");
        fplife = fopen("wonly_lifetime.csv","a");
        fpovhd = fopen("wonly_overhead.csv","a");
    }
    else if(wflag == 14 && gcflag == 6 && rrflag == -1){        // LaWL-D
        u_check = fopen("LaWL_D_rrchecker.csv","w");
        updaterate_fp = fopen("LaWL_D_updaterate.csv","w");
        // rr_profile = fopen("LaWL_D_rr_prof.csv","w");
        // updateorder_fp = fopen("LaWL_D_updateorder.csv", "w");
        fplife = fopen("LaWL_D_lifetime.csv","a");
        fpovhd = fopen("LaWL_D_overhead.csv","a");
    }
    else if(wflag == 14 && gcflag == 6 && rrflag ==  1){        // LaWL
        u_check = fopen("LaWL_rrchecker.csv","w");
        updaterate_fp = fopen("LaWL_updaterate.csv","w");
        // rr_profile = fopen("LaWL_rr_prof.csv","w");
        // updateorder_fp = fopen("LaWL_updateorder.csv", "w");
        fplife = fopen("LaWL_lifetime.csv","a");
        fpovhd = fopen("LaWL_overhead.csv","a");
    }
    else if(gcflag == 8){                                       // RTGC (Chang et al. 2004)
        // Prefix depends on the analysis latency model and on whether the
        // non-real-time wear leveler is enabled (rrflag==3) or disabled
        // (rrflag==-1 for ablation).  Must match run_simul_rtgc.sh.
        char prefix[64];
        char name[128];
        rtgc_log_prefix(rrflag, rtgc_lat_mode, prefix, sizeof(prefix));

        snprintf(name, sizeof(name), "%s_rrchecker.csv",  prefix); u_check           = fopen(name,"w");
        snprintf(name, sizeof(name), "%s_updaterate.csv", prefix); updaterate_fp     = fopen(name,"w");
        snprintf(name, sizeof(name), "%s_lifetime.csv",   prefix); fplife            = fopen(name,"a");
        snprintf(name, sizeof(name), "%s_overhead.csv",   prefix); fpovhd            = fopen(name,"a");
        snprintf(name, sizeof(name), "%s_token.csv",      prefix); rtgc_token_fp     = fopen(name,"w");
        snprintf(name, sizeof(name), "%s_wl.csv",         prefix); rtgc_wl_fp        = fopen(name,"w");
        snprintf(name, sizeof(name), "%s_admission.csv",  prefix); rtgc_admission_fp = fopen(name,"w");
    }


    IO_open(tasknum, w_workloads, r_workloads);
    // lat_open(gcflag, wflag, rrflag, tasknum, lat_log_w, lat_log_r, lat_log_gc);
    // for(int i=0;i<tasknum;i++){
    //     fprintf(fps[i],"%s\n","timestamp, taskidx, WU, new_WU, noblock, w_util, r_util, g_util, old, yng, bidx, state, vp, w_idx, w_state, fb, w");
    // }
    // fprintf(rr_profile,"%s\n","timestamp, vic1, state, window, vic2, state, window");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    
    // itialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum, init_cyc);

#ifdef GC_ON_WRITEBLOCK   // write 시점에 GC trigger (GC에 사용할 reserved block이 없음)
    fblist_head = init_blocklist(0,NOB-1);
    rsvlist_head = init_blocklist(0,-1);
#endif
#ifndef GC_ON_WRITEBLOCK  // classic GC scheduling 기반
    fblist_head = init_blocklist(0, NOB-tasknum-1);     // free block list
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);   // reserved block list : for using GC copy
#endif
    
    full_head = init_blocklist(0,-1);   // generate 0 component ll.
    write_head = init_blocklist(0,-1);
    glob_yb = init_blocklist(0,-1);
    glob_ob = init_blocklist(0, -1);
    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);
    
    // init data access & distribution tracker
    for(int i=0;i<tasknum;i++){
        cur_wb[i] = NULL;
        cur_GC[i].cur_vic = NULL;
        cur_GC[i].cur_rsv = NULL;
    }
    cur_rr.cur_vic1 = NULL;
    cur_rr.cur_vic2 = NULL;
    cur_rr.rrcheck = -1L;

    // do initial writing (validate all logical address)
    printf("total fp before dummy : %d\n",newmeta->total_fp);

    // !!!change logi value to increase/decrease dummy writes.
    // note that addresses which are not accessed during dummy write has no mapping info
    // int logi = (int)(NOP*(1-OP));
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

    // return dummy task's block into blocklist.
    if(cur_fb->fpnum == 0){
        ll_append(full_head,cur_fb);
    } 
    else {
        ll_append(fblist_head,cur_fb);
    }

    // !!finish initialization

    // Run simulation

#ifdef utilsort_writecheck
    for(int i=0;i<4;i++){
        char testgcwriteblockname[20];
        sprintf(testgcwriteblockname,"wbtest_%d.csv",i);
        test_gc_writeblock[i] = fopen(testgcwriteblockname,"w");
    }
#endif

    // ------------------------------------------------------------------
    // RTGC initialization + admission control.  Must run AFTER the dummy
    // write so metadata->total_fp reflects the true post-init Phi.
    // If admission fails, log the reason, write a 0-lifetime row with
    // exit_reason=ADMISSION_REJECT and skip the simulation loop entirely.
    // ------------------------------------------------------------------
    rtgc_state rtgc_st = {0};
    int rtgc_admit_reason = RTGC_ADMIT_PASS;
    // Sampled at every 1 ms tick so the exit path can log the exit-time
    // utilization even when the exit is deadline miss / MAXPE / runtime end.
    float rtgc_u_admission     = 0.0f;
    float rtgc_u_last_selected = 0.0f;
    float rtgc_u_last_pecmax   = 0.0f;
    float rtgc_u_last_lawl     = 0.0f;
    int   rtgc_exit_reason     = RTGC_EXIT_RUNTIME_END;
    if (rtgc_mode){
        rtgc_init(&rtgc_st, tasks, tasknum, newmeta);
        rtgc_admit_reason = rtgc_admission_check(&rtgc_st, tasks, tasknum,
                                                  newmeta, rtgc_lat_mode,
                                                  rtgc_admission_fp);
        if (rtgc_admission_fp) { fflush(rtgc_admission_fp); }
        printf("[RTGC-INIT] pi=%d alpha=%d rho_init=%d demand-check=%s\n",
                rtgc_st.pi, rtgc_st.alpha, rtgc_st.rho_init,
                rtgc_admit_reason == RTGC_ADMIT_PASS ? "PASS" : "REJECT");
        for (int i = 0; i < tasknum; i++){
            printf("  T%d: wp=%d wn=%d rp=%d rn=%d gcp=%d  sigma=%ld pG=%ld  rho_T=%d rho_G=%d\n",
                i, tasks[i].wp, tasks[i].wn, tasks[i].rp, tasks[i].rn, tasks[i].gcp,
                rtgc_st.sigma[i], rtgc_st.pG[i],
                rtgc_st.rho_T[i], rtgc_st.rho_G[i]);
        }
        if (rtgc_admit_reason != RTGC_ADMIT_PASS){
            // Full RUNTIME_END-style row with lifetime=0 so downstream
            // tooling can distinguish reject vs. runtime end by column.
            // Exit code 0 because admission reject is an analytical
            // outcome, not a runtime failure — the run_simul_rtgc.sh
            // aggregator interprets non-zero as "something crashed".
            rtgc_exit_reason = RTGC_EXIT_ADMISSION_REJECT;
            rtgc_log_lifetime(fplife, 0, rtgc_exit_reason, rtgc_lat_mode,
                               0.0f, 0.0f, 0.0f, 0.0f,
                               get_blockstate_meta(newmeta, OLD),
                               get_blockstate_meta(newmeta, YOUNG));
            exit_code = 0;
            goto CLEANUP;
        }
        if (rtgc_token_fp){
            rtgc_log_tokens(&rtgc_st, newmeta, 0, "init", -1, rtgc_token_fp);
            fflush(rtgc_token_fp);
        }
        // Latch the t=0 utilization so the exit row can report drift.
        rtgc_u_admission = rtgc_analysis_sample(&rtgc_st, tasks, tasknum,
                                                 newmeta, rtgc_lat_mode,
                                                 0,
                                                 0.0f,   // no LaWL sample yet
                                                 get_blockstate_meta(newmeta, OLD),
                                                 get_blockstate_meta(newmeta, YOUNG),
                                                 NULL);   // don't write to rrchecker yet
        rtgc_u_last_selected = rtgc_u_admission;
    }

    // Optional smoke-test / debugging override for RUNTIME.  Never
    // set in production runs.
    long sim_runtime = (long)RUNTIME;
    {
        const char* env_rt = getenv("SIM_RUNTIME_US");
        if (env_rt != NULL){
            char* end = NULL;
            long v = strtol(env_rt, &end, 10);
            if (end != env_rt && v > 0){
                sim_runtime = v;
                fprintf(stderr, "[SIM] RUNTIME overridden by SIM_RUNTIME_US=%ld\n", v);
            }
        }
    }

    // updaterate_fp = fopen("updaterate.csv","w");
    gettimeofday(&(tot_start_time),NULL);

    // !!! start of simulation !!!
    while(cur_cp <= sim_runtime){

        // 1. 한 바퀴 돌 때마다 전체 블록의 PEC를 profiling하고, lowest and highest PEC를 check
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);

        // 2. flash state checker
        // 2-(1). 1000000us마다 profile 정보 저장
        if(cur_cp % 1000000L == 0){
            // Always compute LaWL total_u (used by non-RTGC schemes for
            // overflow-1 and by RTGC as the 5th column of rrchecker).
            if (rtgc_mode){
                // RTGC has its own 10-column rrchecker; call the LaWL
                // computation but discard the write side effect on u_check
                // (pass NULL) — instead, sample all four RTGC modes and
                // route the row to u_check (which is the RTGC rrchecker).
                float lawl_total = print_profile_timestamp(tasks, tasknum,
                                                            newmeta, NULL,
                                                            yngest, oldest,
                                                            cur_cp);
                float U_sel = rtgc_analysis_sample(&rtgc_st, tasks, tasknum,
                                                    newmeta, rtgc_lat_mode,
                                                    cur_cp, lawl_total,
                                                    oldest, yngest,
                                                    u_check);
                rtgc_u_last_selected = U_sel;
                rtgc_u_last_lawl     = lawl_total;
                // Also cache the current PECMAX value for lifetime row.
                {
                    float trM, twM, teM;
                    rtgc_get_latency(RTGC_LAT_PECMAX, newmeta, &trM, &twM, &teM);
                    rtgc_u_last_pecmax = rtgc_util_eq14(tasks, tasknum, trM, twM, teM);
                }
                // RTGC analysis overflow gate.  START/END are constants
                // (never re-evaluated per D-Cadence) so PECMAX / PECAVG
                // are the only modes that can trigger this exit.
                if ((rtgc_lat_mode == RTGC_LAT_PECMAX ||
                     rtgc_lat_mode == RTGC_LAT_PECAVG) && U_sel > 1.0f){
                    printf("[%ld] RTGC analysis overflow (mode=%d, U=%f)\n",
                            cur_cp, rtgc_lat_mode, U_sel);
                    rtgc_exit_reason = RTGC_EXIT_RTGC_ANALYSIS_OVER;
                    goto RTGC_EXIT_LOG;
                }
                // NOTE: overflow-1 (LaWL utilization gate) is intentionally
                // NOT enforced in RTGC mode; LaWL is only logged for
                // comparison.
            } else {
                total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
                //printf("cur_u:%f\n",total_u);

                // utilization overflow 1 (exit code)
                if(total_u >= 1.0){
                    printf("[%ld]utilization overflow 1, util : %f\n",cur_cp, total_u);
                    gettimeofday(&tot_end_time,NULL);
                    tot_runtime = tot_end_time.tv_sec * 1000000 + tot_end_time.tv_usec - tot_start_time.tv_sec * 1000000 - tot_start_time.tv_usec;
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
                    print_profile_updaterate(newmeta,updaterate_fp);
                    exit_code = 1;
                        goto CLEANUP;
                }
            }
        }

        // 2-(2). max P/E cycle overflow (exit code)
        // `oldest` already scans the whole state array via get_blockstate_meta above;
        // reuse it instead of scanning NOB blocks a third time per checkpoint.
        if(oldest >= MAXPE){
            if (rtgc_mode){
                printf("[%ld] RTGC MAXPE reached\n", cur_cp);
                rtgc_exit_reason = RTGC_EXIT_MAXPE;
                goto RTGC_EXIT_LOG;
            }
            total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
            printf("[%ld]a block reach maximum P/E, util : %d\n", cur_cp, total_u);
            fprintf(fplife,"%ld,",cur_cp);
            exit_code = 1;
            goto CLEANUP;
        }

        // execution order must be (req completion --> job release --> req pick)
        
        // 3. req completion logic
        if(cur_IO_end == cur_cp){
            if(cur_IO != NULL){
                // a logic to handle I/O to finish
                if(cur_IO->type == GCER){
                    //for(int i=0;i<4;i++){
                    //    print_hotdist_profile(fps[tasknum+i],tasks,cur_cp, newmeta,-1,i);
                    //}
                    //print_freeblock_profile(fps[tasknum+4],cur_cp,newmeta,fblist_head,write_head);
                    // total_u = print_profile(tasks,tasknum,cur_IO->taskidx,newmeta,fps[cur_IO->taskidx],yngest,oldest,cur_cp,
                    //                 cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                    //                 cur_wb[cur_IO->taskidx],fblist_head,write_head,
                    //                newmeta->total_fp,cur_IO->gc_valid_count);
                    total_u = profile(tasks,tasknum,cur_IO->taskidx,newmeta,yngest,oldest,cur_cp,
                                    cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                                    cur_wb[cur_IO->taskidx],fblist_head,write_head,
                                   newmeta->total_fp,cur_IO->gc_valid_count);

                    // utilization overflow 2 (exit code) — LaWL-analysis
                    // gate, intentionally bypassed for RTGC mode because
                    // RTGC uses its own analysis at the 1 ms tick above.
                    if(!rtgc_mode && total_u > 1.0){
                        printf("[%ld]utilization overflow 2, util : %f\n",cur_cp, total_u);
                        gettimeofday(&tot_end_time,NULL);
                        tot_runtime = tot_end_time.tv_sec * 1000000 + tot_end_time.tv_usec - tot_start_time.tv_sec * 1000000 - tot_start_time.tv_usec;
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
                        print_profile_updaterate(newmeta,updaterate_fp);
                        exit_code = 1;
                        goto CLEANUP;
                    }
                }

                // if last req is finished, do the following
                if(cur_IO->last == 1){

                    // check I/O latency
                    //check_latency(lat_log_w,lat_log_r,lat_log_gc,cur_IO,cur_cp);

                    // deadline miss overflow (exit code)
                    if(check_dl_violation(tasks,cur_IO,cur_cp)==1){
                        if (rtgc_mode){
                            printf("[%ld] RTGC deadline miss (task %d, type %d)\n",
                                    cur_cp, cur_IO->taskidx, cur_IO->type);
                            rtgc_exit_reason = RTGC_EXIT_DEADLINE_MISS;
                            goto RTGC_EXIT_LOG;
                        }
                        fprintf(fplife,"%ld,",cur_cp);
                        fflush(fplife);
                        printf("dl miss detected,");
                        exit_code = 1;
                        goto CLEANUP;
                    }

                    // set finish flags for scheduler, 
                    // and if current job is delayed, check if next release is possible.
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
                            if(wjob_deferred[a] == 1){ // 지연된 write job이 있는지 확인
                                wjob_deferred[a] = 0;
                                // if current write is delayed, check if write release is possible
                                if(cur_cp >= cur_IO->deadline){
                                    next_w_release[a] = cur_cp;
                                } 
                            }
                        }
                    }
                }
                // set finish flags of current rr to shedule new WL
                if(rr->reqnum == 0){
                    rr_finished = 1;
                }
                // set start flags to schedule new WL
                if(cur_IO->type == WR && cur_IO->islastreq == 1){
                    do_rr = 1;
                    wr_end_bwr_flag = 1;
                }
                // finish request
                finish_req(tasks, cur_IO, newmeta,
                           fblist_head, rsvlist_head, full_head,
                           &(cur_GC[cur_IO->taskidx]),&(cur_rr));

                // RTGC token bookkeeping (Fig.5).  Order matters:
                //   - WR consume happens after finish_WR updates page maps
                //     (finish_req -> finish_WR just decremented total_fp).
                //   - GCER post-recycle bookkeeping runs after finish_GCER
                //     replenished total_fp, so log entries reflect the
                //     post-erase Phi.
                if (rtgc_mode && cur_IO != NULL){
                    if (cur_IO->type == WR){
                        rtgc_write_consume(&rtgc_st, cur_IO->taskidx, 1);
                        // Do not log per-page write (too chatty); only tag
                        // the last write of a job.
                        if (cur_IO->last == 1 && rtgc_token_fp){
                            rtgc_log_tokens(&rtgc_st, newmeta, cur_cp,
                                            "write_job", cur_IO->taskidx,
                                            rtgc_token_fp);
                        }
                    } else if (cur_IO->type == GCER){
                        rtgc_gc_after_recycle(&rtgc_st, cur_IO->taskidx,
                                              cur_IO->gc_valid_count,
                                              cur_cp, rtgc_token_fp);
                        // After tokens are replenished, retry any deferred
                        // writes so they can release on the next tick.
                        for (int a = 0; a < tasknum; a++){
                            if (wjob_deferred[a] &&
                                rtgc_write_can_release(&rtgc_st, a, tasks[a].wn)){
                                wjob_deferred[a] = 0;
                            }
                        }
                    }
                }

                // reset I/O pointer and IO end time tracker
                free(cur_IO);
                cur_IO = NULL;
                cur_IO_end = __LONG_MAX__;
                IO_end_bwr_flag = 1; // notify that IO is ended
            }
        }

        // 4. job release logic
        // 4-(1). release I/O task jobs (실제 실행하는 것 X, release job을 request queue에 저장하는 과정)
        for(int j=0;j<tasknum;j++){

            // 4-(1)-1. write job을 실행하기에 충분한 free page가 있는지 먼저 확인
            //   (free page < write request page) 이면 write job을 release하는 걸 delay
            if (rtgc_mode){
                // Paper §4.4 guarantees Phi is always sufficient when Ti has
                // enough tokens.  Track per-tick token availability; must
                // RESET to 0 as soon as tokens become sufficient again
                // (via virtual harvest or metaperiod bookkeeping) — non-RTGC
                // schemes get this for free from GCER completion but RTGC
                // does most of its GC as virtual harvest.
                if (rtgc_write_can_release(&rtgc_st, j, tasks[j].wn)){
                    wjob_deferred[j] = 0;
                } else {
                    if (!wjob_deferred[j]) rtgc_st.n_write_deferred_by_token++;
                    wjob_deferred[j] = 1;
                }
            } else if(newmeta->total_fp < newmeta->reserved_write + tasks[j].wn){  // free page < write request page
                printf("%d task write deferred\n",j);  // delay the write request due to the GC for reclaiming the free page
                wjob_deferred[j] = 1;
            }

            // 4-(1)-2. write job release (cur_cp == next_w_release[idx])
            if(cur_cp == next_w_release[j]){
                // 1-(1). previous write job finish, no delayed write job
                if (wjob_finished[j] == 1 && wjob_deferred[j] == 0){
                    gettimeofday(&(algo_start_time),NULL);
                    cur_wb[j] = write_job_start_q(tasks, j, tasknum, newmeta, 
                                                fblist_head, full_head, write_head,
                                                w_workloads[j], wq[j], cur_wb[j], wflag, cur_cp); // return last access block
                    write_release_num++;
                    gettimeofday(&(algo_end_time),NULL);
                    write_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
		    next_w_release[j] = cur_cp + (long)tasks[j].wp; // next write request는 write period 후에 release
                    wjob_finished[j] = 0; // 수행 중인 write request가 있음을 나타내는 flag
                }
                // 2-(2). previous write job is not finished (wjob_finished[idx] == 0)
                // 2-(3). delayed write job remains (wjob_deferred[idx] == 1)
                else if (wjob_finished[j] == 0 || wjob_deferred[j] == 1){
                    next_w_release[j] = cur_cp + (long)tasks[j].wp;
                }
            } 
            
            // 4-(1)-3. read job release (cur_cp == next_r_release[idx])
            if(cur_cp == next_r_release[j]){
                // 3-(1). previous read job finish and new read request release
                if (rjob_finished[j] == 1){ 
                //printf("next r : %ld, cur cp : %ld, rjob : %d\n",next_r_release[j],cur_cp,rjob_finished[j]);
                    read_job_start_q(tasks,j,newmeta,
                                    r_workloads[j],rq[j], cur_cp);
                    next_r_release[j] = cur_cp + (long)tasks[j].rp;
                    rjob_finished[j] = 0;
                } 
                // 3-(2). previous read job is not finished
                else if (rjob_finished[j] == 0){
                    next_r_release[j] = cur_cp + (long)tasks[j].rp;
                }
            }

            // 4-(1)-4. gc job release (cur_cp == next_gc_release[idx])
            if(cur_cp == next_gc_release[j]){
                // 4-(1). previous gc job finish
                if(gcjob_finished[j] == 1){
                    int do_gc = 0;
                    if (rtgc_mode){
                        // Paper Fig.5: (Phi - rho) >= alpha  ⇒  virtual harvest,
                        // no I/O.  Otherwise perform a real recycle.
                        if (tasks[j].wn > 0){
                            do_gc = rtgc_gc_release(&rtgc_st, j, newmeta,
                                                     cur_cp, rtgc_token_fp);
                        } else {
                            do_gc = 0;  // no G_i for tasks with wn==0
                        }
                    } else if(newmeta->total_fp <= expected_fp){
                        do_gc = 1;
                    }

                    if(do_gc){
                        // printf("total_invalid : %d,expected_invalid : %d\n",newmeta->total_invalid,expected_invalid);
                        // printf("total_fp : %d, expected_fp : %d\n",newmeta->total_fp,expected_fp);
                        // printf("blocknum : %d, %d, %d\n",fblist_head->blocknum,full_head->blocknum,write_head->blocknum);

                        // GC release 시점의 시간 check
                        gettimeofday(&(algo_start_time),NULL);
                        gc_job_start_q(tasks, j, tasknum, newmeta,
                                    fblist_head, full_head, rsvlist_head, write_head, 0,
                                    gcq[j], &(cur_GC[j]), gcflag, cur_cp);
                        gc_release_num++;
                        gettimeofday(&(algo_end_time),NULL);
                        // fprintf(fpovhd_gc, "%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
                        gc_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
			next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                        gcjob_finished[j] = 0;
                    }
                    else {
                        next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                        gcjob_finished[j] = 1;
                    }
                }
                // 4-(2). previous gc job is not finished
                else if (gcjob_finished[j] == 0){
                    next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                }
            }

            // 4-(1)-5. RTGC meta-period boundary check. Runs LAST in the
            // per-task iteration so that any tokens harvested by GC
            // (virtual harvest path in rtgc_gc_release) or consumed by
            // completed writes earlier this tick are reflected in ρ_T[j]
            // before we shed the excess (paper §3.3.2, Fig.4).
            // while() because find_next_time may skip multiple boundaries
            // when the sim is idle across long stretches.
            if (rtgc_mode){
                while (cur_cp >= rtgc_st.next_sigma[j]){
                    rtgc_task_metaperiod_boundary(&rtgc_st, j, tasks);
                    if (rtgc_token_fp) rtgc_log_tokens(&rtgc_st, newmeta,
                                                       cur_cp, "metaperiod",
                                                       j, rtgc_token_fp);
                    rtgc_st.next_sigma[j] += rtgc_st.sigma[j];
                }
            }
        }

        // 4-(2). release WL jobs
        if (rrflag == 3){
            // RTGC non-real-time wear leveler (§3.4.2).  Independent of the
            // LaWL/Hybrid do_rr/wl_init/hot-cold pipeline: gates only on
            // (a) mandatory sleep timer, (b) empty WL queue.  rrflag == -1
            // (SKIPRR) skips this branch entirely for the WL ablation.
            if (rtgc_mode && rr->head == NULL && cur_cp >= rtgc_st.wl_next_copy_time){
                gettimeofday(&(algo_start_time),NULL);
                int enq = rtgc_wl_release(&rtgc_st, newmeta,
                                          fblist_head, full_head,
                                          rr, cur_cp, rtgc_wl_fp);
                if (enq) rr_release_num++;
                gettimeofday(&(algo_end_time),NULL);
                rr_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
                if (rr->reqnum != 0) rr_finished = 0;
                else                  rr_finished = 1;
            }
        } else {
            // 4-(2)-1. relocation start 조건 확인 (PEC variation이 큰가?)
            if(oldest-yngest >= THRESHOLD){     // WL start signal
                wl_init = 1;
            }
            // 4-(2)-2. relocation request 생성
            if((do_rr == 1) && (rr_finished == 1) && (rr->head == NULL) && (rrflag != -1) && (wl_init == 1)){
                if(hot_cold_list == 0){
                    build_hot_cold(newmeta,hotlist,coldlist);
                    hot_cold_list = 1;
                }
                // rrutil = 1.0 - find_worst_util(tasks,tasknum,newmeta);
                rrutil = -1.0;                  // override util so that WL always run in background mode.
                gettimeofday(&(algo_start_time),NULL);
                RR_job_start_q(tasks, tasknum, newmeta, fblist_head, full_head, hotlist, coldlist,
                                rr,&(cur_rr),(double)rrutil,cur_cp, skewnum);
                rr_release_num++;
    	    gettimeofday(&(algo_end_time),NULL);
    	    rr_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
    	    // fprintf(fpovhd_rr, "%ld\n", algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
                if(rr->reqnum != 0){
                    rr_finished = 0;
                }
                else {
                    rr_finished = 1;
                }
                do_rr = 0;
            }
        }
        
        // 5. req pick logic
        if(cur_IO == NULL){
            // 5-1. init params
            long cur_dl = __LONG_MAX__;
            int target_task = -1;
            int target_type = -1;

            // 5-2. iterate through per-task queues and pick the I/O with earliest deadline.
            // operation priority is RR < GC < W < R.
            // note that deadline is updated when dl is "less than" cur_dl
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
            // 5-3. pop IO from target task's queue
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

            // 5-4. if something's popped out, update cur_IO_end
            if(cur_IO != NULL){
                cur_IO_end = cur_cp + cur_IO->exec;
            }
            else {
                cur_IO_end = __LONG_MAX__;
            }
        }
        
        // 6. go to the next checkpoint
        // cur_cp는 앞에서 EDF scheduling에 따라 실행된 job이 끝난 지점으로 jump되어 있음
        // jump한 시점보다 더 앞에 release되어야 할 job이 있다면, 해당 시점으로 cur_cp를 jump
        cur_cp = find_next_time(tasks,tasknum,cur_IO_end,rr_check,cur_cp,
                                next_w_release,next_r_release,next_gc_release);
        // RTGC-only event boundaries (meta-period, WL sleep expiry).
        // Kept as a post-hoc min() so find_next_time's signature stays
        // unchanged for the seven existing schemes.
        if (rtgc_mode){
            long rtgc_next = rtgc_next_event_time(&rtgc_st, cur_cp - 1);
            if (rtgc_next < cur_cp) cur_cp = rtgc_next;
        }
        // printf("[fnt res]next_time : %ld\n",cur_cp);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    if (rtgc_mode){
        rtgc_exit_reason = RTGC_EXIT_RUNTIME_END;
        goto RTGC_EXIT_LOG;
    }
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    goto CLEANUP;

RTGC_EXIT_LOG:
    // Single terminal-row writer for all RTGC exit paths.  cur_cp,
    // rtgc_exit_reason, rtgc_lat_mode, and the cached U samples all
    // reflect the moment we decided to exit.
    if (rtgc_mode){
        rtgc_log_lifetime(fplife, cur_cp, rtgc_exit_reason, rtgc_lat_mode,
                           rtgc_u_admission, rtgc_u_last_selected,
                           rtgc_u_last_pecmax, rtgc_u_last_lawl,
                           oldest, yngest);
        // Overhead row (matches format of the other schemes).
        gettimeofday(&tot_end_time, NULL);
        tot_runtime = tot_end_time.tv_sec * 1000000 + tot_end_time.tv_usec
                    - tot_start_time.tv_sec * 1000000 - tot_start_time.tv_usec;
        tot_runtime_readable = (double)tot_runtime / 1000.0 / 1000.0 / 60.0;
        write_ovhd_avg = write_release_num ? (double)write_ovhd_sum / (double)write_release_num : 0.0;
        gc_ovhd_avg    = gc_release_num    ? (double)gc_ovhd_sum    / (double)gc_release_num    : 0.0;
        rr_ovhd_avg    = rr_release_num    ? (double)rr_ovhd_sum    / (double)rr_release_num    : 0.0;
        if (fpovhd){
            fprintf(fpovhd, "%ld, %ld, %ld, ", write_release_num, gc_release_num, rr_release_num);
            fprintf(fpovhd, "%lf, %lf ,%lf, %lf\n", write_ovhd_avg, gc_ovhd_avg, rr_ovhd_avg, tot_runtime_readable);
            fflush(fpovhd);
        }
        if (updaterate_fp) print_profile_updaterate(newmeta, updaterate_fp);
        exit_code = (rtgc_exit_reason == RTGC_EXIT_RUNTIME_END) ? 0 : 1;
    }

    CLEANUP:
    // 1) 진행 중 I/O 있으면 정리
    // cur_IO는 finish_req에서 free하지만, 혹시 남아있으면 방어적으로 free
    if (cur_IO) { free(cur_IO); cur_IO = NULL; }

    // 2) 로그 파일 닫기 (열었던 것만)
    //     if (fplife) fclose(fplife);
    //     if (fpovhd) fclose(fpovhd);
    //     for (int i = 0; i < tasknum; i++) if (fps && fps[i]) fclose(fps[i]);
    //     lat_close(...) 유틸이 있다면 호출

    // 3) I/O 큐들 free: per-task heads + 단일 head
    if (wq) {
        for (int i = 0; i < tasknum; i++) if (wq[i]) ll_free_IO(wq[i]);
        free(wq);
        wq = NULL;
    }
    if (rq) {
        for (int i = 0; i < tasknum; i++) if (rq[i]) ll_free_IO(rq[i]);
        free(rq);
        rq = NULL;
    }
    if (gcq) {
        for (int i = 0; i < tasknum; i++) if (gcq[i]) ll_free_IO(gcq[i]);
        free(gcq);
        gcq = NULL;
    }
    // 단일 큐 head
    if (rr)  { ll_free_IO(rr);  rr  = NULL; }
    if (bwr) { ll_free_IO(bwr); bwr = NULL; }

    // 4) 비율 배열 free
    if (w_prop) { free(w_prop); w_prop = NULL; }
    if (r_prop) { free(r_prop); r_prop = NULL; }
    if (gc_prop){ free(gc_prop); gc_prop = NULL; }

    // 5) task 메모리 free
    if (rand_tasks) { free(rand_tasks); rand_tasks = NULL; }
    if (tasks)      { free(tasks);      tasks = NULL; }

    // 6) workload 파일 배열 free (IO_open/IO_close가 파일을 닫는다면, 여기서는 배열만 free)
    if (w_workloads) { free(w_workloads); w_workloads = NULL; }
    if (r_workloads) { free(r_workloads); r_workloads = NULL; }

    // 7) metadata free (destroy 함수가 있으면 그걸 호출)
    // if (newmeta) destroy_metadata(newmeta);
    if (newmeta) { free(newmeta); newmeta = NULL; }

    // 8) RTGC state free (safe on zero-init if rtgc_mode==0)
    if (rtgc_mode) {
        rtgc_free(&rtgc_st);
        if (rtgc_token_fp)      { fclose(rtgc_token_fp);      rtgc_token_fp      = NULL; }
        if (rtgc_wl_fp)         { fclose(rtgc_wl_fp);         rtgc_wl_fp         = NULL; }
        if (rtgc_admission_fp)  { fclose(rtgc_admission_fp);  rtgc_admission_fp  = NULL; }
    }

    return exit_code;
}
