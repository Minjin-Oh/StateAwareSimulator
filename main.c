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
    int expected_fp = PPB*NOB - max_valid_pg - expected_invalid;
    printf("expected_invalid : %d, expected_fp : %d\n",expected_invalid,expected_fp);
    
    //init tasks(expand this to function for multi-task declaration)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(75000,11,MINRC));
    int gcp_temp2 = (int)(__calc_gcmult(150000,6,MINRC));
    int gcp_temp3 = (int)(__calc_gcmult(300000,3,MINRC)); 
    init_task(&(tasks[0]),0,75000,11,75000,100,gcp_temp);
    init_task(&(tasks[1]),1,150000,6,40000,12,gcp_temp2);
    init_task(&(tasks[2]),2,300000,3,30000,40,gcp_temp3);

#ifdef WORKGEN 
    IOgen(tasknum,tasks,2100000000,0);
    printf("workload generated!\n");
    return 0;
#endif

    //open csv files for profiling and workload
    w_workload = fopen("workload_w.csv","r");
    r_workload = fopen("workload_r.csv","r");
    rr_profile = fopen("rr_prof.csv","w");
    fprintf(fp,"%s\n","timestamp,taskidx,WU,actual_WU,w_util,w_wc,r_util,r_wc,g_util,g_wc,old,yng,std,gc_limit");
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
    
    int gc_limit = MAXPE;
    int gc_blockhistory = -1;
    int write_limit = 0;

    //[SIMULATION BODY]do simulation for designated checkpoints
    for(int i=0;i<cps_size;i++){
        
        //uncomment this area when wl mechanism is necessary
        int cycle_max = get_blockstate_meta(newmeta,OLD);
        int cycle_min = get_blockstate_meta(newmeta,YOUNG);
        if(cycle_max - cycle_min >= THRESHOLD){
            wl_init = 1;
        }
        
        //update current chekpoint
        cur_cp = cps[i];
        if((i != 0) && (cur_cp == cps[i-1])){
            continue; //skip considered checkpoint
        }
        
        //I/O simulation
        for(int j=0;j<tasknum;j++){//for each I/O tasks, simulate I/O operation
            if(cur_cp % tasks[j].wp == 0){//WRITE 
#ifdef DOWRCONTROL
                write_limit = find_writectrl(&(tasks[j]),j,tasknum,newmeta);
#endif
#ifndef DOWRCONTROL
                write_limit = -1;
#endif
                cur_fb = write_simul(tasks[j],newmeta,&(g_cur),
                                     fblist_head,write_head,full_head,
                                     cur_fb,&(total_fp),&(w_util[j]),w_workload,write_limit);
                w_wcutil[j] = __calc_wu(&(tasks[j]),cycle_min);
            }//!WRITE

            if(cur_cp % tasks[j].rp == 0){//READ
                read_simul(tasks[j],newmeta,&(r_util[j]),0,r_workload);
                r_wcutil[j] = __calc_ru(&(tasks[j]),cycle_max);
            }//!READ
            
#ifdef DOGC            
            int fp_from_blocks = 0;
            
            if(cur_cp % tasks[j].gcp == 0){ //GARBAGE COLLECTION
                //edge :: every block is free block
                if(full_head->head == NULL){
                    printf("no target block\n");
                    continue;
                }
                int old = get_blockstate_meta(newmeta,OLD);
                int yng = get_blockstate_meta(newmeta,YOUNG);
                int prev_tot;

                //if free page is not enough, do GC
                if(newmeta->total_invalid >= expected_invalid){
                    printf("==value checking==\n");
                    int inv_test = 0;
                    for(int k=0;k<NOB;k++){
                        inv_test+=newmeta->invnum[k];
                    }
                    printf("tot_inv : %d vs inv in block : %d\n",newmeta->total_invalid,inv_test);
                    printf("==value test end==\n");
                    gc_limit = find_gcctrl(tasks,tasks[j].idx,tasknum,newmeta,full_head);
                    prev_tot = total_fp;
                    printf("[%d][GC] cur fp %d\n",cur_cp,total_fp);
                    printf("[%d][GC] gc_limit is %d\n",cur_cp,gc_limit);
                    gc_simul(tasks[j],tasknum,newmeta,
                             fblist_head,full_head,rsvlist_head,
                             &(total_fp),&(g_util[j]),gc_limit,write_limit,&(gc_blockhistory));
                    g_wcutil[j] = __calc_gcu(&(tasks[j]),MINRC,cycle_min,cycle_max,cycle_max);
                    printf("[%d][GC] inc fp %d\n",cur_cp,total_fp);
                    gc_count++;
                    
                    //profile functions
                    //worst case testing
                    WU = find_worst_util(tasks,tasknum,newmeta);
                    SAWU = find_SAworst_util(tasks,tasknum,newmeta);
                    float actual_WU = WU;
                    for(int k=0;k<tasknum;k++){
                        if(w_util[k] > w_wcutil[k]){
                            actual_WU = actual_WU - w_wcutil[k]+w_util[k];
                            printf("[W]change %f to %f\n",w_wcutil[k],w_util[k]);
                        }
                        if(r_util[k] > r_wcutil[k]){
                            actual_WU = actual_WU - r_wcutil[k]+r_util[k];
                            printf("[R]change %f to %f\n",r_wcutil[k],r_util[k]);
                        }
                        if(g_util[k] > g_wcutil[k]){
                            actual_WU = actual_WU - g_wcutil[k]+g_util[k];
                            printf("[G]change %f to %f\n",g_wcutil[k],g_util[k]);
                        }
                    }
                    //runtime utilization sum
                    float run_util=0.0;
                    for(int k=0;k<tasknum;k++){
                        run_util += w_util[k]+r_util[k]+g_util[k];
                    }
                    //profile the information first.
                    fstamp++;
                    if(fstamp == STAMP){
                        old = get_blockstate_meta(newmeta,OLD);
                        yng = get_blockstate_meta(newmeta,YOUNG);
                        fprintf(fp,"%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%f,%d,%d,%d,%d,%f\n",
                                cur_cp,j,                                
                                WU,actual_WU,
                                w_util[j],w_wcutil[j],
                                r_util[j],r_wcutil[j],
                                g_util[j],g_wcutil[j],
                                old,yng,calc_std(newmeta),
                                gc_limit,prev_tot,total_fp,total_fp-prev_tot,run_util);
                        fstamp = 0;
                        fflush(fp);
                    }
                    if(WU >= 1.0){
                        fprintf(fp,"%d,%d\n",gc_count,wl_count);
                        fflush(fp);
                        fflush(rr_profile);
                        fclose(fp);
                        fclose(rr_profile);
                        printf("utilization overflowed. exit in 5 seconds...\n");
                        sleep(5);
                        abort();
                    }
                    //check if new worstcase goes over 1.0
                    //if (actual_WU > 1.0){
                    //    fprintf(fp,"%d,%d\n",gc_count,wl_count);
                    //    fclose(fp);
                    //    printf("!!!actual utilization overflowed. exit in 5 seconds...\n");
                    //    sleep(5);
                    //    abort();
                    //}  

                }
#ifdef GCDEBUG
                //print free block information
                block* temp = fblist_head->head;
                printf("[%d][GC] idx(fps) :",cur_cp);
                while(temp!=NULL){
                    printf("%d(%d) ",temp->idx, temp->fpnum);
                    fp_from_blocks += temp->fpnum;
                    temp=temp->next;

                }
                printf("%d(%d) ",write_head->head->idx,write_head->head->fpnum);
                fp_from_blocks += write_head->head->fpnum;
                printf("\n");
                printf("fp in block : %d vs counter : %d\n",fp_from_blocks,total_fp);
                if(fp_from_blocks != total_fp) abort();

                //print erase cycle information
                printf("[STAT]erase cycles : ");
                for(int j=0;j<NOB;j++){
                    printf("%d ",newmeta->state[j]);
                }
                printf("\n");
                printf("[runtime]W_util, R_util, G_util:%f %f %f\n",w_util[j],r_util[j],g_util[j]);
#endif
            }//!!GC
#endif 

#ifdef DORELOCATE        
            if((cur_cp % (tasks[0].gcp*wl_mult) == 0) && (wl_init == 1)){ //RELOCATION(a.k.a WL)
                //utilization based read-relocation approach
                if(WU >= 0.4){
                    if(j != 0){
                        continue;
                    }
                    int high = -1;
                    int low = -1;
                    find_RR_target(newmeta,fblist_head,full_head,&high,&low);
                    printf("[WL]passed value : %d, %d\n",high,low);
                    if(high != -1 && low != -1){
                        if(newmeta->state[high] - newmeta->state[low] >= THRESHOLD && 
                           newmeta->access_window[high] - newmeta->state[low] > 0){
                            //if condition meets, do relocation
                            wl_simul(newmeta,tasknum,fblist_head,full_head,hotlist,coldlist,high,low,&(total_fp));
                            wl_count++;
                            //FIXME::due to the various edge cases, 
                            //we RECALCULATE total_fp in here.
                            block* t = fblist_head->head;
                            int new_tot = 0;
                            while(t != NULL){
                                new_tot += t->fpnum;
                                t=t->next;
                            }
                            new_tot += write_head->head->fpnum;
                            total_fp = new_tot;
                            //profile
                            fprintf(rr_profile,"%d,%d,%d,%d,%d,%d,%d\n",cur_cp,
                                    high,newmeta->state[high],newmeta->access_window[high],
                                    low ,newmeta->state[low] ,newmeta->access_window[low]);
                            fflush(rr_profile);
                            //reset access window for accurate hotness detection
                            for(int i=0;i<NOB;i++){
                                newmeta->access_window[i] = 0;
                            }
                        }
                        else{
                            printf("skip this RR. difference too small\n");
                        }
                    }
                }
                
                /*
                //build hot-cold list
                if(hotlist->head == NULL){
                    build_hot_cold(newmeta,hotlist,coldlist,cycle_max);
                    printf("list build finished\n");
                }
        
                int hot_old = get_blkidx_byage(newmeta,hotlist,rsvlist_head,write_head,OLD,0);
                int cold_young = get_blkidx_byage(newmeta,coldlist,rsvlist_head,write_head,YOUNG,0);
                printf("[hot]%d, [cold]%d, threshold is %d\n",
                        newmeta->state[hot_old],
                        newmeta->state[cold_young],
                        THRESHOLD-MARGIN);
                
                if(newmeta->state[hot_old] - newmeta->state[cold_young] >= THRESHOLD-MARGIN){
                    int aged = get_blkidx_byage(newmeta,hotlist,rsvlist_head,write_head,OLD,0);
                    int frsh = get_blkidx_byage(newmeta,coldlist,rsvlist_head,write_head,YOUNG,0);
                    int old_tot_fp = total_fp;
                    wl_simul(newmeta,fblist_head,full_head,hotlist,coldlist,aged,frsh,&(total_fp));
                    wl_count++;

                    //print codes for debugging
                    printf("tot fp change: %d to %d\n",old_tot_fp,total_fp);
                    block* temp = fblist_head->head;
                    int tempsum = 0;
                    printf("[%d][WL] idx(fps) :",cur_cp);
                    while(temp!=NULL){
                        printf("%d(%d) ",temp->idx,temp->fpnum);
                        tempsum+=temp->fpnum;
                        temp=temp->next;
                    }
                    tempsum+=write_head->head->fpnum;
                    printf("%d => %d ",write_head->head->fpnum,tempsum);
                    printf("\n");
                    printf("-------------------------------------\n");
                    sleep(1);
                    //print code end
                }//WL end
                */

            }
#endif  
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