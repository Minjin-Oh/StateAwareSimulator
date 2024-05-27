#include "stateaware.h"

extern int THRES_COLD;
extern int THRES_HOT;
extern int prev_erase;
extern int prev_mincyc;
extern int prev_cyc[NOB];
void find_RR_dualpool(rttask* task, int tasknum, meta* metadata, bhead* full_head, bhead* hotlist, bhead* coldlist, int* res1, int* res2){
    
    //get relocation target
    int hot_old = get_blkidx_byage(metadata,hotlist,full_head,0,0);
    int hot_young = get_blkidx_byage(metadata,hotlist,full_head,1,0);
    int cold_young = get_blkidx_byage(metadata,coldlist,full_head,1,0);
    int cold_max_eec, hot_min_eec, cold_evict_idx;
    //printf("hotold : %d(%d), coldyoung : %d(%d)\n",hot_old,metadata->state[hot_old],cold_young,metadata->state[cold_young]);
    
    if (cold_young == -1 || hot_old == -1){
        *res1 = -1;
        *res2 = -1;
        return;
    }
    if(metadata->state[hot_old] - metadata->state[cold_young] < THRESHOLD){
        *res1 = -1;
        *res2 = -1;
        return;
    }
    //update hot-cold pool
    block* hot_b = ll_remove(hotlist,hot_old);
    block* cold_b = ll_remove(coldlist,cold_young);
    ll_append(hotlist,cold_b);
    ll_append(coldlist,hot_b);
    metadata->EEC[hot_old] = 0;
    metadata->EEC[cold_young] = 0;

    //adjust hot-cold pool if necessary
    //hot pool eviction code
    if(metadata->state[hot_old] - metadata->state[hot_young] >  2*THRESHOLD){
        block* hot_evict = ll_remove(hotlist,hot_young);
        ll_append(coldlist,hot_evict);
    }

    //cold pool eviction code
    block* cur = coldlist->head;
    cold_max_eec = metadata->EEC[cur->idx];
    cold_evict_idx = cur->idx;
    while(cur != NULL){
        if(cold_max_eec < metadata->EEC[cur->idx]){
            cold_max_eec = metadata->EEC[cur->idx];
            cold_evict_idx = cur->idx;
        }
        cur = cur->next;
    }
    cur = hotlist->head;
    hot_min_eec = metadata->EEC[cur->idx];
    while(cur != NULL){
        if(hot_min_eec > metadata->EEC[cur->idx]){
            hot_min_eec = metadata->EEC[cur->idx];
        }
        cur = cur->next;
    }
    
    if(cold_max_eec - hot_min_eec > THRESHOLD){
        block* cold_evict = ll_remove(coldlist,cold_evict_idx);
        ll_append(hotlist,cold_evict);
    }
    //return value
    
    *res1 = hot_old;
    *res2 = cold_young;
    return;
}
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
    int temp_highcnt=0;
    int temp_lowcnt=0;
    int high_cyc, low_cyc, high_idx, low_idx, acc_diff;
    float benefit, loss, best;
    int vic1 = -1, vic2 = -1;

    block* cur;
    struct timeval a;
    struct timeval b;
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
    cur = full_head->head;
    while (cur != NULL){
        if(metadata->state[cur->idx] > cycle_avg){
            temp_high[highcnt] = cur->idx;
            highcnt++;
        } else if(metadata->state[cur->idx] <= cycle_avg){
            temp_low[lowcnt] = cur->idx;
            lowcnt++;
        }
        cur = cur->next;
    }
    /*
    //old blocklist seperation logic (DEPRECATED)
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
    printf("hghcnt:%d, lowcnt:%d\n",highcnt,lowcnt);
    */
    best = 0.0;
    acc_diff = 0;
    //searching through all combination, check if read has benefit.
    for(int i=0;i<highcnt;i++){
        for(int j=0;j<lowcnt; j++){
            if(metadata->access_window[i] - metadata->access_window[j] < 100){
                continue;
            }
            if(metadata->state[i] - metadata->state[j] < THRESHOLD){
                continue;
            }
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
    //return high/low value.
    if(metadata->access_window[vic1] - metadata->access_window[vic2] < 100){
        vic1 = -1;
        vic2 = -1;
    }
    if (metadata->state[vic1] - metadata->state[vic2] < THRESHOLD){
        vic1 = -1;
        vic2 = -1;
    }
    //free
    for(int i=0;i<tasknum;i++){
        free(block_vmap[i]);
    }
    free(block_vmap);
    *res1 = vic1;
    *res2 = vic2;
}

void find_RR_target_simple(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2){
    block* cur;
    block* ho = NULL;
    block* cy = NULL;
    int access_avg = 0;
    //find threshold for access_count.
    for(int i=0;i<NOB;i++){
        access_avg += metadata->access_window[i];
        
    }
    access_avg = access_avg/NOB;
    //search through flash blocks & find hot-old, cold-yng
    cur = full_head->head;
    while (cur != NULL){
        //find hot-old flash blocks(read count >> avg && oldest)
        if(metadata->access_window[cur->idx] > access_avg+100){
            if (ho == NULL){
                ho = cur;
            } 
            else if (metadata->state[ho->idx] < metadata->state[cur->idx]){
                ho = cur;
            }
        }
        //find cold-yng flash blocks(read count << avg && youngest)
        else if (metadata->access_window[cur->idx] < access_avg-100){
            if (cy == NULL){
                cy = cur;
            }
            else if (metadata->state[cy->idx] > metadata->state[cur->idx]){
                cy = cur;
            }
        }
        cur = cur->next;
    }
    //edge case handling
    if(ho == NULL || cy == NULL){
        *res1 = -1;
        *res2 = -1;
        return;
    } 
    else if(metadata->state[ho->idx] - metadata->state[cy->idx] < THRESHOLD){
        *res1 = -1;
        *res2 = -1;
        return;
    }
    //return found block
    *res1 = ho->idx;
    *res2 = cy->idx;
    return;
}

void find_WR_target_simple(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res1, int* res2){
    block* cur;
    block* a = NULL;
    block* b = NULL;
    int cyc_avg = 0;
    int cur_erase = 0;
    int cur_mincyc = 0;
    int erase_diff_max = -1;
    int erase_diff_min = MAXPE;
    int temp_thres_cold = -1;
    int temp_thres_hot = -1;
    for(int i=0;i<NOB;i++){
        cyc_avg += metadata->state[i];
        cur_erase += metadata->state[i];
    }
    cyc_avg = cyc_avg / NOB;
    
    //systematically determine THRES_COLD and THRES_HOT
    if(cur_erase/(int)WR_CYC_INTERVAL != prev_erase){
        prev_erase = cur_erase / (int)WR_CYC_INTERVAL;
        for(int i=0;i<NOB;i++){
            if(metadata->state[i]<= cyc_avg){
                if(metadata->state[i] - prev_cyc[i] < erase_diff_min){
                    //printf("cur state : %d, prev state : %d\n",metadata->state[i],prev_cyc[i]);
                    //printf("thres_cold = %d\n",metadata->invalidation_window[i]);
                    erase_diff_min = metadata->state[i] - prev_cyc[i];
                    temp_thres_cold = metadata->invalidation_window[i];
                
                }   
                else if (metadata->state[i] - prev_cyc[i] == erase_diff_min){
                    if(temp_thres_cold < metadata->invalidation_window[i]){
                        temp_thres_cold = metadata->invalidation_window[i];
                        //printf("state : %d==%d,temp_thres_cold : %d\n",metadata->state[i],prev_cyc[i],metadata->invalidation_window[i]);
                    }
                }
            }
            if(metadata->state[i]>cyc_avg){
                if (metadata->state[i] - prev_cyc[i] > erase_diff_max){
                    erase_diff_max = metadata->state[i] - prev_cyc[i];
                    temp_thres_hot = metadata->invalidation_window[i];
                }
                else if (metadata->state[i] - prev_cyc[i] == erase_diff_max){
                    if(temp_thres_hot > metadata->invalidation_window[i]){
                        temp_thres_hot = metadata->invalidation_window[i];
                    }
                }
            }
            prev_cyc[i] = metadata->state[i];
        }
        //printf("erase_diff_min : %d, erase_diff_max : %d\n",erase_diff_min,erase_diff_max);
        //printf("curthres : %d, %d\n",temp_thres_cold,temp_thres_hot);
    }
    /*
    //hand-picked thres_values + dynamic adjustment 
    if(cur_erase / (int)WR_CYC_INTERVAL != prev_erase){
        prev_erase = cur_erase / (int)WR_CYC_INTERVAL;
        //redetermine THRES_COLD 
        cur_mincyc = get_blockstate_meta(metadata,YOUNG);
        if (cur_mincyc == prev_mincyc){
            THRES_COLD += 5;
        }
        else{
            prev_mincyc = cur_mincyc;
        }
    }
    temp_thres_cold = THRES_COLD;
    temp_thres_hot = 300;
    */
    THRES_COLD = temp_thres_cold;
    THRES_HOT = temp_thres_hot;
    
    cur = full_head->head;
    while (cur != NULL){
        //find hot-old flash blocks(read count >> avg && oldest)
        if(metadata->invalidation_window[cur->idx] > THRES_HOT){
            if (a == NULL){
                a = cur;
            } 
            else if (metadata->state[a->idx] < metadata->state[cur->idx]){
                a = cur;
            }
        }
        //find cold-yng flash blocks(read count << avg && youngest)
        else if (metadata->invalidation_window[cur->idx] < THRES_COLD){
            if (b == NULL){
                b = cur;
            }
            else if (metadata->state[b->idx] > metadata->state[cur->idx]){
                b = cur;
            }
        }
        cur = cur->next;
    }
    //edge case handling
    if(a == NULL || b == NULL){
        *res1 = -1;
        *res2 = -1;
        return;
    } 
    else if(metadata->state[a->idx] - metadata->state[b->idx] < THRESHOLD){
        *res1 = -1;
        *res2 = -1;
        return;
    }
    //return found block
    //printf("[WR]found block. [%d]state:%d, [%d]state:%d\n",a->idx,metadata->state[a->idx],b->idx,metadata->state[b->idx]);
    *res1 = a->idx;
    *res2 = b->idx;
    return;
}
void find_BWR_victim_updatetiming(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, int* res){
    block* cur;
    cur = full_head->head;
    long nu[PPB];       //array which contains next update timings
    long diff, diff_temp;
    double avg, var, max_b_var, max_b_avg;
    int ppa, vnum, lpa, res_b_idx, res_p_idx;
    block* res_b_ptr;
    max_b_var = 0.0;
    max_b_avg = 0.0;
    res_b_idx = -1;
    res_b_ptr = NULL;

    //search through full blocks, finding a worst allocation block
    while(cur != NULL){
        avg = 0.0;
        var = 0.0;
        ppa = 0;
        vnum = 0;
        lpa = 0;
        for(int i=0;i<PPB;i++){
            nu[i] = 0L;
        }
        //find average & variance of update timing of valid pages
        for(int i=0;i<PPB;i++){
            ppa = cur->idx * PPB + i;
            if(metadata->invmap[ppa] == 0){
                //valid
                lpa = metadata->rmap[ppa];
                nu[vnum] = metadata->next_update[lpa];
                avg += (double)nu[vnum];
                vnum++;
            } 
            else{
                //do nothing.
            }
        }
        if (vnum == 0){
            //no valid pg in block
            cur = cur->next;
            continue;
        }
        avg = avg / (double)vnum;
        for(int i=0;i<vnum;i++){
            var += pow((double)nu[i] - avg,2.0);
        }
        var = var / (double)vnum;
        //compare the variance with previous worst block
        if(max_b_var > var){
            max_b_var = var;
            max_b_avg = avg;
            res_b_idx = cur->idx;
            res_b_ptr = cur;
        }
    }

    //edge case handling
    if(res_b_ptr == NULL){
        //none of the blocks have valid page
        *res = -1;
        return;
    }

    //find the worst-allocated page in worst-allocated block
    diff = 0L;
    for(int i=0;i<PPB;i++){
        diff_temp = 0L;
        ppa = res_b_ptr->idx * PPB + i;
        if(metadata->invmap[ppa] == 0){
            lpa = metadata->rmap[ppa];
            diff_temp = labs(metadata->next_update[lpa] - (long)max_b_avg);
            if(diff_temp >= diff){
                res_p_idx = ppa;
                diff = diff_temp;
            }
        }
    }
    *res = res_p_idx;
    return;
}

void find_BWR_target_updatetiming(long update_timing, meta* metadata, int tasknum, bhead* write_head, bhead* fblist_head, int* res){
    int ranknum = metadata->ranknum;
    int num_per_rank[ranknum];
    int target_rank = -1;
    double avg_per_rank[ranknum];
    double sdv_per_rank[ranknum];
    for(int i=0;i<tasknum;i++){
        num_per_rank[i] = 0;
        avg_per_rank[i] = 0.0;
        sdv_per_rank[i] = 0.0;
    }
    //get avg per rank
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<metadata->cur_rank_info.tot_ranked_write[i];j++){
            int r = metadata->cur_rank_info.ranks_for_write[i][j];
            avg_per_rank[r] += (double)metadata->cur_rank_info.timings_for_write[i][j];
            num_per_rank[r] += 1;
        }
    }
    for(int i=0;i<ranknum;i++){
        if(num_per_rank[i] != 0)
            avg_per_rank[i] = avg_per_rank[i] / (double)num_per_rank[i];
        else   
            avg_per_rank[i] = -1.0;
    }
    //get stdev per rank
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<metadata->cur_rank_info.tot_ranked_write[i];j++){
            int r = metadata->cur_rank_info.ranks_for_write[i][j];
            double timing = (double)metadata->cur_rank_info.timings_for_write[i][j];
            sdv_per_rank[r] += pow(fabs(avg_per_rank[r] - timing), 2.0);
        }
    }
    for(int i=0;i<ranknum;i++){
        if(num_per_rank[i] != 0)
            sdv_per_rank[i] = pow(sdv_per_rank[i],0.5);
        else   
            sdv_per_rank[i] = 0.0;
    }
    //check if current victim can be placed in rank
    for(int i=0;i<ranknum;i++){
        if((double)update_timing >= avg_per_rank[i]-sdv_per_rank[i] && (double)update_timing <= avg_per_rank[i]+sdv_per_rank[i]){
            target_rank = i;
            break;
        }
        
    }
    //get corresponding block
    block* cur = write_head->head;
    while(cur != NULL){
        if(cur->wb_rank == target_rank){
            *res = (cur->idx+1)*PPB - cur->fpnum;
            return;
        }
    }
    if(cur==NULL){
        *res = 1;
        return;
    }
    if(target_rank == -1){
        *res = 1;
        return;
    }

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

