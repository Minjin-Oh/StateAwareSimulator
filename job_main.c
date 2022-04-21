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
    int tasknum = 1;                //number of task
    int last_task = tasknum-1;
    int cur_cp = 0;                 //current checkpoint time
    int wl_mult = 10;               //period of wl. computed as wl_mult*gcp
    int total_fp = NOP-PPB*tasknum; //tracks number of free page = (physical space - reserved area)
    int cps_size = 0;               //number of checkpoints
    int wl_init = 0;                //flag for wear-leveling initiation
    int wl_count = 0;               //a count for how many wl occured
    int gc_count = 0;               //a profiles for how many gc occured
    int gc_ctrl = 0;
    int gc_nonworst = 0;
    int fstamp = 0;                 //a period for profile recording.
    float w_util[tasknum];          //runtime utilization tracker
    float r_util[tasknum];
    float g_util[tasknum];
    float w_wcutil[tasknum];        //per-task utilization tracker
    float r_wcutil[tasknum];
    float g_wcutil[tasknum];
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

    //get flags & open file
    int gcflag = 0, wflag = 0, genflag = 0;
    if(strcmp(argv[1],"DOGC")==0){
        gcflag = 1;
    }
    if(strcmp(argv[2],"DOW")==0){
        wflag = 1;
    }
    if(strcmp(argv[3],"DORR")==0){
        rrflag = 1;
    }
    if(strcmp(argv[4],"WORKGEN")==0){
        genflag = 1;
    }
    printf("flags:%d,%d,%d\n",gcflag,wflag,rrflag);
    FILE* fp = open_file_bycase(gcflag,wflag,rrflag);
    FILE* fplife = fopen("lifetime.csv","a");
    FILE* fpwrite = fopen("writeselection.csv","w");
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fprintf(fplife,"\n"); 
    }
    sleep(1);

    //MINRC is now a configurable value, which can be adjusted like OP
    //reference :: RTGC mechanism (2004, li pin chang et al.) 
    int max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int expected_invalid = MINRC*(NOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d\n",expected_invalid,expected_fp);
    //init tasks(expand this to function for multi-task declaration)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(75000,19,MINRC));
    int gcp_temp2 = (int)(__calc_gcmult(150000,6,MINRC));
    int gcp_temp3 = (int)(__calc_gcmult(75000,12,MINRC)); 
    init_task(&(tasks[0]),0,75000,19,75000,1,gcp_temp,0,max_valid_pg);
    //example task
    //init_task(&(tasks[0]),0,1200000,1,30000,50,gcp_temp,0,128);
    //init_task(&(tasks[1]),1,150000,6,40000,12,gcp_temp2,128,max_valid_pg);
    //init_task(&(tasks[2]),2,75000,8,75000,20,gcp_temp3,128,max_valid_pg);

    if(genflag == 1){
        IOgen(tasknum,tasks,2100000000,0);
        printf("workload generated!\n");
        return 0;
    }

    //open csv files for profiling and workload
    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    rr_profile = fopen("rr_prof.csv","w");
    IO_open(tasknum, w_workloads,r_workloads);
    fprintf(fp,"%s\n","timestamp,taskidx,WU,new_WU,w_util,r_util,g_util,old,yng");
    fprintf(rr_profile,"%s\n","timestamp,vic1,state,window,vic2,state,window");
    
    //initialize blocklist for blockmanage.
    init_metadata(newmeta,tasknum);
    int* cps = add_checkpoints(tasknum,tasks,2100000000,&(cps_size));
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);
    
    wl_head = init_blocklist(0,-1);
    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);

    //init data access & distribution tracker
    for(int i=0;i<tasknum;i++){
        w_util[i] = 0.0;
        r_util[i] = 0.0;
        g_util[i] = 0.0;
        w_wcutil[i] = 0.0;
        r_wcutil[i] = 0.0;
        g_wcutil[i] = 0.0;
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
    int rr_check = tasks[last_task].gcp;
    for(int i=0;i<cps_size;i++){
        yngest = get_blockstate_meta(newmeta,YOUNG);
        oldest = get_blockstate_meta(newmeta,OLD);
        cur_cp = cps[i];
        //skip already checked timestamp
        if((i != 0) && (cur_cp == cps[i-1])){continue;}
        
        //iterate through tasks.
        for(int j=0;j<tasknum;j++){
            if(cur_cp % tasks[j].wp == 0){
                int cur_y=0, cur_o=0, targ=0;
                if(wjob[j] == 1){
                    write_job_end(tasks[j],newmeta,IO_wqueue[j],&(total_fp));
                }
                cur_wb[j] = write_job_start(tasks,j,tasknum,newmeta,
                                            fblist_head,full_head,write_head,
                                            w_workloads[j],IO_wqueue[j],cur_wb[j],wflag);
                wjob[j] = 1;
                block* check_yng = ll_find(newmeta, fblist_head, YOUNG);
                block* check_old = ll_find(newmeta, fblist_head, OLD);
                if(check_yng == NULL || check_old == NULL){
                    check_yng = ll_find(newmeta, write_head, YOUNG);
                    check_old = ll_find(newmeta, write_head, OLD);
                }
                cur_y = newmeta->state[check_yng->idx];
                cur_o = newmeta->state[check_old->idx];
                targ = newmeta->state[cur_wb[j]->idx];
                fprintf(fpwrite,"%d,%d,%d,%d\n",cur_cp,targ,cur_y,cur_o);
            }//!write end
            
            if(cur_cp % tasks[j].rp == 0){
                if(rjob[j]==1){
                    read_job_end(tasks[j],newmeta,IO_rqueue[j]);
                }
                read_job_start(tasks[j],newmeta,r_workloads[j],IO_rqueue[j]);
                rjob[j] = 1;
            }//!read end
    
            if((cur_cp % tasks[j].gcp == 0) && (newmeta->total_invalid >= expected_invalid)){
                if(gcjob[j] == 1){
                    
                    //save cur_GC idx for profiling.
                    int cur_gc_idx = cur_GC[j].cur_vic->idx;
                    int cur_gc_state = newmeta->state[cur_gc_idx];
                    if(cur_gc_state >= oldest-1){
                        over_avg += 1;
                    }
                    gc_job_end(tasks,j,tasknum,newmeta,IO_gcqueue[j],
                               fblist_head,rsvlist_head,
                               &(cur_GC[j]),&(total_fp));

                    //PROFILES
                    total_u = print_profile(tasks,tasknum,j,newmeta,fp,yngest,oldest,cur_cp,cur_gc_idx,cur_gc_state);   
                }
                gc_job_start(tasks,j,tasknum,newmeta,
                             fblist_head,full_head,rsvlist_head,-1,
                             IO_gcqueue[j],&(cur_GC[j]),gcflag);
                gcjob[j] = 1;
            }//!gc end
#ifdef DORELOCATE
            if((cur_cp == rr_check) && (j==last_task)){
                int vic1 = -1;
                int vic2 = -1;
                int RRsched = 0;
                float slack_util;
                find_RR_target(tasks,tasknum,newmeta,fblist_head,full_head,&vic1,&vic2);
                if(rrjob == 1){
                    RR_job_end(newmeta,fblist_head,full_head,IO_rrqueue,&cur_rr,&total_fp);
                    rrjob = 0;
                }
                if(vic1 != -1 && vic2 != -1){
                    if(newmeta->state[vic1] - newmeta->state[vic2] >= THRESHOLD){
                        //calculate slack
                        WU = find_worst_util(tasks,tasknum,newmeta);
                        slack_util = 1.0 - WU;
                        //temporary skip condition
                        float worst_exec = ((r_exec(oldest)+w_exec(yngest))*PPB + e_exec(oldest))*2;
                        if(slack_util <= 0.0){
                            printf("skip RR. no util.\n");
                            RRsched = 0;
                        } else {
                            //enqueue RR opeartion
                            RR_job_start(tasks,tasknum,newmeta,fblist_head,full_head,IO_rrqueue,&cur_rr);
                            printf("[RR]fp:%d vs inv:%d, %d\n",total_fp,newmeta->total_invalid,total_fp+newmeta->total_invalid);
                            rrjob = 1;
                            for(int i=0;i<NOB;i++){
                                newmeta->access_window[i]=0;
                            }
                            //calculate least period
                            float rr_period = cur_rr.execution_time / slack_util;
                            float mult = rr_period/(float)tasks[last_task].gcp;
                            int next_rr = (int)ceil((double)mult) * tasks[last_task].gcp + cur_cp;
                            //show result
                            printf("period : %d, mult : %f\n",next_rr,mult);
                            //sleep(1);
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
                    rr_check = cur_cp + tasks[last_task].gcp;
                }
                printf("next rr_check : %d, gap : %d, slack : %f\n",rr_check, rr_check-cur_cp,slack_util);
                
            }//!RR end
#endif
        }//!task iteration end
        //printf("tot_u : %f\n",total_u);
        check_profile(total_u,newmeta,tasks,tasknum,cur_cp,fp,fplife);
    }
}