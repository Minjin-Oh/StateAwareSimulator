#include "findGC.h"

extern double OP;
extern int MINRC;

//FIXME:: a temporary solution to expose
extern IOhead** rq;
extern int update_cnt[NOP];
extern int max_valid_pg;
extern long* lpa_update_timing[NOP];

int _find_gc_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util, int cur_b, int rsv_b){
    //check if current I/O job does not violate util test along with recently released other jobs.
    
    //init variables
    IO* cur;
    int read_b;
    int valid_cnt = 0;
    int* lpas;
    float total_u = 0.0;
    float old_total_u = 0.0;
    float wutils[tasknum];
    float rutils[tasknum];
    float gcutils[tasknum];
    /*
    //test code:: compare with old find_util_safe function
    old_total_u = find_cur_util(tasks,tasknum,metadata,old);
    if(type == WR){
        old_total_u -= metadata->runutils[0][taskidx];
    } else if (type == RD){
        old_total_u -= metadata->runutils[1][taskidx];
    } else if (type == GC){
        old_total_u -= metadata->runutils[2][taskidx];
    }
    old_total_u += util;
    */
    //malloc variables
    lpas = (int*)malloc(sizeof(int)*PPB);
    for(int i=0;i<PPB;i++){
        lpas[i] = -1;
    }

    //allocate current runtime utils on local variable
    for(int i=0;i<tasknum;i++){
        wutils[i] = metadata->runutils[0][i];
        rutils[i] = metadata->runutils[1][i];
        gcutils[i] = metadata->runutils[2][i];
    }
    //get victim block valid lpas.
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[cur_b*PPB+i]==0 && metadata->rmap[cur_b*PPB+i]!=-1){
            lpas[valid_cnt] = metadata->rmap[cur_b*PPB+i];
            valid_cnt++;
        }
    }

    /*write a code which checks if current lpa collides with read.*/
    /*do not consider collision with write, since worst case is when GC happens after write*/
    /*when GC happens after write, write reqs are not affected at all.*/
    /*when write happens after GC, write reqs invalidate relocated page, but write util X change*/
    
    for (int i=0;i<tasknum;i++){
        cur = rq[i]->head;
        while (cur != NULL){
            //write a code which compares lpa in lpas collide with read request.
            //if collision occurs && new block is worse case, update read utilization
            for(int j=0;j<valid_cnt;j++){
                if(cur->lpa == lpas[j]){
                    //printf("collision detected, %d vs %d ",cur->lpa,lpas[j]);
                    read_b = metadata->pagemap[lpas[j]]/PPB;
                    //printf("state : %d, %d\n",metadata->state[read_b],metadata->state[rsv_b]);
                    if(metadata->state[read_b] < metadata->state[rsv_b]){
                        rutils[i] -= r_exec(metadata->state[read_b]) / (float)tasks[i].rp;
                        rutils[i] += r_exec(metadata->state[rsv_b]) / (float)tasks[i].rp;
                    }
                }
                
            }
            cur = cur->next;
        }
    }
    
    free(lpas);
    
    //now calculate total utilization.
    for (int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += wutils[j];
        total_u += rutils[j];
        total_u += gcutils[j];
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    total_u -= gcutils[taskidx];
    total_u += util;
    if (total_u <= 1.0){
        return 0;
    } else if (total_u > 1.0){
        return -1;
    }    
}

// Comparison function for qsort (for sorting task periods)
static int _compare_periods(const void *a, const void *b) {
    int val_a = *(int*)a;
    int val_b = *(int*)b;
    return (val_a > val_b) - (val_a < val_b);
}

// Helper structure for sorting periods along with task indices
struct period_task {
    int period;
    int taskidx;
};

static int _compare_period_task(const void *a, const void *b) {
    int val_a = ((struct period_task*)a)->period;
    int val_b = ((struct period_task*)b)->period;
    return (val_a > val_b) - (val_a < val_b);
}

int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head, FILE* gc_detail){
    // !!!find_gc_ has floating point issues
    // sort the victim blocks in order of utilization
    // allocate victim block to GC task proportionally with GC period

    int vic_arr[full_head->blocknum];               // array to store GC candidate indexes.
    float gc_util_arr[full_head->blocknum];         // array to store gc-related utilization (gcutil+blocking)
    int vic_num = 0;                                // number of victim
    int old = get_blockstate_meta(metadata,OLD);    // oldest block for system
    int min_p = _find_min_period(task,tasknum);     // minimum I/O period of taskset (necessary to calc blocking)
    int cur_state, new_rc;                          // gc util calc params
    float gc_exec, gc_util;                         // gc util calc params
    int best_idx = -1;                              // return value
    float cur_min_util = 2.0f;                      // temp variable to store minimum util(GC+blocking)

    // Pre-compute constant values to avoid redundant calculations
    int copyblock_state = metadata->state[rsvlist_head->head->idx];
    float gc_period = (float)_gc_period(&(task[taskidx]), (int)(MINRC));
    const float blocking_old = e_exec(old) / (float)min_p;
    const float blocking_old_next = e_exec(old+1) / (float)min_p;
    float w_exec_copy = w_exec(copyblock_state);    // cache w_exec result
    
    // 1. build up candidate block list from full_head
    block* cur = full_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        new_rc = metadata->invnum[cur->idx];
        
        // Early rejection: MINRC check first (cheaper than _find_gc_safe)
        if(new_rc >= MINRC){
            gc_exec = (float)(PPB-new_rc)*(w_exec_copy+r_exec(cur_state))+e_exec(cur_state);
            gc_util = gc_exec/gc_period;
            
            // Then check util constraint (more expensive operation)
            if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == 0){
                // add a blocking utilization, since GC has a chance to change it.
                // tweak:: as erase execution time becomes step function, approximate blocking factor
                gc_util += (cur_state == old) ? blocking_old_next : blocking_old;

                // insert util & block into candidate block list.
                gc_util_arr[vic_num] = gc_util;
                vic_arr[vic_num] = cur->idx;
                vic_num++;
            }
        }
        cur = cur->next;
    }

    // 2. EDGE CASE HANDLING: if vic_num is 0, ignore find_gc_safe and add victims.
    // When no blocks satisfy schedulability, fall back to MINRC-only constraint
    if(vic_num == 0){
        printf("vic_num == 0\n");
        cur = full_head->head;
        while(cur != NULL){
            new_rc = metadata->invnum[cur->idx];
            if(new_rc >= MINRC){
                cur_state = metadata->state[cur->idx];
                gc_exec = (float)(PPB-new_rc)*(w_exec_copy+r_exec(cur_state))+e_exec(cur_state);
                gc_util = gc_exec/gc_period;
                gc_util += (cur_state == old) ? blocking_old_next : blocking_old;
                
                gc_util_arr[vic_num] = gc_util;
                vic_arr[vic_num] = cur->idx;
                vic_num++;
            }
            cur = cur->next;
        }
    }

    // 3. Select the block with minimum GC overhead from candidate list
    for(int i=0;i<vic_num;i++){
        if(gc_util_arr[i] < cur_min_util){
            cur_min_util = gc_util_arr[i];
            best_idx = vic_arr[i];
        }
    }

    return best_idx;
}

block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head){
    // find a destination of current page, similar to find_maxinvalid function.
    // FIXME:: this prototype function is hardcoded for TIMING_ON_MEM
    long cur_lpa_timing;
    int cnt;
    int cur_lpa_nextupdatenum;
    int target;
    int yield_pg;
    block *wb_new, *cur;

    // find relocated page's update timing & calculate its order.
    cur_lpa_nextupdatenum = metadata->write_cnt_per_cycle[lpa] + 1;
    if(cur_lpa_nextupdatenum >= update_cnt[lpa]){
        cur_lpa_timing = workload_reset_time + WORKLOAD_LENGTH;
    } 
    else {
        cur_lpa_timing = lpa_update_timing[lpa][cur_lpa_nextupdatenum];
    }
    cnt = __calc_invorder_mem(max_valid_pg, metadata, cur_lpa_timing, workload_reset_time, metadata->total_fp);
    
    // Edgecase 1:: free page is not enough to handle target lpa
    if(cnt >= metadata->total_fp){
        while(fblist_head->blocknum != 0){
            // search through free block list, finding a youngest block,
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        // give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                return cur;    
            }
            cur = cur->next;
        }
    }

    // normal case. find order within write block list.
    yield_pg = 0;
    cur = write_head->head;
    while(cur != NULL){
        yield_pg += cur->fpnum;
        if(yield_pg > cnt){
            // if current block has enough page to satisfy update order, break from loop
            break;
        } else {
            // search next if not
            cur = cur->next;
        }
    }
    // Edgecase 2:: write block list X have corresponding block.
    if(cur==NULL){
        while(fblist_head->blocknum != 0){
            // search through free block list, finding a youngest block, 
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
            yield_pg += wb_new->fpnum;
            if(yield_pg > cnt){
                cur = wb_new;
                break;
            } else {
                // do nothing. 
                // repeat while loop until free block runs out.
            }
            // printf("[FB]yieldpg : %d\n",yield_pg);
        }
        if(cur != NULL){
            return cur;
        }
    }
    // Edgecase 3::somehow no corresponding block.
    if(cur==NULL){
        while(fblist_head->blocknum != 0){
            // search through free block list, finding a youngest block, 
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        // give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                return cur;    
            }
            cur = cur->next;
        }
    }
    return cur;
}