#include "findGC.h"
#include <float.h>

// [FIXED-LATENCY] All exec / util helpers in this file are on the *decision*
// path (GC victim selection). They call *_exec_dec / __calc_*_dec /
// find_util_safe_dec / find_cur_util_dec so that under latency_mode == 1|2
// LaWL sees a fixed latency criterion. Under latency_mode == 0 the _dec
// helpers transparently fall through to the original state-aware versions.

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

static void _build_gc_read_collision_delta(rttask* tasks, int tasknum, meta* metadata, int rsv_b_state, float* read_delta_by_victim){
    for(int i=0;i<NOB;i++){
        read_delta_by_victim[i] = 0.0f;
    }
    const float r_exec_rsv_b = r_exec_dec(rsv_b_state);
    for (int i=0;i<tasknum;i++){
        if(rq[i] == NULL){
            continue;
        }

        const float inv_rp = 1.0f / (float)tasks[i].rp;
        IO* cur = rq[i]->head;
        while (cur != NULL){
            int lpa = cur->lpa;
            int ppa = metadata->pagemap[lpa];

            if (ppa >= 0 && metadata->invmap[ppa] == 0 && metadata->rmap[ppa] == lpa){
                int victim_b = ppa / PPB;
                int read_b_state = metadata->state[victim_b];

                if (read_b_state < rsv_b_state){
                    read_delta_by_victim[victim_b] += (r_exec_rsv_b - r_exec_dec(read_b_state)) * inv_rp;
                }
            }

            cur = cur->next;
        }
    }
}

int _find_gc_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util, int cur_b, int rsv_b){
    //check if current I/O job does not violate util test along with recently released other jobs.

    //init variables
    IO* cur;
    int read_b;
    int valid_cnt = 0;
    float total_u = 0.0;
    float old_total_u = 0.0;
    float wutils[tasknum];
    float rutils[tasknum];
    float gcutils[tasknum];

    int lpas[PPB];

    //allocate current runtime utils on local variable
    for(int i=0;i<tasknum;i++){
        wutils[i] = metadata->runutils[0][i];
        rutils[i] = metadata->runutils[1][i];
        gcutils[i] = metadata->runutils[2][i];
    }

    //get victim block valid lpas.
    // invmap: 1 (invalid), 0 (valid) || rmap: -1 (invalid), valid lpa (valid)
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[cur_b*PPB+i]==0 && metadata->rmap[cur_b*PPB+i]!=-1){
            lpas[valid_cnt] = metadata->rmap[cur_b*PPB+i];
            valid_cnt++;
        }
    }

    int rsv_b_state = metadata->state[rsv_b];
    float r_exec_rsv_b = r_exec_dec(rsv_b_state);

    if(valid_cnt > 0){
        for (int i=0;i<tasknum;i++){
            cur = rq[i]->head;
            if(cur == NULL) continue;

            float read_b_state_cached_exec = -1.0;
            int queue_check_cnt = 0;
            const int MAX_READ_QUEUE_CHECKS = 16;

            while (cur != NULL && queue_check_cnt < MAX_READ_QUEUE_CHECKS){
                for(int j=0;j<valid_cnt;j++){
                    if(cur->lpa == lpas[j]){
                        read_b = metadata->pagemap[lpas[j]]/PPB;
                        int read_b_state = metadata->state[read_b];
                        if(read_b_state < rsv_b_state){
                            if(read_b_state_cached_exec < 0.0){
                                read_b_state_cached_exec = (float)r_exec_dec(read_b_state);
                            }
                            rutils[i] -= read_b_state_cached_exec / (float)tasks[i].rp;
                            rutils[i] += r_exec_rsv_b / (float)tasks[i].rp;
                        }
                    }
                    break;
                }
                cur = cur->next;
                queue_check_cnt++;
            }
        }
    }

    for (int j=0;j<tasknum;j++){
        total_u += wutils[j];
        total_u += rutils[j];
        total_u += gcutils[j];
    }

    static int cached_min_period = -1;
    if (cached_min_period == -1) {
        cached_min_period = _find_min_period(tasks, tasknum);
    }

    total_u += (float)e_exec_dec(old) / (float)cached_min_period;
    total_u -= gcutils[taskidx];
    total_u += util;

    return (total_u <= 1.0) ? 0 : -1;
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
            util_profile[i] += __calc_ru_dec(&(task[taskidx]),metadata->cur_read_worst[taskidx]);
        } else {
            util_profile[i] += __calc_ru_dec(&(task[taskidx]),i);
        }
        util_profile[i] += __calc_wu_dec(&(task[taskidx]),i);
    }

    float gc_runutil = 0.0;
    float profile_util = 0.0;
    while(cur != NULL){
        //if GCing current block is impossible, select another one
        
        if(metadata->invnum[cur->idx] >= MINRC){
            cur_state = metadata->state[cur->idx];
            new_rc = metadata->invnum[cur->idx];
            float profile_util = 0.0;
            float gc_exec = (PPB-new_rc)*(w_exec_dec(yng)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
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
                cur_minutil = __calc_gcu_dec(&(task[taskidx]),MINRC,yng,metadata->state[cur->idx],metadata->state[cur->idx]);
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
            gc_exec = (PPB-new_rc)*(w_exec_dec(yng)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
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
    float slack = 1.0 - find_cur_util_dec(task,tasknum,metadata,get_blockstate_meta(metadata,OLD)) + metadata->runutils[2][taskidx];
    //printf("slack : %f, gcutil : %f, curutil : %f\n",slack,metadata->runutils[2][taskidx],find_cur_util_dec(task,tasknum,metadata,get_blockstate_meta(metadata,OLD)));
    int expected_idx = -1;
    while(cur != NULL){
        if(metadata->invnum[cur->idx] >= MINRC){
            new_rc = metadata->invnum[cur->idx];
            cur_state = metadata->state[cur->idx];
            gc_exec = (PPB-new_rc)*(w_exec_dec(yng)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
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
        gc_exec = (PPB-new_rc)*(w_exec_dec(copyblock_state)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        if(find_util_safe_dec(task,tasknum,metadata,old,taskidx,GC,gc_util ) == -1){
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
        wu = __calc_wu_dec(&(task[taskidx]),metadata->state[cur->idx]);
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
        gc_exec = (PPB-new_rc)*(w_exec_dec(copyblock_state)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
        gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
        gc_util = gc_exec/gc_period;
        if(find_util_safe_dec(task,tasknum,metadata,old,taskidx,GC,gc_util ) == -1){
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
        wu = __calc_wu_dec(&(task[taskidx]),metadata->state[cur->idx]);
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

block* find_gc_utilsort(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head, bhead* rsvlist_head, bhead* write_head){
    int old = get_blockstate_meta(metadata,OLD);    
    int min_p = _find_min_period(task,tasknum);     
    
    int copyblock_state = metadata->state[rsvlist_head->head->idx];
    float gc_period = (float)_gc_period(&(task[taskidx]), (int)(MINRC));
    const float blocking_old = e_exec_dec(old) / (float)min_p;
    const float blocking_old_next = e_exec_dec(old+1) / (float)min_p;
    float w_exec_copy = w_exec_dec(copyblock_state);

    block* best_in_bucket[PPB + 1];
    for (int i = 0; i <= PPB; i++) best_in_bucket[i] = NULL;

    block* cur = full_head->head;
    block* best_relaxed_blk = NULL;
    int best_relaxed_inv = -1;

    while(cur != NULL){
        int inv = metadata->invnum[cur->idx];
        if(inv > PPB) inv = PPB;

	if(inv > best_relaxed_inv){
            best_relaxed_inv = inv;
            best_relaxed_blk = cur;
        }

	if(best_in_bucket[inv] == NULL || metadata->state[cur->idx] < metadata->state[best_in_bucket[inv]->idx]){
            best_in_bucket[inv] = cur;
        }
        cur = cur->next;
    }

    block* best_fallback_blk = NULL;

    for(int inv = PPB; inv >= MINRC; inv--){
        if(best_in_bucket[inv] != NULL){
            block* candidate = best_in_bucket[inv];
            int cur_state = metadata->state[candidate->idx];

            float gc_exec = (float)(PPB - inv) * (w_exec_copy + r_exec_dec(cur_state)) + e_exec_dec(cur_state);
            float gc_util = gc_exec / gc_period;
            gc_util += (cur_state == old) ? blocking_old_next : blocking_old;

	    if(best_fallback_blk == NULL) {
                best_fallback_blk = candidate;
            }

	    if(_find_gc_safe(task, tasknum, metadata, old, taskidx, GC, gc_util, candidate->idx, rsvlist_head->head->idx) == 0){
                return candidate;
            }
        }
    }

    // EDGE CASE HANDLING
    if (best_fallback_blk != NULL){
        printf("[GC] Safe block not found. Using MINRC fallback.\n");
        return best_fallback_blk;
    } else {
        printf("[GC] No block satisfies MINRC=%d, relax MINRC with invnum=%d\n", MINRC, best_relaxed_inv);
        return best_relaxed_blk;
    }
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
        float gc_exec = (float)(PPB-new_rc)*(w_exec_dec(copyblock_state)+r_exec_dec(cur_state))+e_exec_dec(cur_state);
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

// [WAO-GC] Zhang et al. 2015, "Optimizing Deterministic Garbage Collection in
// NAND Flash Storage Systems", DATE '15.
//
// Victim selection rule (Sec. IV-C, wear-leveler paragraph):
//   1) Pick the block with the *fewest valid pages* (greedy). Combined with
//      OP-based over-provisioning this guarantees valid_count <= U(lambda) =
//      ceil(sigma * pi), which upper-bounds the copy work per GC.
//   2) When multiple blocks tie on valid_count, prefer the one with the
//      *lowest P/E cycle*. This is the embedded wear-leveler -- it distributes
//      erases toward less-worn blocks without a separate WL job.
//
// No latency-based scoring here on purpose: the paper reduces GC to a
// combinatorial victim pick + partial-step interleaving. Partial-step
// interleaving is already provided by this simulator's per-page GC request
// queue, so the only thing this function has to do is pick the victim.
block* find_gc_waogc(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head){
    (void)task; (void)taskidx; (void)tasknum;
    block* best = NULL;
    int best_valid = PPB + 1;   // sentinel: any real block beats this
    int best_state = MAXPE + 1;
    for(block* cur = full_head->head; cur != NULL; cur = cur->next){
        int valid = PPB - metadata->invnum[cur->idx];
        int st    = metadata->state[cur->idx];
        if(valid < best_valid ||
          (valid == best_valid && st < best_state)){
            best       = cur;
            best_valid = valid;
            best_state = st;
        }
    }
    return best;
}
