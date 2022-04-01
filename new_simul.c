#include "stateaware.h"

void release_write(rttask* task, int cur_time, int* next){
    int wp = task->wp;
    int ret = cur_time+wp;
    *next = ret;
}

void release_read(rttask* task, int cur_time, int* next){
    int wp = task->wp;
    int ret = cur_time+wp;
    *next = ret;
}

//gc takes a period as a variable, since period can be different from time to time.
void release_gc(rttask* task, int cur_time, int period, int* next){
    int ret = cur_time+period;
    *next = ret;
}

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
    int wl_mult = 5;                //period of wl. computed as wl_mult*gcp
    int total_fp = NOP-PPB*tasknum; //tracks number of free page = (physical space - reserved area)
    int cps_size = 0;               //number of checkpoints
    
    int wl_init = 0;                //flag for wear-leveling initiation
    int wl_count = 0;               //a count for how many wl occured
    
    int gc_count = 0;               //a profiles for how many gc occured
    int gc_ctrl = 0;
    int gc_nonworst = 0;
    int gc_type1 = 0;
    int gc_type2 = 0;
    int gc_type3 = 0;
    int gc_type4 = 0;
    int fstamp = 0;                 //a period for profile recording.
    
    float w_util[tasknum];          //runtime utilization tracker
    float r_util[tasknum];
    float g_util[tasknum];
    float w_wcutil[tasknum];        //per-task utilization tracker
    float r_wcutil[tasknum];
    float g_wcutil[tasknum];
    float WU;                       //worst-case utilization tracker.
    float SAWU;
    FILE* w_workload;
    FILE* r_workload;

    //init profile result file

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


    int runtime = 2100000000;       // a total runtime
    int gc_limit = -1;              // a value which stores limit of GC
    int gc_blockhistory = -1;       // a value which stores gc history
    int write_limit = 0;            // a value which stores limit of write block

    //add classifications for each file
    fprintf(fp,"%s\n","timestamp,taskidx,WU,actual_WU,w_util,w_wc,r_util,r_wc,g_util,g_wc,old,yng,std,gc_limit");
    //FIXME::min_rc is hardcoded now
    int actual_minrc = (int)(((NOB-tasknum)*PPB - NOB*PPB*(1-OP))/NOB) - 2;
    //init tasks(expand this to function for multi-task declaration)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(75000,11,(int)(actual_minrc)));
    int gcp_temp2 = (int)(__calc_gcmult(150000,6,(int)(actual_minrc)));
    int gcp_temp3 = (int)(__calc_gcmult(300000,3,(int)(actual_minrc)));
    init_task(&(tasks[0]),0,75000,11,75000,100,gcp_temp);
    init_task(&(tasks[1]),1,150000,6,40000,12,gcp_temp2);
    init_task(&(tasks[2]),2,300000,3,30000,40,gcp_temp3);
    
#ifdef WORKGEN 
    IOgen(tasknum,tasks,2100000000,0);
    printf("workload generated!\n");
    return 0;
#endif

    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    //initialize blocklist for blockmanage.
    init_metadata(newmeta);

    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);
    wl_head = init_blocklist(0,-1);

    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);

    for(int i=0;i<tasknum;i++){
        w_util[i] = 0.0;
        r_util[i] = 0.0;
        g_util[i] = 0.0;
        w_wcutil[i] = 0.0;
        r_wcutil[i] = 0.0;
        g_wcutil[i] = 0.0;
    }

    //do initial writing (validate all logical address)
    int logi = (int)(NOP*(1-OP));
    cur_fb = ll_pop(fblist_head);
    ll_append(write_head,cur_fb);
    for(int i=0;i<logi;i++){
        if (cur_fb->fpnum == 0){
            ll_remove(write_head,cur_fb->idx);
            ll_append(full_head,cur_fb);
            cur_fb = ll_pop(fblist_head);
            ll_append(write_head,cur_fb);
            g_cur = (cur_fb->idx+1)*PPB-cur_fb->fpnum;
            
        }
        newmeta->pagemap[g_cur] = i;
        newmeta->rmap[i] = g_cur;
        cur_fb->fpnum--;
        g_cur++;
        total_fp--;
    }//!!finish initial writing
    
    
    int nextGC[tasknum];//stores next GC trigger time.
    for(int i=0;i<tasknum;i++){
        nextGC[i] = tasks[i].gcp;
        printf("first GC time of task %d : %d\n",i,nextGC[i]);
    }
    //[SIMULATION BODY]do simulation for designated checkpoints
    for(int i=1;i<runtime;i++){
        
        //uncomment this area when wl mechanism is necessary
        int cycle_max = get_blockstate_meta(newmeta,OLD);
        int cycle_min = get_blockstate_meta(newmeta,YOUNG);
        if(cycle_max - cycle_min >= THRESHOLD){wl_init = 1;}

        //I/O simulation        
        for(int j=0;j<tasknum;j++){//for each I/O tasks, simulate I/O operation
            
            if(i % tasks[j].wp == 0){//WRITE
                printf("[W]cur time : %d\n",i);
                write_limit = -1;
                cur_fb = write_simul(tasks[j],newmeta,&(g_cur),
                                     fblist_head,write_head,full_head,
                                     cur_fb,&(total_fp),&(w_util[j]),w_workload,write_limit);
                w_wcutil[j] = __calc_wu(&(tasks[j]),cycle_min);
            }//!WRITE
            
            if(i % tasks[j].rp == 0){//READ
                printf("[R]cur time : %d\n",i);
                read_simul(tasks[j],newmeta,&(r_util[j]),0,r_workload);
                r_wcutil[j] = __calc_ru(&(tasks[j]),cycle_max);
            }//!READ
            
            if(i % tasks[j].gcp == 0){//GARBAGE COLLECTION
                printf("[G]cur time : %d\n",i);
                if(full_head->head == NULL){
                    printf("no target block\n");
                    continue;
                }
                if(total_fp <= actual_minrc*tasknum){//if free page is not enough, do GC
                    gc_limit = find_gcctrl(tasks,tasks[j].idx,tasknum,newmeta,full_head);
                    gc_simul(tasks[j],newmeta,
                             fblist_head,full_head,rsvlist_head,
                             &(total_fp),&(g_util[j]),
                             gc_limit,write_limit,&(gc_blockhistory));
                    g_wcutil[j] = __calc_gcu(&(tasks[j]),actual_minrc,cycle_min,cycle_max,cycle_max);
                    gc_count++;
                }
            }//!!GC
        }        
    }//!simulation codes

    printf("[FINAL]block state:\n");
    for(int i=0;i<NOB;i++)
        printf("%d ",newmeta->state[i]);
    printf("\n");
    printf("GC: %d, WL : %d\n",gc_count,wl_count);
    fprintf(fp,"%d,%d,%d,%d\n",gc_count,gc_ctrl,gc_nonworst,wl_count);
    fflush(fp);
    fclose(fp);
    ll_free(fblist_head);
    ll_free(rsvlist_head);
    ll_free(full_head);
    ll_free(write_head);
    free(fblist_head);
    free(rsvlist_head);
    free(full_head);
    free(write_head);
    //free data structures
}