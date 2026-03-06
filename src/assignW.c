/*FIXME:: assign function MUST give integer array pointer "lpas"*/

#include "assignW.h"
#include "findW.h"

extern long cur_cp;
extern FILE **fps;

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
    return cur;
}

block* assign_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, 
                             bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long cur_cp, 
                             FILE* fpovhd_w_assign, FILE* w_assign_detail){
    int target;
    block* cur = NULL;

    target = find_write_maxinvalid(task,taskidx,tasknum,metadata,fblist_head,write_head,w_lpas,idx,cur_cp, w_assign_detail);
    cur = ll_findidx(write_head,target);

    return cur;
}