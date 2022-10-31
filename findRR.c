#include "stateaware.h"

extern int rrflag;

void find_RR_target(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2){
    int access_avg = 0;
    int cycle_avg = 0;
    int high = -1;
    int low = -1;
    int cur_high = -1;
    int cur_low = -1;
    int temp_high[NOB];
    int temp_low[NOB];
    int highcnt=0;
    int lowcnt=0;
    int taskhighcnt = 0;
    int tasklowcnt = 0;
    float temp, highest;
    int hightask_idx;


    for(int i=0;i<NOB;i++){
        access_avg += metadata->access_window[i];
        cycle_avg += metadata->state[i];
    }
    access_avg = access_avg/NOB;
    cycle_avg = cycle_avg/NOB;

    //specify a read-intensive task
    highest = 0.0;
    for(int i=0;i<tasknum;i++){
        temp = __calc_ru(&(tasks[i]),0);
        if(temp >= highest){
            highest = temp;
            hightask_idx = i;
        }
    }

    //generate temporary block list w.r.t (A.hotness, B.state) 
    highcnt = 0;
    lowcnt = 0;
    
    if(highcnt == 0 || lowcnt == 0){
        highcnt = 0;
        lowcnt = 0;
        for(int i=0;i<NOB;i++){
            if(metadata->state[i] > cycle_avg && is_idx_in_list(full_head,i)){
                //printf("[HI]%d, state : %d\n",i,metadata->state[i]);
                temp_high[highcnt] = i;
                highcnt++;
            }
            else if(metadata->state[i] <= cycle_avg && is_idx_in_list(full_head,i)){
                //printf("[LO]%d, state : %d\n",i,metadata->state[i]);
                temp_low[lowcnt] = i;
                lowcnt++;
            }
        }
    }
    
    //find the oldest/youngest(h/c) block in hot/cold block
    for(int i=0;i<highcnt;i++){
        //printf("[HI]%d(cnt:%d,cyc:%d)\n",temp_high[i],metadata->access_window[temp_high[i]],metadata->state[temp_high[i]]);
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
        //printf("[LO]%d(cnt:%d,cyc:%d)\n",temp_low[i],metadata->access_window[temp_low[i]],metadata->state[temp_low[i]]);
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
    //printf("[WL]vic1 %d(cnt:%d)(cyc:%d)\n",high,metadata->access_window[high],metadata->state[high]);
    //printf("[WL]vic2 %d(cnt:%d)(cyc:%d)\n",low,metadata->access_window[low],metadata->state[low]);
    //return high/low value.
    
    //free
    *res1 = high;
    *res2 = low;
}


void find_RR_target_util(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2){
    int access_avg = 0;
    int cycle_avg = 0;

    int temp_high[NOB];
    int temp_low[NOB];
    int** block_vmap;

    int highcnt=0;
    int lowcnt=0;

    //init
    block_vmap = (int**)malloc(sizeof(int*)*tasknum); 
    for(int i=0;i<tasknum;i++){
        block_vmap[i]=(int*)malloc(sizeof(int)*NOB);
    }
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<NOB;j++){
            block_vmap[i][j]=0;
        }
    }
    for(int i=0;i<NOB;i++){
        access_avg += metadata->access_window[i];
        cycle_avg += metadata->state[i];
    }
    access_avg = access_avg/NOB;
    cycle_avg = cycle_avg/NOB;

    //build a block valid map
    for(int i=0;i<NOP;i++){
        if(metadata->vmap_task[i]!=-1){
            int tidx = metadata->vmap_task[i];
            if(tidx >= 0){
                block_vmap[tidx][i/PPB]=1;
            }
        }
    }

    //generate temporary block list w.r.t (A.hotness, B.state)
    for(int i=0;i<NOB;i++){
        if(metadata->state[i] > cycle_avg  && is_idx_in_list(full_head,i)){
            //printf("[HI][RRC]%d, state : %d\n",i,metadata->state[i]);
            temp_high[highcnt] = i;
            highcnt++;
        }
        else if(metadata->state[i] <= cycle_avg && is_idx_in_list(full_head,i)){
            //printf("[LO][RRC]%d, state : %d\n",i,metadata->state[i]);
            temp_low[lowcnt] = i;
            lowcnt++;
        }
    }
    int high_cyc, low_cyc, high_idx, low_idx, acc_diff;
    float benefit, loss, best;
    int vic1 = -1, vic2 = -1;
    best = 0.0;
    acc_diff = 0;
    //searching through all combination, check if read has benefit.
    for(int i=0;i<highcnt;i++){
        for(int j=0;j<lowcnt; j++){
            high_cyc = metadata->state[temp_high[i]];
            low_cyc = metadata->state[temp_low[j]];
            high_idx = temp_high[i];
            low_idx = temp_low[j];
            benefit = 0.0;
            loss = 0.0;
            for(int k=0;k<tasknum;k++){
                //calc benefit
                if(block_vmap[k][high_idx] == 1){
                    benefit += __calc_ru(&(tasks[k]),high_cyc) - __calc_ru(&(tasks[k]),low_cyc);
                }
                //calc loss
                if(block_vmap[k][low_idx] == 1){
                    loss += __calc_ru(&(tasks[k]),high_cyc) - __calc_ru(&(tasks[k]),low_cyc);
                }
            }
            //if benefit is better than current best, update best.
            if (benefit-loss >= best){
                if(metadata->access_window[high_idx] - metadata->access_window[low_idx] >= acc_diff){
                    best = benefit-loss;
                    acc_diff = metadata->access_window[high_idx] - metadata->access_window[low_idx];
                    vic1 = high_idx;
                    vic2 = low_idx;
                }
            }
        }
    }
    
    //printf("[WL]vic1 %d(cnt:%d)(cyc:%d)\n",vic1,metadata->access_window[vic1],metadata->state[vic1]);
    //printf("[WL]vic2 %d(cnt:%d)(cyc:%d)\n",vic2,metadata->access_window[vic2],metadata->state[vic2]);
    //return high/low value.
    
    //free
    for(int i=0;i<tasknum;i++){
        free(block_vmap[i]);
    }
    free(block_vmap);
    *res1 = vic1;
    *res2 = vic2;
}

long find_RR_period(int v1, int v2, int vp1_cnt, int vp2_cnt, double rrutil, meta* metadata){
    long rexec, rexec2, wexec, wexec2, eexec, eexec2, tot_exec, period;
    if(rrutil <= 0.0){
        period = __LONG_MAX__;
        return period;
    } else{ 
        rexec = (long)(floor((double)r_exec(metadata->state[v1])));
        rexec2 = (long)(floor((double)r_exec(metadata->state[v2])));
        wexec = (long)(floor((double)w_exec(metadata->state[v1])));
        wexec2 = (long)(floor((double)w_exec(metadata->state[v2])));
        eexec = (long)(floor((double)e_exec(metadata->state[v1])));
        eexec2 = (long)(floor((double)e_exec(metadata->state[v2])));
        tot_exec = (long)vp2_cnt*(rexec2+wexec) + (long)vp1_cnt*(rexec+wexec2)+eexec+eexec2;
        period = (long)ceil((double)tot_exec / (double)rrutil);
    }
    return period;
}