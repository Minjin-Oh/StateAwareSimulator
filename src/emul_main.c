#include "stateaware.h"     // contains 
#include "init.h"           // contains init function for various structure params
#include "emul.h"           // contains request process functions for emulation 
#include "findRR.h"         // contains block selection functions
#include "IOgen.h"          // contains random workload generation functions
#include "emul_logger.h"    // contains latency logger functions

//globals
block* cur_fb = NULL;
int rrflag = 0;
int MINRC;              //minimum reclaimable page, assigned by set_exec_flags in parse.c : 35
double OP;              //overprovisioning rate, assigned by set_exec_flags in parse.c : 0.32
int THRES_COLD = 35;    //a global threshold for write-cold block determination, used in find_WR_target_simple() in findRR.c
int THRES_HOT = 300;
int prev_erase = 0;       //a flag for interval specification of threshold update, used in find_WR_target_simple() in findRR.c
int prev_mincyc = 0;      //a parameter for minimum cycle block comparison, used in find_WR_target_simple() in findRR.c ((이거 주석처리 되어있음. 더 이상 사용하지 않는 변수인가?)
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

//FIXME:: set these as global to expose proportion (ratio) array to find_write_gradient function in findW.c
double* w_prop;
double* r_prop;
double* gc_prop;

//FIXME:: set these as global to expose workloads to find_write_invalid function
FILE** w_workloads;
FILE** r_workloads;

//FIXME:: set these as global to expose log file pointers and system parameters
long cur_cp; // ??
int max_valid_pg;
int tot_longlive_cnt = 0;
FILE **fps;
FILE *test_gc_writeblock[4];
FILE *updaterate_fp;
FILE *longliveratio_fp;
FILE *updateorder_fp;
FILE *getupdateorder_fp;

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
    int rrcond = 0; 
    int tasknum;
    int genflag = 0;
    int taskflag = 0;
    int profflag = 0;
    int skewness;                    //skew of utilization(-1 = noskew, 0 = read-skewed, 1 = write-skewed)
    int skewnum;                     //number of skewed task
    int OPflag;
    int init_cyc = 0;
    float totutil;                   //a total utilization of current system
    //get flags
    set_scheme_flags(argv,
                     &gcflag, &wflag, &rrflag, &rrcond);
    set_exec_flags(argv, &tasknum, &totutil,
                   &genflag, &taskflag, &profflag,
                   &skewness, &sploc, &tploc, &skewnum,
                   &OPflag, &init_cyc, &OP, &MINRC);
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
    long cur_IO_end = __LONG_MAX__;  //absolute time when current req finishes
    float WU;                        //worst-case utilization tracker.
    float rrutil;                    //a utilization allowed to data relocation job
    cur_cp = 0;                      //current checkpoint time
    
    //log file pointers
    FILE* rr_profile;
    FILE *fp, *fplife, *fpwrite, *fpread, *fprr, *fpovhd;
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
    for(int i=0;i<tasknum;i++){ // 1(yes), 0(no)
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
    
    //TEMPCODE::open file for invfull block check.
    longliveratio_fp = fopen("longliveratio.csv","w");

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
            if(skewness == -1){ // UUNIFAST algorithm
                rand_tasks = generate_taskset(tasknum,totutil,max_valid_pg,&res,0); 
            }
            else if (skewness == -2){ //manually assign value for taskset(hardcode). edit parameters for test. (using the taskparam.csv)
                rand_tasks = generate_taskset_hardcode(tasknum,max_valid_pg,&res);
            }
            else if(skewness >= 0){
                rand_tasks = generate_taskset_skew2(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
            }
            else if(skewness == -3){ //manually assign w/r utilization for each task. edit parameters for test.
                rand_tasks = generate_taskset_fixed(max_valid_pg,&res);
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
    fps = open_file_pertask(gcflag,wflag,rrflag,tasknum);
    rr_profile = fopen("rr_prof.csv","w");
    fplife = fopen("lifetime.csv","a");
    fpovhd = fopen("overhead.csv","a");
    updateorder_fp = fopen("updateorder.csv", "w");
    IO_open(tasknum, w_workloads,r_workloads);
    lat_open(tasknum, lat_log_w,lat_log_r,lat_log_gc);
    for(int i=0;i<tasknum;i++){
        fprintf(fps[i],"%s\n","timestamp,taskidx,WU,new_WU,noblock,w_util,r_util,g_util,old,yng,bidx,state,vp,w_idx,w_state,fb,w");
    }
    fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum, init_cyc);

#ifdef GC_ON_WRITEBLOCK  // write 시점에 GC trigger (GC에 사용할 reserved block이 없음)
    fblist_head = init_blocklist(0,NOB-1);
    rsvlist_head = init_blocklist(0,-1);
#endif
#ifndef GC_ON_WRITEBLOCK  // classic GC scheduling 기반
    fblist_head = init_blocklist(0, NOB-tasknum-1);     // free block list
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);   // reserved block list : for using GC copy
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
    FILE* u_check = fopen("rrchecker.csv","w");
#ifdef utilsort_writecheck
    for(int i=0;i<4;i++){
        char testgcwriteblockname[20];
        sprintf(testgcwriteblockname,"wbtest_%d.csv",i);
        test_gc_writeblock[i] = fopen(testgcwriteblockname,"w");
    }
#endif
    updaterate_fp = fopen("updaterate.csv","w");
    gettimeofday(&(tot_start_time),NULL);
    //start of simulation
    while(cur_cp <= RUNTIME){
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);
        //flash state checker
        if(cur_cp % 1000000L == 0){
            total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
            //printf("cur_u:%f\n",total_u);
            //utilization overflow 1(exit code)
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
                sleep(1);
                return 1;
            }
        }
        // max P/E cycle overflow (exit code)
        for(int idx=0;idx<NOB;idx++){
            if(newmeta->state[idx] >= MAXPE){
                total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
                printf("[%ld]a block reach maximum P/E, util : %d\n", cur_cp, total_u);
                fprintf(fplife,"%ld,",cur_cp);
                sleep(1);
                return 1;
            } else {
                /*do nothing*/
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
                    total_u = print_profile(tasks,tasknum,cur_IO->taskidx,newmeta,fps[cur_IO->taskidx],yngest,oldest,cur_cp,
                                    cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                                    cur_wb[cur_IO->taskidx],fblist_head,write_head,
                                    newmeta->total_fp,cur_IO->gc_valid_count);
                    //utilization overflow 2(exit code)
                    if(total_u > 1.0){
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
                        sleep(1);
                        return 1;
                        
                    }
                }
                
                //if last req is finished, do the following
                if(cur_IO->last == 1){
                    //check I/O latency
                    check_latency(lat_log_w,lat_log_r,lat_log_gc,cur_IO,cur_cp);
                    // deadline miss overflow (exit code)
                    if(check_dl_violation(tasks,cur_IO,cur_cp)==1){
                        fprintf(fplife,"%ld,",cur_cp);
                        fflush(fplife);
                        printf("dl miss detected,");
                        sleep(1);
                        return 1;
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
                            if(wjob_deferred[a] == 1){ // 지연된 write job이 있는지 확인
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
            if(newmeta->total_fp < newmeta->reserved_write + tasks[j].wn){  // free page < write request page
                printf("%d task write deferred\n",j);  // delay the write request due to the GC for reclaiming the free page
                wjob_deferred[j] = 1;
            }
            if(cur_cp == next_w_release[j] && wjob_finished[j] == 1 && wjob_deferred[j] == 0){ // previous write job finish, no delayed write job
                gettimeofday(&(algo_start_time),NULL);
                cur_wb[j] = write_job_start_q(tasks, j, tasknum, newmeta, 
                                              fblist_head, full_head, write_head,
                                              w_workloads[j], wq[j], cur_wb[j], wflag, cur_cp); // return last access block
                write_release_num++;
                gettimeofday(&(algo_end_time),NULL);
                //printf("wovhd:%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
                write_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
                next_w_release[j] = cur_cp + (long)tasks[j].wp; // next write request는 write period 후에 release
                wjob_finished[j] = 0; // 수행 중인 write request가 있음을 나타내는 flag
            } 
            else if (cur_cp == next_w_release[j] && wjob_finished[j] == 0){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            } 
            else if (cur_cp == next_w_release[j] && wjob_deferred[j] == 1){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            }

            if(cur_cp == next_r_release[j] && rjob_finished[j] == 1){ // previous read job finish and new read request release
                //printf("next r : %ld, cur cp : %ld, rjob : %d\n",next_r_release[j],cur_cp,rjob_finished[j]);
                read_job_start_q(tasks,j,newmeta,
                                 r_workloads[j],rq[j], cur_cp);
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
                    gettimeofday(&(algo_start_time),NULL);
                    gc_job_start_q(tasks, j, tasknum, newmeta,
                               fblist_head, full_head, rsvlist_head, write_head, 0,
                               gcq[j], &(cur_GC[j]), gcflag, cur_cp);
                    gc_release_num++;
                    gettimeofday(&(algo_end_time),NULL);
                    //printf("gcovhd:%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
                    gc_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
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
        if((do_rr == 1) && (rr_finished == 1) && (rr->head == NULL) && (rrflag != -1) && (wl_init == 1)){
            if(hot_cold_list == 0){
                build_hot_cold(newmeta,hotlist,coldlist);
                hot_cold_list = 1;
            }
            //rrutil = 1.0 - find_worst_util(tasks,tasknum,newmeta);
            rrutil = -1.0; //override util so that WL always run in background mode.
            gettimeofday(&(algo_start_time),NULL);
            RR_job_start_q(tasks, tasknum, newmeta, fblist_head, full_head, hotlist, coldlist,
                            rr,&(cur_rr),(double)rrutil,cur_cp);
            rr_release_num++;
            gettimeofday(&(algo_end_time),NULL);
            //printf("rrovhd:%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
            rr_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
            if(rr->reqnum != 0){
                rr_finished = 0;
            } 
            else {
                rr_finished = 1;
            }
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
        cur_cp = find_next_time(tasks,tasknum,cur_IO_end,rr_check,cur_cp,
                                next_w_release,next_r_release,next_gc_release);
        //printf("[fnt res]next_time : %ld\n",cur_cp);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    sleep(1);
    return 0;
}
