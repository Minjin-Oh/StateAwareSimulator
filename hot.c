#include "stateaware.h"


void find_RR_target(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2){
    int access_avg = 0;
    int cycle_avg = 0;
    int high = -1;
    int low = -1;
    int cur_high = -1;
    int cur_low = -1;
    
    int temp_high[NOB];
    int temp_low[NOB];
    int task_high[NOB];
    int task_low[NOB];
    int highcnt=0;
    int lowcnt=0;
    int taskhighcnt = 0;
    int tasklowcnt = 0;
    float temp, highest;
    int hightask_idx;

    printf("testing access window : ");
    for(int i=0;i<NOB;i++){
        printf("%d(%d), ",i,metadata->access_window[i]);
        access_avg += metadata->access_window[i];
        cycle_avg += metadata->state[i];
    }
    printf("\n");
    access_avg = access_avg/NOB;
    cycle_avg = cycle_avg/NOB;

    //generate temporary block list w.r.t (A.hotness, B.state)
    for(int i=0;i<NOB;i++){
        if(metadata->state[i] > cycle_avg){
            temp_high[highcnt] = i;
            highcnt++;
        }
        else{
            temp_low[lowcnt] = i;
            lowcnt++;
        }
    }
    //specify a read-intensive task
    highest = 0.0;
    for(int i=0;i<tasknum;i++){
        temp = __calc_ru(&(tasks[i]),0);
        if(temp >= highest){
            highest = temp;
            hightask_idx = i;
        }
    }
    //if we can specify the block with read-intensive data, choose among them.
    for(int i=0;i<highcnt;i++){
        if(metadata->access_tracker[hightask_idx][temp_high[i]]==1){
            task_high[taskhighcnt] = temp_high[i];
            taskhighcnt++;
        }
    }
    for(int i=0;i<lowcnt;i++){
        if(metadata->access_tracker[hightask_idx][temp_low[i]]==0){
            task_low[tasklowcnt] = temp_low[i];
            tasklowcnt++;
        }
    }
    //if there exists a hot&read-intensive-task and cold & no-read-intensive task, swap among them.
    if(taskhighcnt!=0 && tasklowcnt!=0){
        for(int i=0;i<taskhighcnt;i++){
            if(cur_high == -1 || metadata->access_window[task_high[i]]>cur_high){
                if(is_idx_in_list(full_head,task_high[i])){
                    cur_high = metadata->access_window[task_high[i]];
                    high = temp_high[i];
                }
            }
        }
        for(int i=0;i<lowcnt;i++){
            if(cur_low == -1 || metadata->access_window[task_low[i]]<cur_low ){
                if(is_idx_in_list(full_head,temp_low[i])){
                    cur_low = metadata->access_window[temp_low[i]];
                    low = temp_low[i];
                }
            }
        }
        *res1 = high;
        *res2 = low;
        return;
    }
    //find the oldest/youngest(h/c) block in hot/cold block
    for(int i=0;i<highcnt;i++){
        printf("[HI]%d(cnt:%d,cyc:%d)\n",temp_high[i],metadata->access_window[temp_high[i]],metadata->state[temp_high[i]]);
        if(cur_high == -1){
            if(is_idx_in_list(full_head,temp_high[i])){
                cur_high = metadata->access_window[temp_high[i]];
                high = temp_high[i];
                
            }
        }
        else if(metadata->access_window[temp_high[i]] > cur_high){
            if(is_idx_in_list(full_head,temp_high[i])){
                cur_high = metadata->access_window[temp_high[i]];
                high = temp_high[i];
            }
        }
        else{
            /*do nothing*/
        }
    }
    for(int i=0;i<lowcnt;i++){
         printf("[LO]%d(cnt:%d,cyc:%d)\n",temp_low[i],metadata->access_window[temp_low[i]],metadata->state[temp_low[i]]);
        if(cur_low == -1){
            if(is_idx_in_list(full_head,temp_low[i])){
                cur_low = metadata->access_window[temp_low[i]];
                low = temp_low[i];
                
            }
        }
        else if(metadata->access_window[temp_low[i]] < cur_low){
            if(is_idx_in_list(full_head,temp_low[i])){
                cur_low = metadata->access_window[temp_low[i]];
                low = temp_low[i];
            }
        } 
        else{
            /*do nothing*/
        }
    }
    printf("[WL]vic1 %d(cnt:%d)(cyc:%d)\n",high,metadata->access_window[high],metadata->state[high]);
    printf("[WL]vic2 %d(cnt:%d)(cyc:%d)\n",low,metadata->access_window[low],metadata->state[low]);
    //return high/low value.
    *res1 = high;
    *res2 = low;
}


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

void build_hot_cold(meta* metadata, bhead* hotlist, bhead* coldlist, int max){
    //this function is called when P/E diff reaches threshold,
    // but hot cold sep is not done yet

    //build hot-cold-list with new blocklist. note that blockinfo should be copied.
    // X worry about fpnum. only index will be considered.
    for(int i=0;i<NOB;i++){
        block* new = (block*)malloc(sizeof(block));
        new->idx = i;
        new->next = NULL;
        new->prev = NULL;
        if(metadata->state[i]>=max-MARGIN)
            ll_append(hotlist,new);
        else   
            ll_append(coldlist,new);
    }
    printf("checking. ");
    block* cur = hotlist->head;
    while(cur!=NULL){
        printf("%d(%d) ",metadata->state[cur->idx],cur->idx);
        cur = cur->next;
    }
    printf(" vs ");
    cur = coldlist->head;
    while(cur!=NULL){
        printf("%d(%d) ",metadata->state[cur->idx],cur->idx);
        cur = cur->next;
    }
    printf("\n");
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
                if(metadata->state[res[j]]<metadata->state[res[j+1]]){//big is first
                    int temp = res[j];
                    res[j] = res[j+1];
                    res[j+1] = temp;
                }
            } else if (order == 1){
                if(metadata->state[res[j]]>metadata->state[res[j+1]]){//small is first
                    int temp = res[j];
                    res[j] = res[j+1];
                    res[j+1] = temp;
                }
            }
        }
    }
    printf("[SORT:%d]state(idx): ",order);
    for(int i=0;i<size;i++) printf("%d(%d) ",metadata->state[res[i]],res[i]);
    printf("\n");
    return res;
}

int get_blkidx_byage(meta* metadata, bhead* list, 
                     bhead* rsvlist_head, bhead* write_head, 
                     int param, int any){
    //gives out block index based on age metadata.
    //edge case : when the most worn-out block is not fb or full
    //sort the block in order and find block in fb/full
    block *cur = list->head;
    int res_idx;
    int *sorted_idx = _sort_bystate(metadata,list,param);
    if(any==0){//if any == 0, skip blocks which are rsv or write area.
        for(int i=0;i<list->blocknum;i++){
            if((is_idx_in_list(rsvlist_head,sorted_idx[i])==0) &&
               (is_idx_in_list(write_head,sorted_idx[i])==0)){
                   res_idx = sorted_idx[i];
                   free(sorted_idx);
                   return res_idx;
            }
            printf("!!!%d in somewhere else!!!\n",sorted_idx[i]);
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

int get_blockstate_meta(meta* metadata, int param){
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
