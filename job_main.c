#include "stateaware.h"

//globals
block* cur_fb = NULL;

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
    int tasknum = 3;                //number of task
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
    char wjob[tasknum], rjob[tasknum],gcjob[tasknum];
    int vic[tasknum],rsv[tasknum];
    block* cur_wb[tasknum];
    GCblock cur_GC[tasknum];
#if defined(DOGCNOTHRES) && !defined(DORELOCATE)
    FILE* fp = fopen("SA_prof_GCC.csv","w");
#endif
#if defined(FORCEDCONTROL) && !defined(DORELOCATE)
    FILE* fp = fopen("SA_prof_forced.csv","w");
#endif
#if !defined(DOGCNOTHRES) && !defined(DORELOCATE) && !defined(FORCEDCONTROL)
    FILE* fp = fopen("SA_prof_noRR.csv","w"); // make sure you turn off all optimizing scheme.
#endif
#if defined(DORELOCATE) && !defined(DOGCCONTROL) && !defined(FORCEDCONTROL)
    FILE* fp = fopen("SA_prof_RR.csv","w");
#endif
#if defined(DORELOCATE) && defined(DOGCCONTROL)
    FILE* fp = fopen("SA_prof_all.csv","w");
#endif
#if defined(DORELOCATE) && defined(FORCEDCONTROL)
    FILE* fp = fopen("SA_prof_heur.csv","w");
#endif

    //MINRC is now a configurable value, which can be adjusted like OP
    //reference :: RTGC mechanism (2004, li pin chang et al.) 
    int max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int expected_invalid = MINRC*(NOB-tasknum);
    int expected_fp = PPB*(NOB-tasknum) - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d\n",expected_invalid,expected_fp);
    
    //init tasks(expand this to function for multi-task declaration)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(600000,1,MINRC));
    int gcp_temp2 = (int)(__calc_gcmult(150000,6,MINRC));
    int gcp_temp3 = (int)(__calc_gcmult(75000,11,MINRC)); 
    init_task(&(tasks[0]),0,1200000,1,30000,70,gcp_temp,0,128);
    init_task(&(tasks[1]),1,150000,6,40000,12,gcp_temp2,128,max_valid_pg);
    init_task(&(tasks[2]),2,75000,11,75000,20,gcp_temp3,128,max_valid_pg);

#ifdef WORKGEN 
    IOgen(tasknum,tasks,2100000000,0);
    printf("workload generated!\n");
    return 0;
#endif

    //open csv files for profiling and workload
    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    IO_open(tasknum, w_workloads,r_workloads);
    rr_profile = fopen("rr_prof.csv","w");
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
    for(int i=0;i<cps_size;i++){
        oldest = get_blockstate_meta(newmeta,OLD);
        cur_cp = cps[i];
        //skip already checked timestamp
        if((i != 0) && (cur_cp == cps[i-1])){continue;}
        
        //iterate through tasks.
        for(int j=0;j<tasknum;j++){
            if(cur_cp % tasks[j].wp == 0){
                if(wjob[j] == 1){
                    write_job_end(tasks[j],newmeta,IO_wqueue[j],&(total_fp));
                }
                cur_wb[j] = write_job_start(tasks,j,tasknum,newmeta,
                                            fblist_head,full_head,write_head,
                                            w_workloads[j],-1,IO_wqueue[j],cur_wb[j]);
                wjob[j] = 1;
                //printf("[T%d]WB:%d,cur_fp:%d,FBs: %d,tot_fp:%d\n",
                //       j,cur_wb[j]->idx,cur_wb[j]->fpnum,fblist_head->blocknum,total_fp);
            }//!write end
            
            if(cur_cp % tasks[j].rp == 0){
                if(rjob[j]==1){
                    read_job_end(tasks[j],newmeta,IO_rqueue[j]);
                }
                read_job_start(tasks[j],newmeta,r_workloads[j],IO_rqueue[j]);
                rjob[j] = 1;
            }

            if((cur_cp % tasks[j].gcp == 0) && (newmeta->total_invalid >= expected_invalid)){
                if(gcjob[j] == 1){
                    printf("[GC][BEF]current FP&INV:%dvs%d,sum=%d\n",total_fp,newmeta->total_invalid,total_fp+newmeta->total_invalid);
                    gc_job_end(tasks,j,tasknum,newmeta,IO_gcqueue[j],
                               fblist_head,rsvlist_head,
                               &(cur_GC[j]),&(total_fp));
                    printf("[GC][AFT]current FP&INV:%dvs%d,sum=%d\n",total_fp,newmeta->total_invalid,total_fp+newmeta->total_invalid);
                    //PROFILES
                    WU = find_worst_util(tasks,tasknum,newmeta);
                    total_u=0.0;
                    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
                        total_u += newmeta->runutils[0][j];
                        total_u += __calc_ru(&(tasks[j]),oldest);
                        total_u += newmeta->runutils[2][j];
                    }
                    total_u += (float)e_exec(oldest) / (float)_find_min_period(tasks,tasknum,OP);
                    fprintf(fp,"%d,%d,%f,%f,%f,%f,%f,%d,%d\n",cur_cp,j,
                    WU,total_u,
                    newmeta->runutils[0][j],__calc_ru(&(tasks[j]),oldest),newmeta->runutils[2][j]);         
                }
                gc_job_start(tasks,j,tasknum,newmeta,
                             fblist_head,full_head,rsvlist_head,-1,
                             IO_gcqueue[j],&(cur_GC[j]));
                gcjob[j] = 1;
                
            }//!gc end
            
            
        }//!task iteration end
        if(total_u >= 1.0){    
            printf("[%d]utilization overflowed(%f) exit in 5 seconds...\n",cur_cp,total_u);
            for(int i=0;i<NOB;i++){
                printf("%d(%d) ",i,newmeta->state[i]);
            }
            printf("\nread-hot(0~128)\n");
            int readhotworst = 0;
            for(int i=0;i<128;i++){
                printf("%d(%d){%d} ",i,newmeta->pagemap[i]/PPB,newmeta->state[newmeta->pagemap[i]/PPB]);
                if(newmeta->state[newmeta->pagemap[i]/PPB] > readhotworst){
                    readhotworst = newmeta->state[newmeta->pagemap[i]/PPB];
                }
            }
            printf("\nreadhotworst : %d\n",readhotworst);
            for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
                printf("utils:%f, %f, %f\n",
                        newmeta->runutils[0][j],
                        __calc_ru(&(tasks[j]),oldest),
                        newmeta->runutils[2][j]);
            }
            fflush(fp);
            sleep(5);
            abort();
        }
    }
}