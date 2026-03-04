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

int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head, FILE* gc_detail){
    // !!!find_gc_ has floating point issues
    // sort the victim blocks in order of utilization
    // allocate victim block to GC task proportionally with GC period
    struct timeval a;
    struct timeval b;
    long cal_efficiency = 0;
    long find_best_candidate = 0;

    int gc_period_sort[tasknum];                    // array to store period of GC
    int task_order[tasknum];                        // array to store order of task
    int vic_arr[full_head->blocknum];               // array to store GC candidate indexes.
    float gc_util_arr[full_head->blocknum];         // array to store gc-related utilization (gcutil+blocking)
    int vic_num = 0;                                // number of victim
    int proportion_sum = 0;                         // allocated proportion for task
    int proportion_tot = 0;                         // sum of gc period
    int old = get_blockstate_meta(metadata,OLD);    // oldest block for system
    int min_p =  _find_min_period(task,tasknum);    // minimum I/O period of taskset (necessary to calc blocking)
    int cur_state, copyblock_state, new_rc;         // gc util calc params
    float gc_exec, gc_period, gc_util;              // gc util calc params
    int cur_priority;                               // param for sorting & param for proportion check
    float cur_offset;                               // proportion of current task
    int cur_offset_int;                             // proportion of current task in victim block list
    int best_invalid = 0;                           // edge case handling param
    int best_idx = -1;                              // return value
    float temp;
    float cur_min_util = 2.0f;                      // temp variable to store minimum util(GC+blocking)

#ifdef utilsort_writecheck
    // arrays to test write block array
    int test_arr[full_head->blocknum + write_head->blocknum];
    float test_gcutil_arr[full_head->blocknum + write_head->blocknum];
    char block_origin[full_head->blocknum + write_head->blocknum];
#endif
    // 1. init arrays
    for(int i=0;i<full_head->blocknum;i++){
        vic_arr[i] = -1;
    }
    for(int i=0;i<tasknum;i++){
        gc_period_sort[i] = task[i].gcp;
        task_order[i] = i;
        proportion_tot += gc_period_sort[i];
    }

    // 2. sort gc period
    for(int i=tasknum-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(gc_period_sort[j] > gc_period_sort[j+1]){
                temp = gc_period_sort[j];
                gc_period_sort[j] = gc_period_sort[j+1];
                gc_period_sort[j+1] = temp;
                temp = task_order[j];
                task_order[j] = task_order[j+1];
                task_order[j+1] = temp;
            }
        }
    }

    // 3. find priority of cur gc
    for(int i=0;i<tasknum;i++){
        if(task_order[i] == taskidx){
            cur_priority = i;
        }
    }
    
    // 4. find start offset of cur gc
    for(int i=0;i<cur_priority-1;i++){
        proportion_sum += gc_period_sort[i];
    }
    
    cur_offset = (float)proportion_sum / (float)proportion_tot;
    
    // 5. build up candidate block list
    block* cur = NULL;
    cur = full_head->head;
    gettimeofday(&a,NULL);

    copyblock_state = metadata->state[rsvlist_head->head->idx];
    gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
    const float blocking_old = e_exec(old) / (float)min_p;
    const float blocking_old_next = e_exec(old+1) / (float)min_p;

    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        new_rc = metadata->invnum[cur->idx];
        gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_util = gc_exec/gc_period;
        // printf("gc_util, gc_exec : %f, %f\n",gc_util,gc_exec);

        // restriction 2. MINRC
        if(new_rc < MINRC){
            cur = cur->next;
            continue;
        }
        
        // restriction 1. util
        if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == -1){
            cur = cur->next;
            continue;
        }

        // add a blocking utilization, since GC has a chance to change it.
        // tweak:: as erase execution time becomes step function, approximate blocking factor
        gc_util += (cur_state == old) ? blocking_old_next : blocking_old;
        // if(metadata->state[cur->idx] == old){
        //     // gc_util += e_exec(old+1) / (float)min_p;
        //     // printf("blocking : %f / %f = %f\n",((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE),(float)min_p,((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE) / (float)min_p);
        //     gc_util += ((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE) / (float)min_p;
        // }
        // else{
        //     // gc_util += e_exec(old) / (float)min_p;
        //     // printf("blocking : %f / %f = %f\n",((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE),(float)min_p,((float)(ENDE-STARTE)/(float)MAXPE*(float)(old) + (float)STARTE) / (float)min_p);
        //     gc_util += ((float)(ENDE-STARTE)/(float)MAXPE*(float)(old) + (float)STARTE) / (float)min_p;
        // }

        // insert util & block into candidate block list.
        gc_util_arr[vic_num] = gc_util;
        vic_arr[vic_num] = cur->idx;

#ifdef utilsort_writecheck
        // testcode:: insert util & block into test block list.
        test_gcutil_arr[vic_num] = gc_util;
        test_arr[vic_num] = cur->idx;
        block_origin[vic_num] = 0;
#endif
        vic_num++;
        cur = cur->next;
    }

#ifdef utilsort_writecheck
    // testcode:: search the write block list
    cur = write_head->head;
    int test_vicnum = vic_num;
    gettimeofday(&a,NULL);
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        copyblock_state = metadata->state[rsvlist_head->head->idx];
        new_rc = metadata->invnum[cur->idx];
        gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        // printf("gc_util, gc_exec : %f, %f\n",gc_util,gc_exec);

        // restriction 1. util
        if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == -1){
        // if(0){
            cur = cur->next;
            continue;
        }
        // restriction 2. MINRC
        if(metadata->invnum[cur->idx] < MINRC){
            cur = cur->next;
            continue;
        }
        // add a blocking utilization, since GC has a chance to change it.
        
        if(metadata->state[cur->idx] == old){
            gc_util += e_exec(old+1) / (float)min_p;
        }
        else{
            gc_util += e_exec(old) / (float)min_p;
        }
        // testcode:: insert util & block into test block list.
        test_gcutil_arr[test_vicnum] = gc_util;
        test_arr[test_vicnum] = cur->idx;
        block_origin[test_vicnum] = 1;
        test_vicnum++;
        // printf("[UGC]add block %d(%d,%d,%f),vicnum : %d\n",cur->idx,cur_state,new_rc,gc_util,vic_num);
        cur = cur->next;
    }
#endif
    gettimeofday(&b,NULL);
    // 모든 candidate block에 대한 efficiency utilization을 계산하는 데 소요되는 overhead
    cal_efficiency = b.tv_sec * 1000000 + b.tv_usec - a.tv_sec * 1000000 - a.tv_usec;
    
    // 6. EDGE CASE HANDLING : if vic_num is 0, ignore find_gc_safe and add victims.
    // 즉, schedulability를 만족하는 victim block이 없는 경우, schedulability는 무시하고 MINRC 조건을 만족하는 victim 선택
    if(vic_num == 0){
        printf("vic_num == 0\n");
        cur = full_head->head;

        while(cur != NULL){
            if(metadata->invnum[cur->idx] < MINRC){
                cur = cur->next;
                continue;
            }
            cur_state = metadata->state[cur->idx];
            // copyblock_state = metadata->state[rsvlist_head->head->idx];
            new_rc = metadata->invnum[cur->idx];
            gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
            // gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
            gc_util = gc_exec/gc_period;

            // add a blocking utilization, since GC has a chance to change it.
            gc_util += (cur_state == old) ? blocking_old_next : blocking_old;
            // if(metadata->state[cur->idx] == old){
            // gc_util += e_exec(old+1) / (float)min_p;
            // }
            // else{
            //     gc_util += e_exec(old) / (float)min_p;
            // }

            gc_util_arr[vic_num] = gc_util;
            vic_arr[vic_num] = cur->idx;
            vic_num++;
            cur = cur->next;
        }
    }
    
    // !EDGE CASE HANDLING
    gettimeofday(&a,NULL);
    
    // select the best index to minimize GC overhead in candidate block list
    for(int i=0;i<vic_num;i++){
        if(gc_util_arr[i] <= cur_min_util){
            cur_min_util = gc_util_arr[i];
            best_idx = vic_arr[i];
        }
    }
    // printf("util:%f\n,best_idx:%d,invnum:%d\n",cur_min_util,best_idx,metadata->invnum[best_idx]);
    gettimeofday(&b,NULL);
    find_best_candidate = b.tv_sec * 1000000 + b.tv_usec - a.tv_sec * 1000000 - a.tv_usec;
    fprintf(gc_detail, "%ld, %ld, \n", cal_efficiency, find_best_candidate);

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