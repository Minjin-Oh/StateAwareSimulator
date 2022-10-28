#include "findW.h"

extern double OP;
extern int MINRC;

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
    int best_idx = -1;
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
    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
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

int find_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas, int write_start_idx){
    //use expected read utilization & GC utilization to caculate value for each flash block

    //params
    int cur_state;
    float ru, gcu, weighted_r, weighted_gc;
    block* cur;
    float cur_best_util = 1.0;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);

    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        weighted_r = calc_weightedread(task,metadata,cur,taskidx,lpas);
        weighted_gc = calc_weightedgc(task,metadata,cur,taskidx,lpas,write_start_idx,OP);
        ru = (float)task[taskidx].rn * weighted_r / (float)task[taskidx].rp;
        gcu = weighted_gc / (float)task[taskidx].gcp;
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        } else {
            printf("block %d is passed, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        weighted_r = calc_weightedread(task,metadata,cur,taskidx,lpas);
        weighted_gc = calc_weightedgc(task,metadata,cur,taskidx,lpas,write_start_idx,OP);
        ru = task[taskidx].rn * weighted_r / task[taskidx].rp;
        gcu = weighted_gc / task[taskidx].gcp;
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        } else {
            printf("block %d is passed, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        }
        cur = cur->next;
    }//!writelist search end

    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_write_taskfixed(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head){
    //params
    block* cur;
    int best_state;
    int cur_state;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);
    
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        //choose a block if block state is younger (task 0,1) || state is older(task2)
        if(taskidx == 0 || taskidx == 1){
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (taskidx == 2){
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        //choose a block if state is younger (task 0,1) || state is older(task2)
        if(taskidx == 0 || taskidx == 1){
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (taskidx == 2){
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        //printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_write_hotness(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int lpa){
    block* cur;
    int best_state,  cur_state;
    int avg_w_cnt;
    int avg_r_cnt;
    int hotness = -1;
    int best_idx = -1;
    int logi_pg = (int)((1.0-OP)*(float)NOP);
    int old = get_blockstate_meta(metadata,OLD);
    avg_w_cnt = (int)((float)metadata->tot_write_cnt / (float)(logi_pg));
    avg_r_cnt = (int)((float)metadata->tot_read_cnt / (float)(logi_pg));
    
    //distinguish hotness first
    if(metadata->read_cnt[lpa] >= avg_r_cnt || metadata->write_cnt[lpa] >= avg_w_cnt){    
        hotness = 1;
    }
    //init cur if possible
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        if(hotness == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else {
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }

        cur = cur->next;
    }//!fblist search end

    //init cur if possible
    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        if(hotness == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else {
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
   
    return best_idx;
}

int find_write_hotness_motiv(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int lpa, int policy){
    block* cur;
    int best_state,  cur_state;
    int avg_w_cnt;
    int avg_r_cnt;
    int hotness = -1;
    int best_idx = -1;
    int logi_pg = (int)((1.0-OP)*(float)NOP);
    int old = get_blockstate_meta(metadata,OLD);
    avg_w_cnt = (int)((float)metadata->tot_write_cnt / (float)(logi_pg));
    avg_r_cnt = (int)((float)metadata->tot_read_cnt / (float)(logi_pg));
    
    //distinguish hotness first
    if(metadata->read_cnt[lpa] >= avg_r_cnt || metadata->write_cnt[lpa] >= avg_w_cnt){    
        hotness = 1;
    }
    //init cur if possible
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        if(policy == 8){
            if(hotness == 1){
                //find youngest block possible
                if(metadata->state[cur->idx] < best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] >= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 9){
            if(hotness == 1){
                //find oldest block possible
                if(metadata->state[cur->idx] > best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] <= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 10){
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (policy == 11){
            if(metadata->state[cur->idx] <= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!fblist search end

    //init cur if possible
    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state))== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        if(policy == 8){
            if(hotness == 1){
                //find youngest block possible
                if(metadata->state[cur->idx] < best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] >= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 9){
            if(hotness == 1){
                //find oldest block possible
                if(metadata->state[cur->idx] > best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] <= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 10){
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (policy == 11){
            if(metadata->state[cur->idx] <= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
   
    return best_idx;
}