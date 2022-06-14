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
    float cur_gcctrl = 0.0;
    float cur_minutil = 1.0;
    float cur_gc,gc_exec,gc_period;
    float util_profile[MAXPE];

    //update read_worst
    update_read_worst(metadata,tasknum);
    
    //find worst util given old/yng value
    cur_wcutil = __calc_gcu(&(task[taskidx]),MINRC,yng,old,old);
    float prev_gc_exec = (PPB-MINRC)*(w_exec(yng)+r_exec(old))+e_exec(old);
    float prev_gc_period = _gc_period(&(task[taskidx]),MINRC);
    
    //scan the metadata and find a block with lowest utilization
    for(int i=0;i<MAXPE;i++){
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
            float gc_period = _gc_period(&(task[taskidx]),(int)(new_rc));
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
        printf("[ctrl]expected target %d, inv : %d, util : %f, state : %d\n",expected_idx,cur_invalid,cur_minutil,metadata->state[expected_idx]);
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

int find_gcctrl_yng(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* full_head){
    //find "the block" which is suitable for GC, considering utilization
    //!!!returns the block number, not limit
    //find the value for youngest/oldest block
    block* cur = full_head->head;
    int expected_idx = -1;
    int bstate = MAXPE;

    while(cur != NULL){
        //if GCing current block is impossible, select another one
        int cur_state = metadata->state[cur->idx];
        if(metadata->invnum[cur->idx] >= MINRC){
            if(bstate >= cur_state){
                expected_idx = cur->idx;
            }
        }
        cur = cur->next;
    }
    printf("expected target %d, inv : %d, state : %d\n",expected_idx,metadata->invnum[cur->idx],metadata->state[expected_idx]);
    return expected_idx;
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
    //if(cur_read_worst != old){sleep(1);}
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
        cur_state = metadata->state[cur->idx];
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
        cur_state = metadata->state[cur->idx];
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