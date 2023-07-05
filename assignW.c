/*FIXME:: assign function MUST give integer array pointer "lpas"*/

#include "assignW.h"
#include "findW.h"

extern long cur_cp;
extern int max_valid_pg;
extern FILE **fps;

extern block** b_glob_young;
extern block** b_glob_old;
extern bhead* glob_yb;
extern bhead* glob_ob;
extern long* lpa_update_timing[NOP];

block* assign_write_greedy(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    if(cur_b != NULL){
        if(cur_b->fpnum > 0){
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            cur = ll_condremove(metadata,fblist_head,YOUNG);
            if (cur != NULL){
                ll_append(write_head,cur);
            } else {
                cur = write_head->head;
            }
            target = cur->idx;
            //printf("target block : %d\n",target);
        }
    } else { //initial case :: cur_b == NULL. find new block
        cur = ll_condremove(metadata,fblist_head,YOUNG);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = write_head->head;
        }
        target = cur->idx;
        //printf("[INIT]target block : %d\n",target);
    }//if state is different, get another write block
    return cur;
}

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    //printf("fbnum : %d, writenum : %d\n",fblist_head->blocknum,write_head->blocknum);
    block* cur = NULL;
    cur = write_head->head;
    if (cur == NULL){
        cur = ll_pop(fblist_head);
        if (cur != NULL){
            ll_append(write_head,cur);
        }
    }
    //if state is different, get another write block
    print_writeblock_profile(fps[tasknum+taskidx],cur_cp,metadata,fblist_head,write_head,-1,cur->idx,-1,-1,-1.0,cur->idx,-1);
    return cur;
}

block* assign_write_ctrl(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    target = find_writectrl(task,taskidx,tasknum,metadata,fblist_head,write_head);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d, fp : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* lpas){
    int target;
    block* cur = NULL;
    target = find_writelimit(task,taskidx,tasknum,metadata,fblist_head,write_head,lpas);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d, fp : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* lpas, int write_start_idx){
    int target;
    block* cur = NULL;
    target = find_writeweighted(task,taskidx,tasknum,metadata,fblist_head,write_head,lpas,write_start_idx);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d, fp : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_writefixed(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    target = find_write_taskfixed(task,taskidx,tasknum,metadata,fblist_head,write_head);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d, fp : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_writehotness(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas,int idx){
    int target;
    block* cur = NULL;
    target = find_write_hotness(task,taskidx,tasknum,metadata,fblist_head,write_head,w_lpas,idx);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            //printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            //printf("target block : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                //printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        //printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_write_old(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    if(cur_b != NULL){
        if(cur_b->fpnum > 0){
            //printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            cur = ll_condremove(metadata,fblist_head,OLD);
            if (cur != NULL){
                ll_append(write_head,cur);
            } else {
                cur = write_head->head;
            }
            target = cur->idx;
            //printf("target block : %d\n",target);
        }
    } else { //initial case :: cur_b == NULL. find new block
        cur = ll_condremove(metadata,fblist_head,OLD);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = write_head->head;
        }
        target = cur->idx;
        //printf("[INIT]target block : %d\n",target);
    }//if state is different, get another write block
    return cur;
}

block* assign_writehot_motiv(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b, int lpa, int policy){
    int target;
    block* cur = NULL;
    target = find_write_hotness_motiv(task,taskidx,tasknum,metadata,fblist_head,write_head,lpa,policy);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            //printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            //printf("target block : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                //printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, 
                             bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx, int flag){
    int target;
    block* cur = NULL;
    target = find_write_gradient(task,taskidx,tasknum,metadata,fblist_head,write_head,w_lpas,idx,flag);
    //printf("fbnum,writenum:%d,%d\n",fblist_head->blocknum,write_head->blocknum);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            //printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            //printf("target block : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                //printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        //printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_write_invalid(rttask* task, int taskidx, int tasknum, meta* metadata,
                            bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx){
    
    int target;
    int cur_state;
    //params for update rate check
    int lpa = w_lpas[idx];
    int num = 0;
    char old_flag = -1;
    char young_flag = -1;
    long avg_update_rate = 0;
    //params for candidate block list
    int old = get_blockstate_meta(metadata,OLD);
    int candidate_arr[write_head->blocknum+fblist_head->blocknum];
    int* cand_state_arr[write_head->blocknum+fblist_head->blocknum];
    int candidate_num = 0;

    //check if current lpa is update-hot or not.
    for(int i=0;i<max_valid_pg;i++){
        if(metadata->avg_update[i] != 0){
            avg_update_rate += metadata->avg_update[i];
            num++;
        }
    }    
    if(num == 0){//init state edgecase
        avg_update_rate = 0;
    } else {
        avg_update_rate = avg_update_rate / (long)num;
    }
    //edgecase:: if lpa is never updated before, go to young
    if(metadata->recent_update[lpa] == 0 && metadata->avg_update[lpa] == 0){
        young_flag = 1;
    }
    //if update rate is short, go to young, else, go to old.
    if(metadata->avg_update[lpa] <= avg_update_rate){
        young_flag = 1;
    } else if (metadata->avg_update[lpa] > avg_update_rate){
        old_flag = 1;
    }//!check if current lpa is update-hot or not


    //find out target block
    //update write block status if necessary
    block* cur = NULL;
    block* ret = NULL;
    //search for current write block
    //!!!make sure that, when write block is removed, it MUST BE REMOVED FROM GLOBAL BLOCK LIST
    if(young_flag == 1){   
        cur = glob_yb->head;
        while(cur != NULL){
            cur_state = metadata->state[cur->idx];
            if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == 0){
                ret = cur;
                return ret;
            }
            cur = cur->next;
        }
    }
    else if(old_flag == 1){
        cur = glob_ob->head;
        while(cur != NULL){
            cur_state = metadata->state[cur->idx];
            if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == 0){
                ret = cur;
                return ret;
            }
            cur = cur->next;
        }
    }
    //if write block is not specified in global array, get one from free block list
    if(ret == NULL){
        printf("checking fb. num : %d\n",fblist_head->blocknum);
        cur = fblist_head->head;
        if(young_flag == 1){        
            while(cur != NULL){
                //skip check if current block is not suitable
                cur_state = metadata->state[cur->idx];
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
                    cur = cur->next;
                    continue;
                }
                //if current free block is younger, change b_glob_young[taskidx].
                if(ret == NULL){
                    ret = cur;
                } 
                else if (metadata->state[cur->idx] < metadata->state[ret->idx]){
                    ret = cur;
                }
                cur = cur->next;
            }
            if(ret != NULL){
                ll_remove(fblist_head,ret->idx);
                ll_append(glob_yb,ret);
                printf("[Y]retrieve %d from fblist\n",ret->idx);
                return ret;
            }
        }
        else if(old_flag == 1){        
            while(cur != NULL){
                //skip check if current block is not suitable
                cur_state = metadata->state[cur->idx];
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
                    cur = cur->next;
                    continue;
                }
                //if current free block is older, change b_glob_old[taskidx].
                if(ret == NULL){
                    ret = cur;
                } 
                else if (metadata->state[cur->idx] > metadata->state[ret->idx]){
                    ret = cur;
                }
                cur = cur->next;
            }
            if(ret != NULL){
                ll_remove(fblist_head,ret->idx);
                ll_append(glob_ob,ret);
                printf("[O]retrieve %d from fblist\n",ret->idx);
                return ret;
            }
        }   
    }
    //edgecase::ret is still NULL?
    if(glob_yb->head != NULL){
        return glob_yb->head;
    } 
    else if (glob_ob->head != NULL){
        return glob_ob->head;
    }
    else if (fblist_head->head != NULL){
        ret = fblist_head->head;
        ll_remove(fblist_head,ret->idx);
        if(old_flag == 1){
            ll_append(glob_ob,ret);
        }
        else if(young_flag == 1){
            ll_append(glob_yb,ret);
        }
        return ret;
    }
}

block* assign_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, 
                             bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx, long workload_reset_time){
    int target;
    block* cur = NULL;
    target = find_write_maxinvalid(task,taskidx,tasknum,metadata,fblist_head,write_head,w_lpas,idx,workload_reset_time);
    cur = ll_findidx(write_head,target);
    return cur;
}