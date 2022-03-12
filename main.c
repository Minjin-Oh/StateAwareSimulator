#include "stateaware.h"

//globals
block* cur_fb = NULL;

int main(void){
    //init params
    srand(time(NULL)); 
    bhead* fblist_head;    //heads for block list
    bhead* rsvlist_head;
    bhead* full_head;
    bhead* write_head;
    bhead* wl_head;
    bhead* hotlist;       //overlaps previous blocklist
    bhead* coldlist;

    meta* newmeta = (meta*)malloc(sizeof(meta));
    int g_cur = 0;          //pointer for current writing page
    int tasknum = 1;        //number of task
    int cur_cp = 0;         //current checkpoint time
    int wl_mult = 5;        //period of wl. computed as wl_mult*gcp
    int total_fp = NOP-PPB; //tracks number of free page
    int cps_size = 0;       //number of checkpoints
    int wl_init = 0;        //flag for wear-leveling initiation
    int wl_count = 0;       //a count for how many wl occured
    int gc_count = 0;       //a count for how many gc occured
    int fstamp = 0;         //a period for profile recording.
    float w_util[tasknum];  //runtime utilization tracker
    float r_util[tasknum];
    float g_util[tasknum];
    float WU;               //worst-case utilization tracker.
    float SAWU;

    //profiling
#ifdef DORELOCATE
    FILE* fp = fopen("SA_prof.csv","w");
#endif
#ifndef DORELOCATE
    FILE* fp = fopen("SA_prof_noRR.csv","w");
#endif
    fprintf(fp,"%s\n","timestamp,WU,SAWU,w_util,r_util,g_util,old,yng,std");
    //init tasks(expand this to function for multi-task declaration)
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    int gcp_temp = (int)(__calc_gcmult(75000,5,(int)(PPB*OP)));
    init_task(&(tasks[0]),0,75000,5,75000,500,gcp_temp);

    //init 
    init_metadata(newmeta);
    int* cps = add_checkpoints(1,tasks,1440000000,&(cps_size));
    fblist_head = init_blocklist(0, NOB-tasknum-1);
    rsvlist_head = init_blocklist(NOB-tasknum,NOB-1);
    full_head = init_blocklist(0,-1);//generate 0 component ll.
    write_head = init_blocklist(0,-1);
    wl_head = init_blocklist(0,-1);
    hotlist = init_blocklist(0,-1);
    coldlist = init_blocklist(0,-1);

    //do initial writing(validate all logical address)
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
        //printf("[INIT]mapping %d and %d\n",g_cur,i);
        newmeta->pagemap[g_cur] = i;
        newmeta->rmap[i] = g_cur;
        cur_fb->fpnum--;
        g_cur++;
        total_fp--;
    }//!!finish initial writing
    
    //utilization function check codes
    util_check_main();

    //[SIMULATION BODY]do simulation for designated checkpoints
    for(int i=0;i<cps_size;i++){
        int cycle_max = get_blockidx_meta(newmeta,OLD);
        int cycle_min = get_blockidx_meta(newmeta,YOUNG);
        if(cycle_max - cycle_min >= THRESHOLD){wl_init = 1;}
        
        //update current chekpoint
        cur_cp = cps[i];
        if((i != 0) && (cur_cp == cps[i-1])){
            continue; //skip considered checkpoint
        }

        //I/O simulation
        if(cur_cp % tasks[0].wp == 0){ //WRITE 
            //!!!write_simul MUST return & update current write block
            //for next write_simul !!!
            //printf("[%d]",cur_cp);
            cur_fb = write_simul(tasks[0],newmeta,&(g_cur),fblist_head,
                        write_head,full_head,cur_fb,&(total_fp),&(w_util[0]));
        }//!WRITE

        if(cur_cp % tasks[0].rp == 0){ //READ
            read_simul(tasks[0],newmeta,&(r_util[0]));
        }
        //!READ
        int fp_from_blocks = 0;
        if(cur_cp % tasks[0].gcp == 0){ //GARBAGE COLLECTION
            if(full_head->head == NULL){
                printf("no target block\n");
                continue;
            }    
            printf("[%d][GC] cur fp %d\n",cur_cp,total_fp);
            if(total_fp < 1){//if free page is enough, skip the GC
                gc_simul(tasks[0],newmeta,fblist_head,full_head,rsvlist_head,&(total_fp),&(g_util[0]));
                gc_count++;
            }

            //print functions
            printf("[%d][GC] inc fp %d\n",cur_cp,total_fp);
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

            //profile functions
            int old = get_blockidx_meta(newmeta,OLD);
            int yng = get_blockidx_meta(newmeta,YOUNG);

            if (old - yng >= 5){
                printf("[STAT]erase cycles : ");
                for(int j=0;j<NOB;j++){
                    printf("%d ",newmeta->state[j]);
                }
            }
            printf("[runtime]W_util, R_util, G_util:%f %f %f\n",w_util[0],r_util[0],g_util[0]);
            fstamp++;
            if(fstamp == STAMP){
                fprintf(fp,"%d,%f,%f,%f,%f,%f,%d,%d,%f\n",
                        cur_cp,                                
                        WU,SAWU,w_util[0],r_util[0],g_util[0], 
                        old,yng,calc_std(newmeta));
                fstamp = 0;
                fflush(fp);
            }

            
            //worst-case utilization finder
            WU = find_worst_util(tasks,1,newmeta);
            SAWU = find_SAworst_util(tasks,1,newmeta);
            if(SAWU >= 1.0){
                fprintf(fp,"%d,%d\n",gc_count,wl_count);
                fclose(fp);
                printf("utilization overflowed. exit in 5 seconds...\n");
                sleep(5);
                abort();
            } 
                        
        }//!!GC

#ifdef DORELOCATE        
        if((cur_cp % tasks[0].gcp*wl_mult == 0) && (wl_init == 1)){ //WEAR LEVELING
            //utilization based read-relocation approach
            if(WU >= 0.4){
                //printf("==[access window]==\n");
                //for(int i=0;i<NOB;i++){
                //    printf("%d ",newmeta->access_window[i]);
                //}
                //printf("\n");
                //find block with hottest read
                int hotblock = -1;
                int hotcount = -1;
                for(int i=0;i<NOB;i++){
                    if(newmeta->access_window[i]>hotcount){
                        if(is_idx_in_list(rsvlist_head,i)){
                            printf("[HOT]that block is rsv. skip %d\n",i);
                        }
                        else if(is_idx_in_list(write_head,i)){
                            printf("[HOT]that block is wrt. skip %d\n",i);
                        }
                        else{
                            hotcount = newmeta->access_window[i];
                            hotblock = i;
                        }
                    }
                }
                //find fresh block, which is not wrt or rsv
                int frsh = -1;
                int frshstate = MAXPE*2+1;
                for(int i=0;i<NOB;i++){
                    if(frshstate >= newmeta->state[i]){
                        if(is_idx_in_list(rsvlist_head,i)){
                            printf("[CLD]that block is rsv. skip %d\n",i);
                        }
                        else if(is_idx_in_list(write_head,i)){
                            printf("[CLD]that block is wrt. skip %d\n",i);
                        }
                        else{
                            frshstate = newmeta->state[i];
                            frsh = i;
                        }
                    }
                }

                //check if hottest block is aged enough
                int old = get_blockidx_meta(newmeta,OLD);
                int yng = get_blockidx_meta(newmeta,YOUNG);
                if((newmeta->state[hotblock]>(yng+old)/2) && (frsh != hotblock)){
                    wl_simul(newmeta,fblist_head,full_head,hotlist,coldlist,hotblock,frsh,&(total_fp));
                    wl_count++;
                    //FIXME::due to the various edge cases, 
                    //we simply RECALCULATE total_fp in here.
                    block* t = fblist_head->head;
                    int new_tot = 0;
                    while(t!=NULL){
                        new_tot += t->fpnum;
                        t=t->next;
                    }
                    new_tot += write_head->head->fpnum;
                    total_fp = new_tot;
                    for(int i=0;i<NOB;i++){
                        newmeta->access_window[i] = 0;
                    }
                }
                else if (frsh == hotblock){
                    printf("hot block is freshest now. skip WL.\n");
                }
                else if (newmeta->state[hotblock]<=MAXPE/2){
                    printf("hotblock is tolerable\n");
                }
            }
            
            /*
            //WL test code 1. build hot-cold list
            if(hotlist->head == NULL){
                build_hot_cold(newmeta,hotlist,coldlist,cycle_max);
                printf("list build finished\n");
            }
            
            //WL test code 2. swap 2 full(or fb) block
            //int aged = get_blkidx_byage(newmeta,hotlist,rsvlist_head,write_head,OLD,0);
            //int frsh = get_blkidx_byage(newmeta,coldlist,rsvlist_head,write_head,YOUNG,0);
            //printf("swapping %d and %d\n",aged,frsh);
            //wl_simul(newmeta,fblist_head,full_head,hotlist,coldlist,aged,frsh);
            
            //WL test code 3. swap 2 block at each period until some threshold
            
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
    //!simulation codes
    printf("[FINAL]block state:\n");
    for(int i=0;i<NOB;i++)
        printf("%d ",newmeta->state[i]);
    printf("\n");
    printf("GC: %d, WL : %d\n",gc_count,wl_count);
    fclose(fp);
    //free data structures

}