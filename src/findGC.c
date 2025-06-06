#include "findGC.h"

extern double OP;
extern int MINRC;

//FIXME:: a temporary solution to expose queue to find_util_safe function.
extern IOhead** wq;
extern IOhead** rq;
extern IOhead** gcq;
extern FILE* test_gc_writeblock[4];
extern long cur_cp;

//FIXME:: a temporary solution to expose 
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


int find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head){
    //!!!find_gc_ has floating point issues
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
    int cur_priority;                               //param for sorting & param for proportion check
    float cur_offset;                               //proportion of current task
    int cur_offset_int;                             //proportion of current task in victim block list
    int best_invalid = 0;                           //edge case handling param
    int best_idx = -1;                              //return value
    float temp;
    float cur_min_util = 2.0f;                     //temp variable to store minimum util(GC+blocking)

#ifdef utilsort_writecheck
    //arrays to test write block array
    int test_arr[full_head->blocknum + write_head->blocknum];
    float test_gcutil_arr[full_head->blocknum + write_head->blocknum];
    char block_origin[full_head->blocknum + write_head->blocknum];
#endif
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
        gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        //printf("gc_util, gc_exec : %f, %f\n",gc_util,gc_exec);
        //restriction 1. util
        if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == -1){
            cur = cur->next;
            continue;
        }
        //restriction 2. MINRC
        if(metadata->invnum[cur->idx] < MINRC){
            cur = cur->next;
            continue;
        }
        //add a blocking utilization, since GC has a chance to change it.
        //tweak:: as erase execution time becomes step function, approximate blocking factor
        if(metadata->state[cur->idx] == old){
            //gc_util += e_exec(old+1) / (float)min_p;
            //printf("blocking : %f / %f = %f\n",((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE),(float)min_p,((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE) / (float)min_p);
            gc_util += ((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE) / (float)min_p;
        }
        else{
            //gc_util += e_exec(old) / (float)min_p;
            //printf("blocking : %f / %f = %f\n",((float)(ENDE-STARTE)/(float)MAXPE*(float)(old+1) + (float)STARTE),(float)min_p,((float)(ENDE-STARTE)/(float)MAXPE*(float)(old) + (float)STARTE) / (float)min_p);
            gc_util += ((float)(ENDE-STARTE)/(float)MAXPE*(float)(old) + (float)STARTE) / (float)min_p;
        }
        //insert util & block into candidate block list.
        gc_util_arr[vic_num] = gc_util;
        vic_arr[vic_num] = cur->idx;
#ifdef utilsort_writecheck
        //testcode:: insert util & block into test block list.
        test_gcutil_arr[vic_num] = gc_util;
        test_arr[vic_num] = cur->idx;
        block_origin[vic_num] = 0;
#endif
        vic_num++;
        cur = cur->next;
    }
#ifdef utilsort_writecheck
    //testcode:: search the write block list
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
        //printf("gc_util, gc_exec : %f, %f\n",gc_util,gc_exec);
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
        //testcode:: insert util & block into test block list.
        test_gcutil_arr[test_vicnum] = gc_util;
        test_arr[test_vicnum] = cur->idx;
        block_origin[test_vicnum] = 1;
        test_vicnum++;
        //printf("[UGC]add block %d(%d,%d,%f),vicnum : %d\n",cur->idx,cur_state,new_rc,gc_util,vic_num);
        cur = cur->next;
    }
#endif
    gettimeofday(&b,NULL);
    //printf("[gcsafe]%d\n",b.tv_sec * 1000000 + b.tv_usec - a.tv_sec * 1000000 - a.tv_usec);
    //EDGE CASE HANDLING : if vic_num is 0, ignore find_gc_safe and add victims.
    if(vic_num == 0){
        printf("vic_num == 0\n");
        cur = full_head->head;
        while(cur != NULL){
            if(metadata->invnum[cur->idx] < MINRC){
                cur = cur->next;
                continue;
            }
            cur_state = metadata->state[cur->idx];
            copyblock_state = metadata->state[rsvlist_head->head->idx];
            new_rc = metadata->invnum[cur->idx];
            gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
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
    //currently, sorting is negligible to greedily minimize GC overhead
    /*
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
    }*/
    for(int i=0;i<vic_num;i++){
        if(gc_util_arr[i] <= cur_min_util){
            cur_min_util = gc_util_arr[i];
            best_idx = vic_arr[i];
        }
    }
    //printf("util:%f\n,best_idx:%d,invnum:%d\n",cur_min_util,best_idx,metadata->invnum[best_idx]);
    gettimeofday(&b,NULL);
#ifdef utilsort_writecheck
    //testcode:: sort test block list
    for(int i=test_vicnum-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(test_gcutil_arr[j] > test_gcutil_arr[j+1]){
                temp = test_gcutil_arr[j];
                test_gcutil_arr[j] = test_gcutil_arr[j+1];
                test_gcutil_arr[j+1] = temp;
                temp = test_arr[j];
                test_arr[j] = test_arr[j+1];
                test_arr[j+1] = temp;
                temp = block_origin[j];
                block_origin[j] = block_origin[j+1];
                block_origin[j+1] = temp;
            }
        }
    }
#endif
    //using offset factor, choose best block
    cur_offset_int = (int)(cur_offset * (float)vic_num);
#ifdef UTILSORT_BEST
    cur_offset_int = 0;
#endif
    //currently, sorting is negligible to greedily minimize GC overhead
    //best_idx = vic_arr[cur_offset_int];
#ifdef utilsort_writecheck
    int test_cur_offset_int = (int)(cur_offset * (float)test_vicnum);
    int test_best_idx = test_arr[test_cur_offset_int];
    fprintf(test_gc_writeblock[taskidx],"%ld, %d, %d, %d, %d, %d,%d\n",
    cur_cp,
    best_idx,metadata->state[best_idx],
    test_best_idx,metadata->state[test_best_idx],
    block_origin[test_cur_offset_int],test_vicnum - vic_num);
#endif
    return best_idx;
}

int find_gc_test(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head){
    int old = get_blockstate_meta(metadata,OLD);    //oldest block for system
    int oldest_vic_invalid = -1;
    int others_vic_invalid = -1;
    int oldest_vic_idx;
    int others_vic_idx;
    block* oldest_vic = NULL;
    block* others_vic = NULL;
    block* cur = full_head->head;
    block* vic = NULL;
    while(cur != NULL){
        //check gc safe
        int cur_state = metadata->state[cur->idx];
        int copyblock_state = metadata->state[rsvlist_head->head->idx];
        int new_rc = metadata->invnum[cur->idx];
        float gc_exec = (float)(PPB-new_rc)*(w_exec(copyblock_state)+r_exec(cur_state))+e_exec(cur_state);
        float gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        float gc_util = gc_exec/gc_period;
        if(_find_gc_safe(task,tasknum,metadata,old,taskidx,GC,gc_util,cur->idx,rsvlist_head->head->idx) == -1){
            cur=cur->next;
            continue;
        }
        //if gc safe passed, check block's age and direct to one of 2 group.
        if(metadata->state[cur->idx] == old){
        //if oldest, compare invnum among oldest
            if(metadata->invnum[cur->idx] > oldest_vic_invalid){
                oldest_vic_invalid = metadata->invnum[cur->idx];
                oldest_vic_idx = cur->idx;
                oldest_vic = cur;
            }
        }
        else{
        //if not oldest, compare invnum among not oldest. choose the youngest+many inv
            if(metadata->invnum[cur->idx] > others_vic_invalid){
                others_vic_invalid = metadata->invnum[cur->idx];
                others_vic_idx = cur->idx;
                others_vic = cur;
            }
        }
        cur=cur->next;
    }
    //compare two candidate. if invnum of b from oldest > invnum of b from not-oldest + X, choose former.
    if(oldest_vic == NULL && others_vic == NULL){//none of blocks meet gc_safe.
        //fall back to baseline GC.
        cur = full_head->head;
        vic = cur;
        while(cur != NULL){
            if(metadata->invnum[vic->idx] < metadata->invnum[cur->idx]){
                vic = cur;
            }
            cur = cur->next;
        }
    }
    else if(oldest_vic == NULL && others_vic != NULL){ //not possible, but if...
        vic = others_vic;
    }
    else if(others_vic == NULL && oldest_vic != NULL){//if all block has same cyc
        vic = oldest_vic;
    }
    else{//if two group exists, choose the block from oldest only when following eq holds.
        printf("inv from oldest = %d, inv from others = %d, oldestP/E = %d\n",oldest_vic_invalid,others_vic_invalid,old);
        if(oldest_vic_invalid > others_vic_invalid + (int)GCGROUP_THRES){ 
            vic = oldest_vic;
        }
        else{
            vic = others_vic;
        }
    }
    return vic->idx;
}

block* find_gc_destination(meta* metadata, int lpa, long workload_reset_time, bhead* fblist_head, bhead* write_head){
    //find a destination of current page, similar to find_maxinvalid function.
    //FIXME:: this prototype function is hardcoded for TIMING_ON_MEM
    long cur_lpa_timing;
    int cnt;
    int cur_lpa_nextupdatenum;
    int target;
    int yield_pg;
    block *wb_new, *cur;

    //find relocated page's update timing & calculate its order.
    cur_lpa_nextupdatenum = metadata->write_cnt_per_cycle[lpa] + 1;
    if(cur_lpa_nextupdatenum >= update_cnt[lpa]){
        cur_lpa_timing = workload_reset_time + WORKLOAD_LENGTH;
    } 
    else {
        cur_lpa_timing = lpa_update_timing[lpa][cur_lpa_nextupdatenum];
    }
    cnt = __calc_invorder_mem(max_valid_pg, metadata, cur_lpa_timing, workload_reset_time, metadata->total_fp);
    
    //edgecase 1:: free page is not enough to handle target lpa
    if(cnt >= metadata->total_fp){
        while(fblist_head->blocknum != 0){
            //search through free block list, finding a youngest block,
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        //give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                return cur;    
            }
            cur = cur->next;
        }
    }

    //normal case. find order within write block list.
    yield_pg = 0;
    cur = write_head->head;
    while(cur != NULL){
        yield_pg += cur->fpnum;
        if(yield_pg > cnt){
            //if current block has enough page to satisfy update order, break from loop
            break;
        } else {
            //search next if not
            cur = cur->next;
        }
    }
    //edgecase 2:: write block list X have corresponding block.
    if(cur==NULL){
        while(fblist_head->blocknum != 0){
            //search through free block list, finding a youngest block, 
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
            yield_pg += wb_new->fpnum;
            if(yield_pg > cnt){
                cur = wb_new;
                break;
            } else {
                //do nothing. 
                //repeat while loop until free block runs out.
            }
            //printf("[FB]yieldpg : %d\n",yield_pg);
        }
        if(cur != NULL){
            return cur;
        }
    }
    //edgecase 3::somehow no corresponding block.
    if(cur==NULL){
        while(fblist_head->blocknum != 0){
            //search through free block list, finding a youngest block, 
            target = find_block_in_list(metadata,fblist_head,YOUNG);
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        //give last write block.
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