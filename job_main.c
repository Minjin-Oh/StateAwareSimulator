#include "stateaware.h"

//globals
block* cur_fb = NULL;
int rrflag = 0;
int main(int argc, char* argv[]){
    //init params
    srand(time(NULL)); 
    bhead* fblist_head;                             //heads for block list
    bhead* rsvlist_head;
    bhead* full_head;
    bhead* write_head;
    bhead* wl_head;
    bhead* hotlist;                                 //(for WL) overlaps previous blocklist
    bhead* coldlist;
    meta* newmeta = (meta*)malloc(sizeof(meta));    //metadata structure

    int g_cur = 0;                  //pointer for current writing page
    int tasknum = 4;                //number of task
    int skewness;                   //skew of utilization(-1 = noskew, 0 = read, 1 = write)
    int skewnum;                    //number of skewed task
    float totutil;
    int last_task = tasknum-1;
    long cur_cp = 0;                 //current checkpoint time
    int cps_size = 0;                //number of checkpoints
    int wl_mult = 8;               //period of wl. computed as wl_mult*gcp
    int total_fp = NOP-PPB*tasknum; //tracks number of free page = (physical space - reserved area)
                 
    int wl_init = 0;                //flag for wear-leveling initiation
    int wl_count = 0;               //a count for how many wl occured
    int gc_count = 0;               //a profiles for how many gc occured
    int gc_ctrl = 0;
    int gc_nonworst = 0;
    int fstamp = 0;                 //a period for profile recording.
    
    float WU;                       //worst-case utilization tracker.
    float SAWU;
    FILE* w_workload;               //init profile result file
    FILE* r_workload;
    FILE* rr_profile;
    FILE* w_workloads[tasknum];
    FILE* r_workloads[tasknum];
    IO* IO_wqueue[tasknum];
    IO* IO_rqueue[tasknum];
    IO* IO_gcqueue[tasknum];
    IO IO_rrqueue[PPB*2];
    char wjob[tasknum], rjob[tasknum],gcjob[tasknum], rrjob;
    int vic[tasknum],rsv[tasknum];
    block* cur_wb[tasknum];
    GCblock cur_GC[tasknum];
    RRblock cur_rr;
    
    //only enable following two lines in a case to check util per cycle.
    //randtask_statechecker(tasknum,8000);
    //return;

    //get flags & open file
    int gcflag = 0, wflag = 0, genflag = 0, taskflag = 0, rrcond = 0;
    if(strcmp(argv[1],"DOGC")==0){
        gcflag = 1;
    }
    if(strcmp(argv[2],"DOW")==0){
        wflag = 1;
    } else if (strcmp(argv[2],"GREEDYW")==0){
        wflag = 2;
    }
    if(strcmp(argv[3],"SKIPRR")==0){// do util-based relocation
        rrflag = -1;
    } else if (strcmp(argv[3],"DORR")==0){//a case when read occurs in best
        rrflag = 1;
        rrcond = 0;
    } else if (strcmp(argv[3],"RR005")==0){
        rrflag = 1;
        rrcond = 1;
    } else if (strcmp(argv[3],"RR01")==0){
        rrflag = 1;
        rrcond = 2;
    } else if (strcmp(argv[3],"FRR")==0){
        rrflag = 1;
        rrcond = 3;
    } else if (strcmp(argv[3], "FRR01")==0){
        rrflag = 1;
        rrcond = 4;
    } else if (strcmp(argv[3], "FRR02")==0){
        rrflag = 1;
        rrcond = 5;
    } else if (strcmp(argv[3], "FRR03")==0){
        rrflag = 1;
        rrcond = 6;
    } else if (strcmp(argv[3],"BASE")==0){
        rrflag = 0;
        rrcond = 0;   
    } else if (strcmp(argv[3],"BASE005")==0){
        rrflag = 0;
        rrcond = 1;
    } else if (strcmp(argv[3],"BASE01")==0){
        rrflag = 0;
        rrcond = 2;
    } else if (strcmp(argv[3],"FBASE")==0){
        rrflag = 0;
        rrcond = 3;
    } else if (strcmp(argv[3], "FBASE01")==0){
        rrflag = 0;
        rrcond = 4;
    } else if (strcmp(argv[3], "FBASE02")==0){
        rrflag = 0;
        rrcond = 5;
    } else if (strcmp(argv[3], "FBASE03")==0){
        rrflag = 0;
        rrcond = 6;
    } 
    else if (strcmp(argv[3],"BESTR")==0){//always SKIP RR
        rrflag = 5;
    }
    if(strcmp(argv[4],"WORKGEN")==0){
        genflag = 1;
    }
    if(strcmp(argv[4],"TASKGEN")==0){
        taskflag = 1;
    }
    tasknum = atoi(argv[5]);
    totutil = atof(argv[6]);
    skewness = atof(argv[7]);
    float sploc = atof(argv[8]);
    float tploc = atof(argv[9]);
    if(skewness != -1){
        skewnum = atoi(argv[10]);
    }
    printf("we got %d, %d, %f\n",tasknum,skewness,totutil);

    printf("flags:%d,%d,%d\n",gcflag,wflag,rrflag);
    FILE* fp = open_file_bycase(gcflag,wflag,rrflag);
    FILE* fplife = fopen("lifetime.csv","a");
    FILE* fpwrite = fopen("writeselection.csv","w");
    FILE* fpread = fopen("readworst.csv","w");
    FILE* fprr = fopen("relocperiod.csv","w");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
   

    //MINRC is now a configurable value, which can be adjusted like OP
    //reference :: RTGC mechanism (2004, li pin chang et al.) 
    int max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int expected_invalid = MINRC*(NOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d, maxvalid : %d\n",expected_invalid,expected_fp,max_valid_pg);
    sleep(1);
    float res = 1.0;
    rttask* rand_tasks = NULL;
    if(taskflag==1){
        while(res >= 1.0){
            if(skewness == -1){
                rand_tasks = generate_taskset(tasknum,totutil,max_valid_pg,&res,0);
            }
            if(skewness != -1){
                rand_tasks = generate_taskset_skew(tasknum,totutil,max_valid_pg,&res,skewnum,skewness,0);
            }
            if(res >= 1.0){
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
    /*//init tasks(old logic)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(75000,19,MINRC));
    int gcp_temp2 = (int)(__calc_gcmult(150000,3,MINRC));
    int gcp_temp3 = (int)(__calc_gcmult(75000,6,MINRC)); 
    init_task(&(tasks[0]),0,75000,19,75000,1,gcp_temp,0,max_valid_pg);
    //example task
    //init_task(&(tasks[0]),0,1200000,1,30000,200,gcp_temp,0,128);
    //init_task(&(tasks[1]),1,150000,3,40000,12,gcp_temp2,128,max_valid_pg);
    //init_task(&(tasks[2]),2,75000,6,75000,20,gcp_temp3,128,max_valid_pg);
    */
    if(genflag == 1){
        FILE* file_taskparam = fopen("taskparam.csv","r");
        rand_tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
        get_task_from_file(rand_tasks,tasknum,file_taskparam);
        IOgen(tasknum,rand_tasks,8400000000,0,sploc,tploc);
        printf("workload generated!\n");
        fclose(file_taskparam);
        return 0;
    }
    //open csv files for profiling and workload
    FILE* main_taskparam = fopen("taskparam.csv","r");
    rand_tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    get_task_from_file(rand_tasks,tasknum,main_taskparam);
    float bfac_min = e_exec(0)/(float)_find_min_period(rand_tasks,tasknum);
    float bfac_max = e_exec(MAXPE)/(float)_find_min_period(rand_tasks,tasknum);
    printf("blocking : %f to %f\n",bfac_min,bfac_max);
    sleep(1);
    fclose(main_taskparam);
    
    //!!!rest of the main function uses "tasks", so remember to assign correct pointer. 
    rttask* tasks = rand_tasks;
    
    //init csv files
    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    rr_profile = fopen("rr_prof.csv","w");
    IO_open(tasknum, w_workloads,r_workloads);
    fprintf(fp,"%s\n","timestamp,taskidx,WU,new_WU,w_util,r_util,g_util,old,yng,bidx,state,vp");
    fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    fprintf(fpread,"%s\n","timestamp");
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum);
    long* cps = add_checkpoints(tasknum,tasks,8400000000,&(cps_size));
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);
    
    wl_head = init_blocklist(0,-1);
    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);

    //init data access & distribution tracker
    for(int i=0;i<tasknum;i++){
        IO_wqueue[i] = (IO*)malloc(sizeof(IO)*tasks[i].wn);
        IO_rqueue[i] = (IO*)malloc(sizeof(IO)*tasks[i].rn);
        IO_gcqueue[i] = (IO*)malloc(sizeof(IO)*PPB);
        wjob[i] = 0;
        rjob[i] = 0;
        gcjob[i] = 0;
        cur_wb[i] = NULL;
        cur_GC[i].cur_vic = NULL;
        cur_GC[i].cur_rsv = NULL;
    }
    cur_rr.cur_vic1 = NULL;
    cur_rr.cur_vic2 = NULL;
    rrjob = 0;
    //do initial writing (validate all logical address)
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
        cur_fb->fpnum--;
        g_cur++;
        total_fp--;
    }
    ll_append(fblist_head,cur_fb);
    //!!finish initial writing
    
    //simulate I/O
    float total_u;
    int oldest;
    int yngest;
    int over_avg = 0;
    long rr_check = (long)100000;
    for(int i=0;i<cps_size;i++){
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);
        cur_cp = cps[i];
        //printf("cur cp : %ld\n",cur_cp);
        //skip already checked timestamp
        if((i != 0) && (cur_cp == cps[i-1])){continue;}
        
        //iterate through tasks.
        for(int j=0;j<tasknum;j++){

            if(cur_cp % (long)tasks[j].wp == 0){
                //write body
                if(wjob[j] == 1){
                    write_job_end(tasks[j],newmeta,IO_wqueue[j],&(total_fp));
                }
                cur_wb[j] = write_job_start(tasks,j,tasknum,newmeta,
                                            fblist_head,full_head,write_head,
                                            w_workloads[j],IO_wqueue[j],cur_wb[j],wflag);
                wjob[j] = 1;
                //write profile
                int cur_y=0, cur_o=0, targ=0;
                block* check_yng = ll_find(newmeta, fblist_head, YOUNG);
                block* check_old = ll_find(newmeta, fblist_head, OLD);
                if(check_yng == NULL || check_old == NULL){
                    check_yng = ll_find(newmeta, write_head, YOUNG);
                    check_old = ll_find(newmeta, write_head, OLD);
                }
                cur_y = newmeta->state[check_yng->idx];
                cur_o = newmeta->state[check_old->idx];
                targ = newmeta->state[cur_wb[j]->idx];
                fprintf(fpwrite,"%ld,%d,%d,%d\n",cur_cp,targ,cur_y,cur_o);
            }//!write end
            
            if(cur_cp % (long)tasks[j].rp == 0){
                //read body
                if(rjob[j]==1){
                    read_job_end(tasks[j],newmeta,IO_rqueue[j]);
                    
                }
                read_job_start(tasks[j],newmeta,r_workloads[j],IO_rqueue[j]);
                rjob[j] = 1;
                //read profile
                update_read_worst(newmeta,tasknum);
                fprintf(fpread,"%ld,",cur_cp);
                for(int k=0;k<tasknum;k++){
                    fprintf(fpread,"%d,",newmeta->cur_read_worst[k]);
                }
                fprintf(fpread,"%d\n",oldest);
                fflush(fpread);
            }//!read end
    
            if((cur_cp % (long)tasks[j].gcp == 0) && (newmeta->total_invalid >= expected_invalid)){
                if(gcjob[j] == 1){
                    //save cur_GC idx for profiling.
                    int cur_gc_idx = cur_GC[j].cur_vic->idx;
                    int cur_gc_state = newmeta->state[cur_gc_idx];
                    int prev_fp = total_fp;
                    gc_job_end(tasks,j,tasknum,newmeta,IO_gcqueue[j],
                               fblist_head,rsvlist_head,
                               &(cur_GC[j]),&(total_fp));
                    //PROFILES
                    if(rrflag != 2){
                        total_u = print_profile(tasks,tasknum,j,newmeta,fp,yngest,oldest,cur_cp,cur_gc_idx,cur_gc_state,total_fp-prev_fp);   
                    } else if (rrflag == 2){
                        total_u = print_profile_best(tasks,tasknum,j,newmeta,fp,yngest,oldest,cur_cp,cur_gc_idx,cur_gc_state);   
                    }
                }
                gc_job_start(tasks,j,tasknum,newmeta,
                             fblist_head,full_head,rsvlist_head,-1,
                             IO_gcqueue[j],&(cur_GC[j]),gcflag);
                gcjob[j] = 1;
            }//!gc end
#ifdef DORELOCATE
            if((cur_cp == rr_check) && (j==last_task) && rrflag != -1){
                int vic1 = -1;
                int vic2 = -1;
                int RRsched = 0;
                float slack_util;
                if(rrflag == 1){
                    find_RR_target_util(tasks, tasknum, newmeta,fblist_head,full_head,&vic1,&vic2);
                } else if (rrflag == 0){
                    find_RR_target(tasks, tasknum, newmeta,fblist_head,full_head,&vic1,&vic2);
                }
                if(rrjob == 1){
                    RR_job_end(newmeta,fblist_head,full_head,IO_rrqueue,&cur_rr,&total_fp);
                    rrjob = 0;
                    update_read_worst(newmeta,tasknum);
                    fprintf(fprr,"%ld,%ld,",cur_cp,rr_check);
                    for(int k=0;k<tasknum;k++){
                        fprintf(fprr,"%d,",newmeta->cur_read_worst[k]);
                    }
                    fprintf(fprr,"%d,%d,%d,%d,%d,%d,%f,%ld,%ld\n",yngest, oldest,
                    newmeta->state[cur_rr.cur_vic1->idx],newmeta->state[cur_rr.cur_vic2->idx],
                    cur_rr.vic1_acc,cur_rr.vic2_acc,
                    cur_rr.execution_time,cur_rr.period,cur_rr.rrcheck);

                    fflush(fprr);
                }
                if(vic1 != -1 && vic2 != -1){
                    if((newmeta->state[vic1] - newmeta->state[vic2] >= THRESHOLD) && 
                       (newmeta->access_window[vic1] - newmeta->access_window[vic2] > 100)){
                        //calculate slack
                        //evaluate various cases
                        //case1. allocate slack <= 0.05
                        //case2. allocate slack <= 0.1
                        //case3. allocate every slack
                        //case4. allocate 0.05~0.3 statically
                        WU = find_worst_util(tasks,tasknum,newmeta);
                        if(rrcond==1){//case1
                            slack_util = 1.0 - WU;
                            if(slack_util >= 0.05)
                                slack_util = (float)0.05;
                        } else if (rrcond==2){//case2
                            slack_util = 1.0 - WU;
                            if(slack_util >= 0.1)
                                slack_util = (float)0.1;
                        } else if(rrcond==0){//case3
                            slack_util = 1.0 - WU;
                        } else if (rrcond==3){//case4
                            slack_util = 0.05;
                        } else if (rrcond==4){//case4
                            slack_util = 0.1;
                        } else if (rrcond==5){//case4
                            slack_util = 0.2;
                        } else if (rrcond==6){//case4
                            slack_util = 0.3;
                        }
                        //temporary skip condition
                        float worst_exec = ((r_exec(oldest)+w_exec(yngest))*PPB + e_exec(oldest))*2;
                        //if(slack_util <= -10.0){
                        if(slack_util <= 0.0){
                            printf("skip RR. no util.\n");
                            RRsched = 0;
                        } else {
                            //enqueue RR opeartion
                            RR_job_start(tasks,tasknum,newmeta,fblist_head,full_head,IO_rrqueue,&cur_rr);
                            printf("[RR]fp:%d vs inv:%d, %d\n",total_fp,newmeta->total_invalid,total_fp+newmeta->total_invalid);
                            cur_rr.vic1_acc = newmeta->access_window[cur_rr.cur_vic1->idx];
                            cur_rr.vic2_acc = newmeta->access_window[cur_rr.cur_vic2->idx];
                            rrjob = 1;
                            for(int i=0;i<NOB;i++){
                                newmeta->access_window[i]=0;
                            }
                            //calculate least period
                            float rr_period = cur_rr.execution_time / slack_util;
                            float mult = rr_period/(float)100000;
                            long next_rr = (long)ceil((double)mult) * (long)100000 + cur_cp;
                            cur_rr.cur = cur_cp;
                            cur_rr.period = next_rr - cur_cp;
                            cur_rr.rrcheck = next_rr;
                            //show result
                            printf("slack : %f, period : %ld, mult : %f\n",slack_util,next_rr,mult);
                            rr_check = next_rr;
                            RRsched = 1;
                        }
                    } else {
                        printf("skip RR, difference too small.\n");
                        RRsched = 0;
                    }
                } else {
                    printf("skip RR. no feasible target.\n");
                    RRsched = 0;
                }
                if(RRsched == 0){
                    rr_check = cur_cp + (long)100000;
                }
                printf("next rr_check : %ld, gap : %ld, slack : %f\n",rr_check, rr_check-cur_cp,slack_util);
            }//!RR end

            
#endif
        }//!task iteration end
        //printf("tot_u : %f\n",total_u);
        check_profile(total_u,newmeta,tasks,tasknum,cur_cp,fp,fplife);
        check_block(total_u,newmeta,tasks,tasknum,cur_cp,fp,fplife);
    }
    printf("run through all!!![cur_cp : %ld]\n",cur_cp);
    fprintf(fplife,"%ld,",cur_cp);
    fflush(fplife);
    sleep(1);
    return 0;
}