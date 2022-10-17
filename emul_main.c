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

    //int vic[tasknum],rsv[tasknum];
    int g_cur = 0;                   //pointer for current writing page
    int tasknum = 4;                 //number of task
    int last_task = tasknum-1;       //index of last I/O task
    int skewness;                    //skew of utilization(-1 = noskew, 0 = read-skewed, 1 = write-skewed)
    int skewnum;                     //number of skewed task
    float totutil;                   //a total utilization of current system
    float rrutil;                    //a utilization allowed to data relocation job
    long cur_cp = 0;                 //current checkpoint time
    long cur_IO_end = __LONG_MAX__;             //absolute time when current req finishes
    int wl_init = 0;                 //flag for wear-leveling initiation
    float WU;                        //worst-case utilization tracker.

    FILE* w_workload;                             //init profile result file
    FILE* r_workload;
    FILE* rr_profile;
    FILE* w_workloads[tasknum];
    FILE* r_workloads[tasknum];
    FILE *fp, *fplife, *fpwrite, *fpread, *fprr;
    FILE **fps;
    block* cur_wb[tasknum];
    GCblock cur_GC[tasknum];
    RRblock cur_rr;

    rttask* rand_tasks = NULL;
    rttask* tasks = NULL;

    //simulate I/O(init related params)
    float total_u;
    int oldest;
    int yngest;
    int over_avg = 0;
    long rr_check = (long)100000;
    long runtime = 8400000000;
    //long runtime = 2100000000L;
    IO* cur_IO = NULL;
    IOhead* wq[tasknum];
    IOhead* rq[tasknum];
    IOhead* gcq[tasknum];
    IOhead* rr;
    FILE* lat_log_w[tasknum];
    FILE* lat_log_r[tasknum];
    FILE* finish_log;
    long* timings_w[tasknum];
    long* timings_r[tasknum];

    for(int i=0;i<tasknum;i++){
        wq[i] = ll_init_IO();
        rq[i] = ll_init_IO();
        gcq[i]= ll_init_IO();
    }
    rr = ll_init_IO();

    for(int i=0;i<tasknum;i++){
        timings_w[i] = (long*)malloc(sizeof(long)*2);
        timings_r[i] = (long*)malloc(sizeof(long)*2);
    }

    //flag variables
    int gcflag = 0, wflag = 0, rrcond = 0, genflag = 0, taskflag = 0;
    float sploc, tploc;
    int OPflag;
    
    //only enable following two lines in a case to check util per cycle.
    //randtask_statechecker(tasknum,8000);
    //return;

    //get flags
    set_scheme_flags(argv,
                     &gcflag, &wflag, &rrflag, &rrcond);
    set_exec_flags(argv, &tasknum, &totutil,
                   &genflag, &taskflag,
                   &skewness, &sploc, &tploc, &skewnum,
                   &OPflag, &OP, &MINRC);
    
    printf("[ SCHEMES ] %d, %d, %d, %d\n",wflag,gcflag,rrflag,rrcond);
    printf("[EXEC-main] %d, %d\n",genflag,taskflag);
    printf("[EXEC-task] %d, %f\n",tasknum,totutil);
    printf("[EXEC-skew] %d, %f, %f, %d\n",skewness,sploc,tploc,skewnum);
    printf("[EXEC-OP  ] %d, %f, %d\n",OPflag, OP, MINRC);
    sleep(1);
   
    //MINRC is now a configurable value, which can be adjusted like OP
    //reference :: RTGC mechanism (2004, li pin chang et al.) 
    int max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
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
                rand_tasks = generate_taskset_skew(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
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
        IOgen(tasknum,rand_tasks,8400000000,0,sploc,tploc);
        printf("workload generated!\n");
        fclose(file_taskparam);
        return 0;
    }

    //init task
    //!!!rest of the main code uses "tasks", so remember to assign correct pointer. 
    FILE* main_taskparam = fopen("taskparam.csv","r");
    rand_tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    get_task_from_file(rand_tasks,tasknum,main_taskparam);
    fclose(main_taskparam);
    tasks = rand_tasks;
    sleep(1);
    
    //init csv files
    //fp = open_file_bycase(gcflag,wflag,rrflag);
    fps = open_file_pertask(gcflag,wflag,rrflag,tasknum);
    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    rr_profile = fopen("rr_prof.csv","w");
    fplife = fopen("lifetime.csv","a");
    //fpwrite = fopen("writeselection.csv","w");
    //fpread = fopen("readworst.csv","w");
    //fprr = fopen("relocperiod.csv","w"); 
    finish_log = fopen("log.csv","w");
    IO_open(tasknum, w_workloads,r_workloads);
    lat_open(tasknum, lat_log_w,lat_log_r);
    
    //fprintf(fp,"%s\n","timestamp,taskidx,WU,new_WU,noblock,w_util,r_util,g_util,old,yng,bidx,state,vp");
    for(int i=0;i<tasknum;i++){
        fprintf(fps[i],"%s\n","timestamp,taskidx,WU,new_WU,noblock,w_util,r_util,g_util,old,yng,bidx,state,vp,w_idx,w_state,fb,w");
    }
    fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    //fprintf(fpread,"%s\n","timestamp");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum);
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);

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
    sleep(1);
    //!!finish initial writing
    
    while(cur_cp <= runtime){
        
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);
       
        //execution order must be (req completion --> job release --> req pick)

        //req completion logic
        if(cur_IO_end == cur_cp){
            if(cur_IO != NULL){

                //print cur qsize
                //printf("cur_cp : %ld, qsize :",cur_cp);
                //for(int i=0;i<tasknum;i++){
                //    printf("%d ", wq[i]->reqnum);
                //    printf("%d ", rq[i]->reqnum);
                //}
                //printf(" tot_u : %f\n",get_totutil(tasks,tasknum,cur_IO->taskidx,newmeta,oldest));
                //update util
                if(cur_IO->type == GCER){
                    total_u = print_profile(tasks,tasknum,cur_IO->taskidx,newmeta,fps[cur_IO->taskidx],yngest,oldest,cur_cp,
                                            cur_IO->vic_idx,newmeta->state[cur_IO->vic_idx],
                                            cur_wb[cur_IO->taskidx],fblist_head,write_head,
                                            newmeta->total_fp);
                    if(total_u > 1.0){
                        printf("[%ld]utilization overflow, util : %f\n",cur_cp, total_u);
                        fprintf(fplife,"%ld,",cur_cp);
                        sleep(3);
                        return 1;
                    }
                }
                //save latency
                if(cur_IO->last == 1){
                    check_latency(lat_log_w,lat_log_r,cur_IO,cur_cp);
                    if(check_dl_violation(tasks,cur_IO,cur_cp)==1){
                        lat_close(tasknum,lat_log_w,lat_log_r);
                        fprintf(fp,"last cp, %ld\n",cur_cp);
                        fflush(fp);
                        sleep(3);
                        return 1;
                    }
                }

                //finish request q
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
            if(cur_cp % (long)tasks[j].wp == 0){
                cur_wb[j] = write_job_start_q(tasks, j, tasknum, newmeta, 
                                              fblist_head, full_head, write_head,
                                              w_workloads[j], wq[j], cur_wb[j], wflag, cur_cp);  
            }
            if(cur_cp % (long)tasks[j].rp == 0){
                read_job_start_q(tasks,j,newmeta,
                                 r_workloads[j],rq[j], cur_cp);
            }
            if((cur_cp % (long)tasks[j].gcp == 0) && (newmeta->total_invalid >= expected_invalid)){
                gc_job_start_q(tasks, j, tasknum, newmeta,
                               fblist_head, full_head, rsvlist_head, 0,
                               gcq[j], &(cur_GC[j]), gcflag, cur_cp);
            } else if ((cur_cp % (long)tasks[j].gcp == 0) && (newmeta->total_invalid < expected_invalid)){
                //(util check)update runtime utilization of gc
                //newmeta->runutils[2][j] = 0.0;
            }
        }
        //release WL jobs
        if((cur_cp % rr_check == 0) && (rr->head == NULL) && (cur_cp >= cur_rr.rrcheck) && (rrflag != -1)){
            rrutil = 1.0 - find_cur_util(tasks,tasknum,newmeta,oldest);
            printf("[WL]util allowed : %lf\n",(double)rrutil);
            if(rrutil >= 0.05){
                RR_job_start_q(tasks, tasknum, newmeta,
                              fblist_head, full_head,
                              rr,&(cur_rr),0.05,cur_cp);
            } else {
                RR_job_start_q(tasks, tasknum, newmeta,
                              fblist_head, full_head,
                              rr,&(cur_rr),(double)rrutil,cur_cp);
            }
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
                if(rr->head->deadline < cur_dl){
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
                //printf("popping %d, map: %d to %d, exec time : %d\n",cur_IO->type, cur_IO->lpa, cur_IO->ppa,cur_IO->exec);
            }
        }
        
        //go to the next checkpoint
        cur_cp = find_next_time(tasks,tasknum,cur_IO_end,rr_check,cur_cp+1);
        
        //printf("cur_cp : %ld\n",cur_cp);
        //sleep(1);
        
        //check_block(total_u,newmeta,tasks,tasknum,cur_cp,fp,fplife);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    //sleep(1);
    return 0;
}
