#include "stateaware.h"     // contains 
#include "init.h"           // contains init function for various structure params
#include "emul.h"           // contains request process functions for emulation 
#include "findRR.h"         // contains block selection functions
#include "IOgen.h"          // contains random workload generation functions
#include "emul_logger.h"    // contains latency logger function.

//globals
block* cur_fb = NULL;
int rrflag = 0;
int MINRC;
double OP;

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

//FIXME:: set these as global to expose log file pointers and system parameters
long cur_cp;
int max_valid_pg;
FILE **fps;
FILE *test_gc_writeblock[4];
FILE *updaterate_fp;

int* IO_resetflag;
long* IO_resetpoint;

//FIXME:: set these as global to expose global write block to assign_write_invalid function
block** b_glob_young;
block** b_glob_old;
bhead* glob_yb;
bhead* glob_ob;
int main(int argc, char* argv[]){
    //init params
    srand(time(NULL)); 
    bhead* fblist_head;                             //heads for block list
    bhead* rsvlist_head;
    bhead* full_head;
    bhead* write_head;
    bhead* hotlist;                                 //(for WL) overlaps previous blocklist
    bhead* coldlist;
    meta* newmeta = (meta*)malloc(sizeof(meta));    //metadata structure
    
    //initialize flag variables first.
    int gcflag = 0;
    int wflag = 0;
    int rrcond = 0; 
    int tasknum;
    float totutil;                   //a total utilization of current system
    int genflag = 0;
    int taskflag = 0;
    int profflag = 0;
    int skewness;                    //skew of utilization(-1 = noskew, 0 = read-skewed, 1 = write-skewed)
    int skewnum;                     //number of skewed task
    int OPflag;
    int init_cyc = 0;
    
    //get flags
    set_scheme_flags(argv,
                     &gcflag, &wflag, &rrflag, &rrcond);
    set_exec_flags(argv, &tasknum, &totutil,
                   &genflag, &taskflag, &profflag,
                   &skewness, &sploc, &tploc, &skewnum,
                   &OPflag, &init_cyc, &OP, &MINRC);
    
    //initialize misc variables
    int g_cur = 0;                   //pointer for current writing page
    int last_task = tasknum-1;       //index of last I/O task
    float rrutil;                    //a utilization allowed to data relocation job
    cur_cp = 0;                      //current checkpoint time
    long cur_IO_end = __LONG_MAX__;  //absolute time when current req finishes
    int wl_init = 0;                 //flag for wear-leveling initiation
    float WU;                        //worst-case utilization tracker.
    int do_rr = 0;
    int rr_finished = 1;
    int hot_cold_list = 0;
    
    FILE* rr_profile;
    FILE* w_workloads[tasknum];
    FILE* r_workloads[tasknum];
    FILE *fp, *fplife, *fpwrite, *fpread, *fprr, *fpovhd;
    b_glob_old = (block**)malloc(sizeof(block*)*tasknum);
    b_glob_young = (block**)malloc(sizeof(block*)*tasknum);
    for(int i=0;i<tasknum;i++){
        b_glob_old[i] = NULL;
        b_glob_young[i] = NULL;
    }
    IO_resetflag = (int*)malloc(sizeof(int)*tasknum);
    IO_resetpoint = (long*)malloc(sizeof(long)*tasknum);
    for(int i=0;i<tasknum;i++){
        IO_resetflag[i] = 0;
        IO_resetpoint[i] = 0L;
    }
    block *cur_wb[tasknum];
    GCblock cur_GC[tasknum];
    RRblock cur_rr;

    rttask* rand_tasks = NULL;
    rttask* tasks = NULL;
    w_prop = (double*)malloc(sizeof(double)*tasknum*2);
    r_prop = (double*)malloc(sizeof(double)*tasknum*2);
    gc_prop = (double*)malloc(sizeof(double)*tasknum*2);
    //simulate I/O(init related params)
    float total_u;
    int oldest;
    int yngest;
    int over_avg = 0;
    long rr_check = (long)100000;
    //long runtime = 160000000000;
    //long init_runtime = 160000000000;
    long runtime = WORKLOAD_LENGTH;
    long init_runtime = WORKLOAD_LENGTH;
    IO* cur_IO = NULL;
    wq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    rq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    gcq = (IOhead**)malloc(sizeof(IOhead*)*tasknum);
    IOhead* rr;
    FILE* lat_log_w[tasknum];
    FILE* lat_log_r[tasknum];
    FILE* lat_log_gc[tasknum];
    //FILE* finish_log;
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
    int expected_invalid = MINRC*(NOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d, maxvalid : %d\n",expected_invalid,expected_fp,max_valid_pg);
    
    //TASK GENERATOR CODE
    if(taskflag == 1){ //generate taskset and save
        float res = 1.0;
        while(res >= 1.0){
            if(skewness == -1){
                rand_tasks = generate_taskset(tasknum,totutil,max_valid_pg,&res,0);
            } else if (skewness == -2){ //manually assign value for taskset(hardcode). edit parameters for test.
                rand_tasks = generate_taskset_hardcode(tasknum,max_valid_pg);
                res = 0.5;//just hardcode and pass
            } else if(skewness >= 0){
                rand_tasks = generate_taskset_skew2(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
            } else if(skewness == -3){ //manually assign w/r utilization for each task. edit parameters for test.
                rand_tasks = generate_taskset_fixed(max_valid_pg,&res);
            }
            if(res > 1.0){
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
        IOgen(tasknum,rand_tasks,runtime,offset,sploc,tploc);
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
    //run gradient tests for write in offline, and assign offset value for WGRAD policy.
    _find_rank_lpa(tasks,tasknum);
    offset = (int)((float)(tasks[0].addr_ub - tasks[0].addr_lb)*sploc/2.0);

    //init csv files
    fps = open_file_pertask(gcflag,wflag,rrflag,tasknum);
    rr_profile = fopen("rr_prof.csv","w");
    fplife = fopen("lifetime.csv","a");
    fpovhd = fopen("overhead.csv","a");
    //finish_log = fopen("log.csv","w");
    IO_open(tasknum, w_workloads,r_workloads);
    lat_open(tasknum, lat_log_w,lat_log_r,lat_log_gc);
    
    for(int i=0;i<tasknum;i++){
        fprintf(fps[i],"%s\n","timestamp,taskidx,WU,new_WU,noblock,w_util,r_util,g_util,old,yng,bidx,state,vp,w_idx,w_state,fb,w");
    }
    fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    //fprintf(fpread,"%s\n","timestamp");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum, init_cyc);
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
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
    int logi = (int)(NOP*(1-OP));
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
    ll_append(fblist_head,cur_fb);
    //!!finish initialization

    //run simulation
    FILE* u_check = fopen("rrchecker.csv","w");
    for(int i=0;i<4;i++){
        char testgcwriteblockname[20];
        sprintf(testgcwriteblockname,"wbtest_%d.csv",i);
        test_gc_writeblock[i] = fopen(testgcwriteblockname,"w");
    }
    updaterate_fp = fopen("updaterate.csv","w");
    gettimeofday(&(tot_start_time),NULL);
    while(cur_cp <= runtime){
        //if current time is near end of workload window, rewind to first of workload
        if((double)cur_cp >= (double)runtime * 0.95){
            long prev_run;
            printf("curcp:%ld,runtime:%ld\n",cur_cp,runtime);
            for(int i=0;i<tasknum;i++){
                rewind(r_workloads[i]);
                rewind(w_workloads[i]);
                IO_resetflag[i] = 1;
            }
            prev_run = runtime;
            runtime = runtime + init_runtime;
            printf("rewind check, prev run : %ld, next run : %ld\n",prev_run, runtime);
           // sleep(1);
        }
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);
        //flash state checker
        if(cur_cp % 1000000L == 0){
            total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
            
            if(total_u >= 1.0){
                printf("[%ld]utilization overflow, util : %f\n",cur_cp, total_u);
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
        for(int idx=0;idx<NOB;idx++){
            if(newmeta->state[idx] >= MAXPE){
                //total_u = print_profile_timestamp(tasks,tasknum,newmeta,u_check,yngest,oldest,cur_cp);
                //printf("[%ld]a block reach maximum P/E, util : %d\n",total_u);
                //fprintf(fplife,"%ld,",cur_cp);
                //sleep(1);
                //return 1;
            } else {
                /*do nothing*/
            }
        }
        //execution order must be (req completion --> job release --> req pick)
        
        //req completion logic
        if(cur_IO_end == cur_cp){
            if(cur_IO != NULL){
                /* a logic to handle I/O finish*/
                if(cur_IO->type == GCER){
                    //for(int i=0;i<4;i++){
                    //    print_hotdist_profile(fps[tasknum+i],tasks,cur_cp, newmeta,-1,i);
                    //}
                    //print_freeblock_profile(fps[tasknum+4],cur_cp,newmeta,fblist_head,write_head);
                    total_u = print_profile(tasks,tasknum,cur_IO->taskidx,newmeta,fps[cur_IO->taskidx],yngest,oldest,cur_cp,
                                    cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                                    cur_wb[cur_IO->taskidx],fblist_head,write_head,
                                    newmeta->total_fp,cur_IO->gc_valid_count);
                    
                    if(total_u > 1.0){
                        printf("[%ld]utilization overflow, util : %f\n",cur_cp, total_u);
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
                }
                //finish request
                finish_req(tasks, cur_IO, newmeta, 
                           fblist_head, rsvlist_head, full_head, 
                           &(cur_GC[cur_IO->taskidx]),&(cur_rr));


                //reset I/O pointer and IO end time tracker
                cur_IO = NULL;
                cur_IO_end = __LONG_MAX__;
            }
        }

        //job release logic
        //release I/O task jobs
        for(int j=0;j<tasknum;j++){
            //printf("[%d]fin flags : %d, %d, %d\n",j,wjob_finished[j],rjob_finished[j],gcjob_finished[j]);
        }
        for(int j=0;j<tasknum;j++){
            if(newmeta->total_fp < newmeta->reserved_write + tasks[j].wn){
                printf("%d task write deferred\n",j);
                wjob_deferred[j] = 1;
            }
            if(cur_cp == next_w_release[j] && wjob_finished[j] == 1 && wjob_deferred[j] == 0){
                if(IO_resetflag[j] == 1){
                    IO_resetpoint[j] = cur_cp;
                    IO_resetflag[j] = 0;
                    reset_IO_update(newmeta,tasks[j].addr_lb,tasks[j].addr_ub,cur_cp);
                }
                //printf("next w : %ld, cur cp : %ld, wjob : %d\n",next_w_release[j],cur_cp,wjob_finished[j]);
                gettimeofday(&(algo_start_time),NULL);
                cur_wb[j] = write_job_start_q(tasks, j, tasknum, newmeta, 
                                              fblist_head, full_head, write_head,
                                              w_workloads[j], wq[j], cur_wb[j], wflag, cur_cp,IO_resetpoint[j]);
                write_release_num++;
                gettimeofday(&(algo_end_time),NULL);
                //printf("wovhd:%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
                write_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
                wjob_finished[j] = 0;
            } else if (cur_cp == next_w_release[j] && wjob_finished[j] == 0){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            } else if (cur_cp == next_w_release[j] && wjob_deferred[j] == 1){
                next_w_release[j] = cur_cp + (long)tasks[j].wp;
            }
            if(cur_cp == next_r_release[j] && rjob_finished[j] == 1){
                //printf("next r : %ld, cur cp : %ld, rjob : %d\n",next_r_release[j],cur_cp,rjob_finished[j]);
                read_job_start_q(tasks,j,newmeta,
                                 r_workloads[j],rq[j], cur_cp);
                next_r_release[j] = cur_cp + (long)tasks[j].rp;
                rjob_finished[j] = 0;
            } else if (cur_cp == next_r_release[j] && rjob_finished[j] == 0){
                next_r_release[j] = cur_cp + (long)tasks[j].rp;
            }
            if(cur_cp == next_gc_release[j] && gcjob_finished[j] == 1){
                //printf("next gc : %ld, cur cp : %ld, gcjob : %d\n",next_gc_release[j],cur_cp,gcjob_finished[j]);
                if(newmeta->total_invalid >= expected_invalid){
                    print_fullblock_profile(fps[tasknum+tasknum+1],cur_cp,newmeta,full_head);
                    gettimeofday(&(algo_start_time),NULL);
                    gc_job_start_q(tasks, j, tasknum, newmeta,
                               fblist_head, full_head, rsvlist_head, write_head, 0,
                               gcq[j], &(cur_GC[j]), gcflag, cur_cp);
                    gc_release_num++;
                    gettimeofday(&(algo_end_time),NULL);
                    gc_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
                    gcjob_finished[j] = 0;
                    next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                } else {
                    next_gc_release[j] = cur_cp + (long)tasks[j].gcp;
                    gcjob_finished[j] = 1;
                }
            } else if (cur_cp == next_gc_release[j] && gcjob_finished[j] == 0){
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
            //printf("[WL]rrcheck : %ld, cur_cp : %ld, util allowed : %lf\n",cur_rr.rrcheck, cur_cp, (double)rrutil);
            gettimeofday(&(algo_start_time),NULL);
            RR_job_start_q(tasks, tasknum, newmeta, fblist_head, full_head, hotlist, coldlist,
                            rr,&(cur_rr),(double)rrutil,cur_cp);
            rr_release_num++;
            gettimeofday(&(algo_end_time),NULL);
            rr_ovhd_sum += algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec;
            //printf("[RR]%ld\n",algo_end_time.tv_sec * 1000000 + algo_end_time.tv_usec - algo_start_time.tv_sec * 1000000 - algo_start_time.tv_usec);
            if(rr->reqnum != 0){
                rr_finished = 0;
            } else {
                rr_finished = 1;
            }
            do_rr = 0;
        }

        //req pick logic
        if(cur_IO == NULL){
            //init params
            long cur_dl = __LONG_MAX__;
            int target_task = -1;
            int target_type = -1;

            //iterate through per-task queues and pick the I/O with earliest deadline.
            //operation priority is RR > GC > W > R.
            //note that deadline is updated when dl is "less than " cur_dl
            if(rr->head != NULL){
                if(rr->head->deadline <= cur_dl){
                    target_type = RR;
                    cur_dl = rr->head->deadline;
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

            //if something's popped out, update cur_IO_end
            if(cur_IO != NULL){
                cur_IO_end = cur_cp + cur_IO->exec;
            } else {
                cur_IO_end = __LONG_MAX__;
            }
        }
        
        //go to the next checkpoint
        cur_cp = find_next_time(tasks,tasknum,cur_IO_end,rr_check,cur_cp,
                                next_w_release,next_r_release,next_gc_release);
        
        //printf("cur_cp : %ld\n",cur_cp);
        //sleep(1);
        
        //check_block(total_u,newmeta,tasks,tasknum,cur_cp,fp,fplife);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    sleep(1);
    return 0;
}
