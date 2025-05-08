#include "stateaware.h"

extern int rrflag;

void _build_hc_fromlist(bhead* list, meta* metadata, bhead* hotlist, bhead* coldlist,int max){
    block* cur = list->head;
    while(cur != NULL){
        block* new = (block*)malloc(sizeof(block));
        memcpy(new,cur,sizeof(block));
        if(metadata->state[cur->idx] >= max-MARGIN)
            ll_append(hotlist,new);
        else
            ll_append(coldlist,new);
        cur = cur->next;
    }
}

void build_hot_cold(meta* metadata, bhead* hotlist, bhead* coldlist){
    //this function is called when P/E diff reaches threshold,
    // but hot cold sep is not done yet

    //build hot-cold-list with new blocklist. note that blockinfo should be copied.
    // X worry about fpnum. only index will be considered.
    int tot_ec = 0;
    float avg_ec;
    for(int i=0;i<NOB;i++){
        tot_ec += metadata->state[i];
    }
    avg_ec = (float)tot_ec / (float)NOB;
    for(int i=0;i<NOB;i++){
        block* new = (block*)malloc(sizeof(block));
        new->idx = i;
        new->next = NULL;
        new->prev = NULL;
        if((float)metadata->state[i]>=(float)avg_ec)
            ll_append(hotlist,new);
        else   
            ll_append(coldlist,new);
    }
    /*
    printf("checking. ");
    */
}

void swap(int* a, int* b){
    int temp = *b;
    *b = *a;
    *a = temp;
}

int* _sort_bystate(meta* metadata, bhead* list, int order){
    block* cur = list->head;
    int size = list->blocknum;
    int i, j, temp;
    int* res = (int*)malloc(sizeof(int)*size);
    for(i=0;i<size;i++){
        res[i]=cur->idx;
        cur = cur->next;
    }
    for(i=size-1;i>0;i--){
        for(j=0;j<i;j++){
            if(order == 0){
                if(metadata->state[res[j]] < metadata->state[res[j+1]]){//big is first
                    int temp = res[j];
                    res[j] = res[j+1];
                    res[j+1] = temp;
                }
            } else if (order == 1){
                if(metadata->state[res[j]] > metadata->state[res[j+1]]){//small is first
                    int temp = res[j];
                    res[j] = res[j+1];
                    res[j+1] = temp;
                }
            }
        }
    }
    //printf("[SORT:%d]state(idx): ",order);
    //for(int i=0;i<size;i++) printf("%d(%d) ",metadata->state[res[i]],res[i]);
    //printf("\n");
    return res;
}

int get_blkidx_byage(meta* metadata, bhead* list, 
                     bhead* full_head, 
                     int param, int any){
    //gives out block index based on age metadata.
    //edge case : when the most worn-out block is not fb or full
    //sort the block in order and find block in fb/full
    block *cur = list->head;
    int res_idx;
    int *sorted_idx = _sort_bystate(metadata,list,param);
    if(any==0){//if any == 0, skip blocks which are rsv or write area.
        for(int i=0;i<list->blocknum;i++){
            if(is_idx_in_list(full_head,sorted_idx[i])==1){
                   res_idx = sorted_idx[i];
                   free(sorted_idx);
                   return res_idx;
            }
            //printf("!!!%d in somewhere else!!!\n",sorted_idx[i]);
        }
    } else if(any==1){//if any == 1, simply return first block idx.
        res_idx = sorted_idx[0];
        free(sorted_idx);
        return res_idx;
    }
    printf("\n");
    printf("no feasible target in list\n");
    free(sorted_idx);
    return -1;
}

int get_blockstate_meta(meta* metadata, int param){ // from line 541-542 in emul_main.c -> metadata : newmeta, param : YOUNG(-1)/OLD(0)
    // a function to find youngest/oldset block inside whole system
    int ret_state = metadata->state[0];
    if(param == OLD){
        for(int i=0;i<NOB;i++){
            if (metadata->state[i] > ret_state){
                ret_state = metadata->state[i];
            }
        }
    } else if (param == YOUNG){
        for(int i=0;i<NOB;i++){
            if (metadata->state[i] < ret_state){
                ret_state = metadata->state[i];
            }
        }
    }
    return ret_state;
}
/*
if(rrflag == 1){
        for(int i=0;i<NOB;i++){
            if(metadata->state[i] > cycle_avg && (block_vmap[hightask_idx][i]==1) && is_idx_in_list(full_head,i)){
                //printf("[HI][RRC]%d, state : %d\n",i,metadata->state[i]);
                temp_high[highcnt] = i;
                highcnt++;
            }
            else if(metadata->state[i] <= cycle_avg && (block_vmap[hightask_idx][i]==0) && is_idx_in_list(full_head,i)){
                //printf("[LO][RRC]%d, state : %d\n",i,metadata->state[i]);
                temp_low[lowcnt] = i;
                lowcnt++;
            }
        }
    }
*/
