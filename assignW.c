#include "assignW.h"
#include "findW.h"
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
            printf("target block : %d\n",target);
        }
    } else { //initial case :: cur_b == NULL. find new block
        cur = ll_condremove(metadata,fblist_head,YOUNG);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = write_head->head;
        }
        target = cur->idx;
        printf("[INIT]target block : %d\n",target);
    }//if state is different, get another write block
    return cur;
}

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    //printf("fbnum : %d, writenum : %d\n",fblist_head->blocknum,write_head->blocknum);
    block* cur = NULL;
    if(cur_b != NULL){
        if(cur_b->fpnum > 0){
            //printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            cur = ll_pop(fblist_head);
            if (cur != NULL){
                ll_append(write_head,cur);
            } else {
                cur = write_head->head;
            }
            target = cur->idx;
            //printf("target block : %d\n",target);
        }
    } else { //initial case :: cur_b == NULL. find new block
        cur = ll_pop(fblist_head);
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
                         bhead* fblist_head, bhead* write_head, block* cur_b, int lpa){
    int target;
    block* cur = NULL;
    target = find_write_hotness(task,taskidx,tasknum,metadata,fblist_head,write_head,lpa);
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
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d\n",target);
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