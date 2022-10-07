#include "findGC.h"

extern double OP;
extern int MINRC;

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
    
    printf("expected target %d, inv : %d, util : %f, state : %d\n",expected_idx,cur_invalid,cur_minutil,metadata->state[expected_idx]);
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
    printf("expected target %d, inv : %d, state : %d\n",expected_idx,cur_invalid,metadata->state[expected_idx]);
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
    printf("slack : %f, gcutil : %f, curutil : %f\n",slack,metadata->runutils[2][taskidx],find_cur_util(task,tasknum,metadata,get_blockstate_meta(metadata,OLD)));
    int expected_idx = -1;
    while(cur != NULL){
        if(metadata->invnum[cur->idx]>= MINRC){
            new_rc = metadata->invnum[cur->idx];
            cur_state = metadata->state[cur->idx];
            gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            gc_period = (float)_gc_period(&(task[taskidx]),(int)(MINRC));
            gc_util = gc_exec/gc_period;
            if(gc_util <= slack){
                if(metadata->state[cur->idx] <= cur_min_state){
                    cur_min_state = metadata->state[cur->idx];
                    expected_idx = cur->idx;
                }    
            }
        }
        cur = cur->next;
    }
    printf("expected target %d, inv : %d, state : %d\n",expected_idx,metadata->invnum[expected_idx],metadata->state[expected_idx]);
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
        printf("[edge]expected target %d, inv : %d, state : %d\n",expected_idx,metadata->invnum[expected_idx],metadata->state[expected_idx]);
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
        if(find_util_safe(task,tasknum,metadata,old,taskidx,GC,gc_util) == -1){
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
int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head){
    //find a optimized value for worst case utilization
    
    //init params
    int yng, old, ret, iter;
    float cur_worst, cur_write, new_write, margin;
    int cyc[NOB];
    float rw_util[NOB];
    yng = get_blockstate_meta(metadata,YOUNG);
    old = get_blockstate_meta(metadata,OLD);
    cur_worst = find_worst_util(task,tasknum,metadata);
    iter = 0;
    //find a worst read block for task
    //FIXME:: we can add worst-block for task in metadata array.
    int cur_read_worst = 0;
    for(int i=0;i<NOP;i++){
        if(metadata->vmap_task[i] == taskidx){
            int cur_b = i/PPB;
            if(metadata->state[cur_b] >= cur_read_worst){
                cur_read_worst = metadata->state[cur_b];
            }
        }
    }
    printf("task read worst : %d, system worst : %d\n",cur_read_worst,old);

    //draw a write-read profile according to cur_read_worst
    //printf("state(util):");
    for(int i=yng;i<=old;i++){
        cyc[iter] = i;
        if(i<cur_read_worst){
            rw_util[iter] = __calc_wu(&(task[taskidx]),i) + __calc_ru(&(task[taskidx]),cur_read_worst) + __calc_gcu(&task[taskidx],MINRC,yng,i,i);
        }
        else if (i >= cur_read_worst){
            rw_util[iter] = __calc_wu(&(task[taskidx]),i) + __calc_ru(&(task[taskidx]),i) + __calc_gcu(&task[taskidx],MINRC,yng,i,i);;
        }
        //printf("%d(%f) ",i,rw_util[iter]);
        iter++;   
    }
    printf("\n");
    //find a suitable block for best utilization
    block* cur = fblist_head->head;
    float cur_util = -1.0;
    int best_idx;
    int cur_state;
    //printf("cur bnum : %d\n",fblist_head->blocknum);
    while(cur != NULL){
        //check if the block is OK for write
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state)) == -1){
            cur = cur->next;
            continue;
        }
        //if block is OK, test of it is optimal block
        //printf("[F]candidate : %d, %d, %d, %f\n",cur->idx,cur_state,cur->fpnum,rw_util[cur_state-yng]);
        if(cur_util == -1.0 || cur_util >= rw_util[cur_state-yng]){
            cur_util = rw_util[cur_state-yng];
            best_idx = cur->idx;
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    //printf("cur bnum : %d\n",write_head->blocknum);
    while(cur != NULL){
        //check if the block is OK for write
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(task,cur_state)) == -1){
            cur = cur->next;
            continue;
        }
        //printf("[W]candidate : %d, %d, %d, %f\n",cur->idx,cur_state,cur->fpnum,rw_util[cur_state-yng]);
        if(cur_util == -1.0 || cur_util >= rw_util[cur_state-yng]){
            cur_util = rw_util[cur_state-yng];
            best_idx = cur->idx;
        }
        cur = cur->next;
    }//!writelist search end
    printf("best block : %d, state : %d, util : %f\n",best_idx,metadata->state[best_idx],cur_util);
    return best_idx;
}

int find_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas){
    //use expected read utilization & GC utilization to caculate value for each flash block

    //params
    int cur_state;
    float ru, gcu;
    block* cur;
    float cur_best_util = 1.0;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);
    float cur_read_lat = calc_readlatency(task, metadata, taskidx);
    
    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        
        //check if current block is OK for write operation w.r.t util restriction
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        //calc new expected read util : assume invalidated pages are all moved to new block
        for(int i=0;i<task[taskidx].wn;i++){
            //printf("given lpa : %d", lpas[i]);
            cur_read_lat = calib_readlatency(metadata,taskidx,cur_read_lat,metadata->pagemap[lpas[i]],cur->idx*PPB);
        }
        ru = cur_read_lat * (float)task[taskidx].rn / (float)task[taskidx].rp;
        
        //calc expected gc util : assume that target block joined candidate pool with minimum invalids.
        gcu = __calc_gcu(&(task[taskidx]),MINRC,0,cur_state,cur_state);
        //printf("block %d, ru+gcu : %f\n",cur->idx,ru+gcu);
        //summate read util + GC util. compare with optimal block.
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated\n",cur->idx);
        } else {
            printf("block %d is passed\n",cur->idx);
        }
        //iterate through all blocks and check thier values.
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        
        //check if current block is OK for write operation w.r.t util restriction
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        //calc new expected read util : assume invalidated pages are all moved to new block
        for(int i=0;i<task[taskidx].wn;i++){
            cur_read_lat = calib_readlatency(metadata,taskidx,cur_read_lat,metadata->rmap[lpas[i]],cur->idx*PPB);
        }
        ru = cur_read_lat * (float)task[taskidx].rn / (float)task[taskidx].rp;
        
        //calc expected gc util : assume that target block joined candidate pool with minimum invalids.
        gcu = __calc_gcu(&(task[taskidx]),MINRC,0,cur_state,cur_state);
        printf("block %d, ru+gcu : %f\n",cur->idx,ru+gcu);
        //summate read util + GC util. compare with optimal block.
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated\n",cur->idx);
        } else {
            printf("block %d is passed\n",cur->idx);
        }
        //iterate through all blocks and check thier values.
        cur = cur->next;
    }//!writelist search end

    if(best_idx == -1){
        cur = write_head->head;
        if(cur->fpnum == 0){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}