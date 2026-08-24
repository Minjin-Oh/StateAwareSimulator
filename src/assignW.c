/*FIXME:: assign function MUST give integer array pointer "lpas"*/

#include "assignW.h"
#include "findW.h"
#include "stdlib.h"

extern long cur_cp;
extern int max_valid_pg;
extern FILE **fps;

extern bhead* glob_yb;
extern bhead* glob_ob;
extern long* lpa_update_timing[NOP];

// Prediction error injection (for accuracy sweep experiments)
// Compile-time controls (can be overridden by -D options)
#ifndef ENABLE_PRED_ERR
// 0: off, 1: on
#define ENABLE_PRED_ERR 0
#endif

#ifndef PRED_ACC
// accuracy in [0,1], e.g., 0.9 means 10% misprediction
#define PRED_ACC 1.0
#endif

static inline double __rand01(void){
    return (double)rand() / (double)RAND_MAX;
}

// pick a random block idx from a list (write_head)
static int __pick_random_idx_from_list(bhead* head){
    if (head==NULL || head->blocknum <= 0 || head->head == NULL) return -1;
    int r = rand() % head->blocknum;
    block* cur = head->head;
    while (cur != NULL && r>0){
        cur = cur->next;
        r--;
    }
    return (cur != NULL) ? cur->idx : -1;
}

block* assign_write_dynwl(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    int youngest_fb_idx = -1;
    int youngest_wb_idx = -1;
    block* ret = NULL;
    //1. search through free block list / write block list
    if(fblist_head->blocknum > 0){
        youngest_fb_idx = find_block_in_list(metadata,fblist_head,YOUNG);
    }
    if(write_head->blocknum > 0){
        youngest_wb_idx = find_block_in_list(metadata,write_head,YOUNG);
    }

    //2. choose youngest block in fblist or wblist.
    if(youngest_fb_idx != -1 && youngest_wb_idx != -1){
        //both list has at least 1 component.
        if(metadata->state[youngest_fb_idx] < metadata->state[youngest_wb_idx]){ // free block의 P/E cycle이 더 적은 경우, 새로운 block 할당
            ret = ll_remove(fblist_head,youngest_fb_idx); 
            ll_append(write_head,ret);
            return ret;
        } else { // write block의 P/E cycle이 더 적은 경우, 기존 write block list에서 꺼내서 씀
            ret = ll_find(metadata,write_head,YOUNG);
            return ret;
        }
    }
    else if (youngest_fb_idx != -1 && youngest_wb_idx == -1){ // write block list에 할당된 블록이 없음 (free block에서 할당)
        //no write block, so we must get new free block.
        ret = ll_remove(fblist_head,youngest_fb_idx);
        ll_append(write_head,ret);
        return ret;
    }
    else if (youngest_fb_idx == -1 && youngest_wb_idx != -1){ // free block list에 여유 블록이 없음 (write block에 있는 블록 사용)
        //no free block, so we must use current write block.
        ret = ll_find(metadata,write_head,YOUNG);
        return ret;
    }
    else if (youngest_fb_idx == -1 && youngest_wb_idx == -1){ // free block list, write block list 모두 사용 가능한 블록이 없음 (즉, full block)
        //full??
        printf("ssd full\n");
        abort();
    }

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
    //print_writeblock_profile(fps[tasknum+taskidx],cur_cp,metadata,fblist_head,write_head,-1,cur->idx,-1,-1,-1.0,cur->idx,-1);
    return cur;
}

block* assign_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, 
                             bhead* fblist_head, bhead* write_head, block* cur_b, int* w_lpas, int idx, long cur_cp){
    //struct timeval a;
    //struct timeval b;
    //int sec, usec;
    int target;
    block* cur = NULL;
    
    //gettimeofday(&a,NULL);
    target = find_write_maxinvalid(task,taskidx,tasknum,metadata,fblist_head,write_head,w_lpas,idx,cur_cp);
    #if ENABLE_PRED_ERR
        // with probability (1-PRED_ACC), override the "correct" target with a random WB target
        if(__rand01() > PRED_ACC){
            int alt = __pick_random_idx_from_list(write_head);
            if(alt != -1){
                target = alt;
            }
        }
    #endif

    cur = ll_findidx(write_head,target);
    //gettimeofday(&b,NULL);
    //sec = (b.tv_sec - a.tv_sec)*1000000;
    //usec = (b.tv_usec - a.tv_usec);
    //printf("[assignovhd]:%d\n",sec+usec);
    return cur;
}
