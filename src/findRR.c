#include "stateaware.h"

extern int THRES_COLD;
extern int THRES_HOT;
extern int prev_erase;
extern int prev_mincyc;
extern int prev_cyc[NOB];

void find_RR_dualpool(rttask* task, int tasknum, meta* metadata, bhead* full_head, bhead* hotlist, bhead* coldlist, int* res1, int* res2){
    
    //get relocation target
    int hot_old = get_blkidx_byage(metadata,hotlist,full_head,0,0);         // read hot? write hot?, old : high P/E cycle
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

    // prev-ver.
    // while(cur != NULL){
    //     if(cold_max_eec < metadata->EEC[cur->idx]){
    //         cold_max_eec = metadata->EEC[cur->idx];
    //         cold_evict_idx = cur->idx;
    //     }
    //     cur = cur->next;
    // }
    // opt-ver.
    // Single pass to find cold pool max EEC
    for(cur = cur->next; cur != NULL; cur = cur->next){
        int eec = metadata->EEC[cur->idx];
        if(eec > cold_max_eec){
            cold_max_eec = eec;
            cold_evict_idx = cur->idx;
        }
    }

    // hot pool eviction code
    cur = hotlist->head;
    hot_min_eec = metadata->EEC[cur->idx];
    
    // prev-ver.
    // while(cur != NULL){
    //     if(hot_min_eec > metadata->EEC[cur->idx]){
    //         hot_min_eec = metadata->EEC[cur->idx];
    //     }
    //     cur = cur->next;
    // }
    
    // opt-ver. 
    // Only proceed if cold_max_eec is significantly high
    // or if we haven't already found a violation
    for(cur = cur->next; cur != NULL; cur = cur->next){
        int eec = metadata->EEC[cur->idx];
        if(eec < hot_min_eec){
            hot_min_eec = eec;
            // Early termination: if gap is already too large, no need to continue
            if(cold_max_eec - eec > THRESHOLD){
                break;
            }
        }
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
    int hotnum = 0;
    int coldnum = 0;
    for(int i=0;i<NOB;i++){
		cyc_avg += metadata->state[i];      // state = P/E cycle ?
        cur_erase += metadata->state[i];    // total P/E cycle = erase count ?
    }
    cyc_avg = cyc_avg / NOB;
    
    // systematically determine THRES_COLD and THRES_HOT
    if (cur_erase / (int)WR_CYC_INTERVAL != prev_erase) {      // (total P/E cycle) / (an interval of re-determining write-cold threshold) 
        prev_erase = cur_erase / (int)WR_CYC_INTERVAL;         // ??
        for(int i=0;i<NOB;i++){
			if (metadata->state[i] <= cyc_avg) {   // young block
                if(metadata->state[i] - prev_cyc[i] < erase_diff_min){     // B_coldest set
                    printf("cur state : %d, prev state : %d\n",metadata->state[i],prev_cyc[i]);
                    printf("thres_cold = %d\n",metadata->invalidation_window[i]);
                    erase_diff_min = metadata->state[i] - prev_cyc[i];
                    temp_thres_cold = metadata->invalidation_window[i];
                
                }   
                else if (metadata->state[i] - prev_cyc[i] == erase_diff_min){   // B_coldest set
                    printf("cur state : %d, prev state : %d\n",metadata->state[i],prev_cyc[i]);
                    printf("thres_cold = %d\n",metadata->invalidation_window[i]);
                    if(temp_thres_cold < metadata->invalidation_window[i]){     // THRES_COLD is maximum invalidation count of set B_coldest
                        temp_thres_cold = metadata->invalidation_window[i];
                        //printf("state : %d==%d,temp_thres_cold : %d\n",metadata->state[i],prev_cyc[i],metadata->invalidation_window[i]);
                    }
                }
            }
			if (metadata->state[i] > cyc_avg) {     // old block
				if (metadata->state[i] - prev_cyc[i] > erase_diff_max) {     // B_hotest set
                    erase_diff_max = metadata->state[i] - prev_cyc[i];
                    temp_thres_hot = metadata->invalidation_window[i];
                }
				else if (metadata->state[i] - prev_cyc[i] == erase_diff_max) {      // B_hotest set
					if (temp_thres_hot > metadata->invalidation_window[i]) {     // THRES_HOT is minimum invalidation count of set B_hotest
                        temp_thres_hot = metadata->invalidation_window[i];
                    }
                }
            }
            prev_cyc[i] = metadata->state[i];
        }
        printf("erase_diff_min : %d, erase_diff_max : %d\n",erase_diff_min,erase_diff_max);
        printf("curthres : %d, %d\n",temp_thres_cold,temp_thres_hot);
	THRES_COLD = temp_thres_cold;
	THRES_HOT = temp_thres_hot;
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
    
	// find hot-old, cold-young block (data swap target blocks)
    cur = full_head->head;  // full_head : closed block list ?
    while (cur != NULL){
        //find hot-old(write-hot + high P/E cycle) flash blocks(read count >> avg && oldest)
        if(metadata->invalidation_window[cur->idx] > THRES_HOT){
	    hotnum++;
            if (a == NULL){   // a : previous selected hot-old block
                a = cur;
            } 
			else if (metadata->state[a->idx] < metadata->state[cur->idx]) {     // what is the highest P/E cycle ?
                a = cur;
            }
        }
        //find cold-yng(write-cold + low P/E cycle) flash blocks(read count << avg && youngest)
        else if (metadata->invalidation_window[cur->idx] < THRES_COLD){
            coldnum++;
	    if (b == NULL){   // b : previous selected cold-young block
                b = cur;
            }
            else if (metadata->state[b->idx] > metadata->state[cur->idx]){     // what is the lowest P/E cycle ?
                b = cur;
            }
        }
        cur = cur->next;
    }
    //printf("[WR]hotnum : %d, coldnum : %d\n",hotnum,coldnum);
    //edge case handling
    if(a == NULL || b == NULL){
        *res1 = -1;
        *res2 = -1;
        return;
    } 
	// data swap is possible only when (hot-old block's P/E cycle - cold-young block's P/E cycle) > THRESHOLD
    else if(metadata->state[a->idx] - metadata->state[b->idx] < THRESHOLD){
        *res1 = -1;
        *res2 = -1;
        return;
    }
    //return found block (data swap target blocks)
    //printf("[WR]found block. [%d]state:%d, [%d]state:%d\n",a->idx,metadata->state[a->idx],b->idx,metadata->state[b->idx]);
    *res1 = a->idx;
    *res2 = b->idx;
    return;
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

