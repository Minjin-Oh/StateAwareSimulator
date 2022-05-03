#include "stateaware.h"

extern int rrflag;

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
    
    printf("[WL]vic1 %d(cnt:%d)(cyc:%d)\n",vic1,metadata->access_window[vic1],metadata->state[vic1]);
    printf("[WL]vic2 %d(cnt:%d)(cyc:%d)\n",vic2,metadata->access_window[vic2],metadata->state[vic2]);
    //return high/low value.
    
    //free
    for(int i=0;i<tasknum;i++){
        free(block_vmap[i]);
    }
    free(block_vmap);
    *res1 = vic1;
    *res2 = vic2;
}