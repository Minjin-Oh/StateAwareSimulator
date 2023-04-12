#include "findGC.h"

extern double OP;
extern int MINRC;

//FIXME:: a temporary solution to expose queue to find_util_safe function.
extern IOhead** wq;
extern IOhead** rq;
extern IOhead** gcq;

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

int find_gcctrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head){
    //find "the block" which is suitable for GC, considering utilization
    //!!!returns the block number, not limit
    //find the value for youngest/oldest block
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);
    block* cur = full_head->head;
    int new_rc = 0;
    int expected_idx = -1;
    int cur_invalid = 0;
    int cur_target = -1;
    int cur_state;
    float cur_wcutil = 0.0;
    float cur_minutil = 1.0;
    float cur_gc,gc_exec,gc_period;
    float util_profile[MAXPE];

    //update read_worst
    update_read_worst(metadata,tasknum);
    
    //scan the metadata and find a block with lowest utilization
    for(int i=0;i<MAXPE+1;i++){
        util_profile[i] = 0.0;
        if(i <= metadata->cur_read_worst[taskidx]){
            util_profile[i] += __calc_ru(&(task[taskidx]),metadata->cur_read_worst[taskidx]);
        } else {
            util_profile[i] += __calc_ru(&(task[taskidx]),i);
        }
        util_profile[i] += __calc_wu(&(task[taskidx]),i);
    }

    float gc_runutil = 0.0;
    float profile_util = 0.0;
    while(cur != NULL){
        //if GCing current block is impossible, select another one
        
        if(metadata->invnum[cur->idx] >= MINRC){
            cur_state = metadata->state[cur->idx];
            new_rc = metadata->invnum[cur->idx];
            float profile_util = 0.0;
            float gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            float gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
            cur_gc = gc_exec/gc_period;
            profile_util = cur_gc + util_profile[cur_state+1];
            if(profile_util <= cur_minutil){
                expected_idx = cur->idx;
                cur_minutil = cur_gc + util_profile[cur_state+1];
                cur_invalid = metadata->invnum[cur->idx];
            }
        }
        cur = cur->next;
    }
    if(expected_idx != -1){
        //printf("[ctrl]expected target %d, inv : %d, util : %f, state : %d\n",expected_idx,cur_invalid,cur_minutil,metadata->state[expected_idx]);
    }
    //EDGE CASE HANDLING!!
    if(expected_idx == -1){
        cur = full_head->head;
        cur_invalid = metadata->invnum[cur->idx];
        while(cur != NULL){
            if(metadata->invnum[cur->idx] >= cur_invalid){
                expected_idx = cur->idx;
                cur_invalid = metadata->invnum[cur->idx];
                cur_minutil = __calc_gcu(&(task[taskidx]),MINRC,yng,metadata->state[cur->idx],metadata->state[cur->idx]);
            }
            cur = cur->next;
        }
    }
    
    //printf("expected target %d, inv : %d, util : %f, state : %d\n",expected_idx,cur_invalid,cur_minutil,metadata->state[expected_idx]);
    return expected_idx;
}

int find_gcctrl_greedy(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head){
    //find "the block" which is suitable for GC, considering utilization
    //!!!returns the block number, not limit
    //find the value for youngest/oldest block
    block* cur = full_head->head;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int cur_invalid = metadata->invnum[cur->idx];
    int expected_idx, cur_state, new_rc;
    float cur_minutil = 1.0;
    float profile_util, gc_exec, gc_period;
    while(cur != NULL){
        if(metadata->invnum[cur->idx] >= MINRC){
            cur_state = metadata->state[cur->idx];
            new_rc = metadata->invnum[cur->idx];
            gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            gc_period = _gc_period(&(task[taskidx]),(int)(MINRC));
            profile_util = gc_exec/gc_period;
            if(profile_util <= cur_minutil){
                expected_idx = cur->idx;
                cur_minutil = profile_util;
                cur_invalid = metadata->invnum[cur->idx];
            }
        }
        cur = cur->next;
    }
    //printf("expected target %d, inv : %d, state : %d\n",expected_idx,cur_invalid,metadata->state[expected_idx]);
    return expected_idx;
}

int find_gcctrl_yng(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head){
    block* cur = full_head->head;
    //cur_min_state : current minimum flash block
    //slack : left util (1.0 - (runtime util - task's recent write util))
    int yng = get_blockstate_meta(metadata,YOUNG);
    int cur_min_state = MAXPE;
    int new_rc = -1, cur_state = -1, cur_invalid = -1;
    float gc_exec, gc_period, gc_util;
    float slack = 1.0 - find_cur_util(task,tasknum,metadata,get_blockstate_meta(metadata,OLD)) + metadata->runutils[2][taskidx];
    //printf("slack : %f, gcutil : %f, curutil : %f\n",slack,metadata->runutils[2][taskidx],find_cur_util(task,tasknum,metadata,get_blockstate_meta(metadata,OLD)));
    int expected_idx = -1;
    while(cur != NULL){
        if(metadata->invnum[cur->idx] >= MINRC){
            new_rc = metadata->invnum[cur->idx];
            cur_state = metadata->state[cur->idx];
            gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
            gc_util = gc_exec/gc_period;
            slack = 1.0; //uncomment this to only consider block age
            if(gc_util <= slack){
                if(metadata->state[cur->idx] <= cur_min_state){
                    cur_min_state = metadata->state[cur->idx];
                    expected_idx = cur->idx;
                }    
            }
        }
        cur = cur->next;
    }
    //printf("expected target %d, inv : %d, state : %d\n",expected_idx,metadata->invnum[expected_idx],metadata->state[expected_idx]);
    //EDGE CASE HANDLING!!
    if(expected_idx == -1){
        cur = full_head->head;
        cur_invalid = metadata->invnum[cur->idx];
        while(cur != NULL){
            if(metadata->invnum[cur->idx] >= cur_invalid){
                expected_idx = cur->idx;
                cur_invalid = metadata->invnum[cur->idx];
            }
            cur = cur->next;
        }
        //printf("[edge]expected target %d, inv : %d, state : %d\n",expected_idx,metadata->invnum[expected_idx],metadata->state[expected_idx]);
    }
    return expected_idx;
}

int find_gcctrl_limit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head){
    //find gc victim considering expected read util change & write util change
    
    //params
    int cur_ppa;
    
    int new_rc;
    int cur_state;
    int copyblock_state;
    float ru, wu;
    float calib_read_lat;
    float gc_exec, gc_period, gc_util;
    block* cur = full_head->head;
    int best_idx = -1;
    int best_invalid = 0;
    int old = get_blockstate_meta(metadata,OLD);
    float cur_best_util = 1.0;
    float cur_read_lat = calc_readlatency(task, metadata, taskidx);

    //search through full blocks to find gc block
    while(cur != NULL){
        //restriction 1. util
        cur_state = metadata->state[cur->idx];
        copyblock_state = metadata->state[rsvlist_head->head->idx];
        new_rc = metadata->invnum[cur->idx];
        gc_exec = (PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        if(find_util_safe(task,tasknum,metadata,old,taskidx,GC,gc_util ) == -1){
            cur = cur->next;
            continue;
        }

        //restriction 2. MINRC
        if(metadata->invnum[cur->idx] < MINRC){
            cur = cur->next;
            continue;
        }

        //if restriction is met, check if block is optimal
        //calculate expected read util change
        calib_read_lat = cur_read_lat;
        for(int i=0;i<PPB;i++){
            cur_ppa = cur->idx*PPB + i;
            if(metadata->vmap_task[cur_ppa] == taskidx){
                calib_read_lat = calib_readlatency(metadata,taskidx,calib_read_lat,cur_ppa,(rsvlist_head->head->idx)*PPB);
            }
        }
        ru = cur_read_lat * task[taskidx].rn / task[taskidx].rp;
        //calculate expected write util change
        wu = __calc_wu(&(task[taskidx]),metadata->state[cur->idx]);
        //check if current util value is optimal
        if (ru+wu <= cur_best_util){
            cur_best_util = ru+wu;
            best_idx = cur->idx;
        }
        cur = cur->next;
    }
    //EDGE CASE HANDLING!!
    if(best_idx == -1){
        cur = full_head->head;
        best_invalid = metadata->invnum[cur->idx];
        while(cur != NULL){
            if(metadata->invnum[cur->idx] >= best_invalid){
                best_idx = cur->idx;
                best_invalid = metadata->invnum[cur->idx];
            }
            cur = cur->next;
        }
    }
    return best_idx;
}

int find_gcweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head){
    //find gc victim considering expected read util change & write util change
    
    //params
    int cur_ppa;
    
    int new_rc;
    int cur_state;
    int copyblock_state;
    int gcskipfactor;
    float ru, wu, gcu;
    float calib_read_lat;
    float gc_exec, gc_period, gc_util;
    block* cur = full_head->head;
    int best_idx = -1;
    int best_invalid = 0;
    int old = get_blockstate_meta(metadata,OLD);
    float cur_best_util = 1.0;
    float cur_read_lat = calc_readlatency(task, metadata, taskidx);

    //search through full blocks to find gc block
    while(cur != NULL){
        //restriction 1. util
        cur_state = metadata->state[cur->idx];
        copyblock_state = metadata->state[rsvlist_head->head->idx];
        new_rc = metadata->invnum[cur->idx];
        gc_exec = (PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        if(find_util_safe(task,tasknum,metadata,old,taskidx,GC,gc_util ) == -1){
            cur = cur->next;
            continue;
        }

        //restriction 2. MINRC
        if(metadata->invnum[cur->idx] < MINRC){
            cur = cur->next;
            continue;
        }

        //if restriction is met, check if block is optimal
        //calculate expected read util change
        calib_read_lat = cur_read_lat;
        for(int i=0;i<PPB;i++){
            cur_ppa = cur->idx*PPB + i;
            if(metadata->vmap_task[cur_ppa] == taskidx){
                //printf("calibration : %d(%d), %d(%d)\n",
                //cur_ppa, metadata->state[cur_ppa/PPB],
                //rsvlist_head->head->idx*PPB,metadata->state[rsvlist_head->head->idx]);
                //printf("before calib : %f, ",calib_read_lat);
                calib_read_lat = calib_readlatency(metadata,taskidx,calib_read_lat,cur_ppa,(rsvlist_head->head->idx)*PPB);
                //printf("calib result : %f\n",calib_read_lat);
            }
        }
        ru = cur_read_lat * task[taskidx].rn / task[taskidx].rp;
        //calculate expected write util change
        wu = __calc_wu(&(task[taskidx]),metadata->state[cur->idx]);
        //calculate expected gc util, considering GC skip.
        //gcskipfactor = new_rc / task[taskidx].wn;
        gcskipfactor = 1;
        //printf("[%d]rc : %d, wnum : %d, skipfactor : %d\n",cur->idx,new_rc,task[taskidx].wn,gcskipfactor);
        gcu = gc_util / (float)gcskipfactor;
        //check if current util value is optimal
        if (ru+wu+gcu <= cur_best_util){
            cur_best_util = ru+wu+gcu;
            best_idx = cur->idx;
        }
        cur = cur->next;
    }
    //EDGE CASE HANDLING!!
    if(best_idx == -1){
        cur = full_head->head;
        best_invalid = metadata->invnum[cur->idx];
        while(cur != NULL){
            if(metadata->invnum[cur->idx] >= best_invalid){
                best_idx = cur->idx;
                best_invalid = metadata->invnum[cur->idx];
            }
            cur = cur->next;
        }
    }
    return best_idx;
}


int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head){
    
    //sort the victim blocks in order of utilization
    //allocate victim block to GC task proportionally with GC period
    struct timeval a;
    struct timeval b;

    int gc_period_sort[tasknum];                    //array to store period of GC
    int task_order[tasknum];                        //array to store order of task
    int vic_arr[full_head->blocknum];               //array to store GC candidate indexes.
    float gc_util_arr[full_head->blocknum];         //array to store gc-related utilization (gcutil+blocking)
    int vic_num = 0;                                //number of victim
    int proportion_sum = 0;                         //allocated proportion for task
    int proportion_tot = 0;                         //sum of gc period
    int old = get_blockstate_meta(metadata,OLD);    //oldest block for system
    int min_p =  _find_min_period(task,tasknum);    //minimum I/O period of taskset (necessary to calc blocking)
    int cur_state, copyblock_state, new_rc;         //gc util calc params
    float gc_exec, gc_period, gc_util;              //gc util calc params
    int temp, cur_priority;                         //param for sorting & param for proportion check
    float cur_offset;                               //proportion of current task
    int cur_offset_int;                             //proportion of current task in victim block list
    int best_invalid = 0;                           //edge case handling param
    int best_idx = -1;                              //return value

    //init arrays
    for(int i=0;i<full_head->blocknum;i++){
        vic_arr[i] = -1;
    }
    for(int i=0;i<tasknum;i++){
        gc_period_sort[i] = task[i].gcp;
        task_order[i] = i;
        proportion_tot += gc_period_sort[i];
    }

    //sort gc period
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

    //find priority of cur gc
    for(int i=0;i<tasknum;i++){
        if(task_order[i] == taskidx){
            cur_priority = i;
        }
    }
    //find start offset of cur gc
    for(int i=0;i<cur_priority-1;i++){
        proportion_sum += gc_period_sort[i];
    }
    
    cur_offset = (float)proportion_sum / (float)proportion_tot;
    
    //build up candidate block list
    block* cur = NULL;
    cur = full_head->head;
    gettimeofday(&a,NULL);
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        copyblock_state = metadata->state[rsvlist_head->head->idx];
        new_rc = metadata->invnum[cur->idx];
        gc_exec = (PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        //restriction 1. util
        if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == -1){
        //if(0){
            cur = cur->next;
            continue;
        }

        //restriction 2. MINRC
        if(metadata->invnum[cur->idx] < MINRC){
            cur = cur->next;
            continue;
        }
        //add a blocking utilization, since GC has a chance to change it.
        if(metadata->state[cur->idx] == old){
            gc_util += e_exec(old+1) / (float)min_p;
        } 
        else{
            gc_util += e_exec(old) / (float)min_p;
        }
        //insert util & block into candidate block list.
        gc_util_arr[vic_num] = gc_util;
        vic_arr[vic_num] = cur->idx;
        vic_num++;

        //printf("[UGC]add block %d(%d,%d,%f),vicnum : %d\n",cur->idx,cur_state,new_rc,gc_util,vic_num);
        cur = cur->next;
    }
    gettimeofday(&b,NULL);
    //printf("[gcsafe]%d\n",b.tv_sec * 1000000 + b.tv_usec - a.tv_sec * 1000000 - a.tv_usec);
    //EDGE CASE HANDLING : if vic_num is 0, ignore find_gc_safe and add victims.
    if(vic_num == 0){
        cur = full_head->head;
        while(cur != NULL){
            if(metadata->invnum[cur->idx] < MINRC){
                cur = cur->next;
                continue;
            }
            cur_state = metadata->state[cur->idx];
            copyblock_state = metadata->state[rsvlist_head->head->idx];
            new_rc = metadata->invnum[cur->idx];
            gc_exec = (PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
            gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
            gc_util = gc_exec/gc_period;
            //add a blocking utilization, since GC has a chance to change it.
            if(metadata->state[cur->idx] == old){
                gc_util += e_exec(old+1) / (float)min_p;
            } 
            else{
                gc_util += e_exec(old) / (float)min_p;
            }
            gc_util_arr[vic_num] = gc_util;
            vic_arr[vic_num] = cur->idx;
            vic_num++;
            cur = cur->next;
        }
    }
    //!EDGE CASE HANDLING
    gettimeofday(&a,NULL);
    //sort candidate block list
    for(int i=vic_num-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(gc_util_arr[j] > gc_util_arr[j+1]){
                temp = gc_util_arr[j];
                gc_util_arr[j] = gc_util_arr[j+1];
                gc_util_arr[j+1] = temp;
                temp = vic_arr[j];
                vic_arr[j] = vic_arr[j+1];
                vic_arr[j+1] = temp;
            }
        }
    }
    gettimeofday(&b,NULL);
    
    //using offset factor, choose best block
    cur_offset_int = (int)(cur_offset * (float)vic_num);
  
    best_idx = vic_arr[cur_offset_int];
    
    /*
    //edge case handling(deprecated)
    if(best_idx == -1){
        cur = full_head->head;
        best_invalid = metadata->invnum[cur->idx];
        while(cur != NULL){
            if(metadata->invnum[cur->idx] >= best_invalid){
                best_idx = cur->idx;
                best_invalid = metadata->invnum[cur->idx];
            }
            cur = cur->next;
        }
    }
    */
    return best_idx;
}