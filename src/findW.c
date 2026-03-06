#include "findW.h"

extern double OP;
extern int MINRC;

//FIXME:: a temporary solution to expose
extern int offset;
extern IOhead** rq;
extern long cur_cp;
extern int max_valid_pg;

//FIXME:: a temporary solution to expose update timing & info to find_write_maxinvalid function.
extern long* lpa_update_timing[NOP];
extern int update_cnt[NOP];
extern int tot_longlive_cnt;
extern FILE *updateorder_fp;
extern FILE **w_workloads;

//Determine rank of current lpa using task & locality information.
//assuming that locality is fixed, rank of lpa is determined statically.
int _find_write_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util, int cur_b, int* w_lpas){
    //check if current I/O job does not violate util test along with recently released other jobs.
    
    IO* cur;
    int read_b;
    int valid_cnt = 0;
    int* lpas;
    float total_u = 0.0;
    float wutils[tasknum];
    float rutils[tasknum];
    float gcutils[tasknum];
    // float old_total_u;
    //malloc variables
    lpas = (int*)malloc(sizeof(int)*PPB);
    for(int i=0;i<tasks[taskidx].wn;i++){
        lpas[i] = w_lpas[i];
    }

    //test code:: compare with old find_util_safe function
    // old_total_u = find_cur_util(tasks,tasknum,metadata,old);
    // if(type == WR){
    //     old_total_u -= metadata->runutils[0][taskidx];
    // } else if (type == RD){
    //     old_total_u -= metadata->runutils[1][taskidx];
    // } else if (type == GC){
    //     old_total_u -= metadata->runutils[2][taskidx];
    // }
    // old_total_u += util;

    //allocate current runtime utils on local variable
    for(int i=0;i<tasknum;i++){
        wutils[i] = metadata->runutils[0][i];
        rutils[i] = metadata->runutils[1][i];
        gcutils[i] = metadata->runutils[2][i];
    }

    /*write a code which checks if current lpa collides with read.*/
    /*we do not check collision with GC, since worst case is when write happens after GC.*/
    /*when write happens after GC, GC reqs are not affected at all*/
    for (int i=0;i<tasknum;i++){
        cur = rq[i]->head;
        while(cur != NULL){
           for(int j=0;j<tasks[taskidx].wn;j++){
                if(cur->lpa == lpas[j]){ // read가 접근하는 LPA와 write LPA가 동일한 경우
                    read_b = metadata->pagemap[lpas[j]]/PPB;
                    if(metadata->state[read_b] < metadata->state[cur_b]){
                        rutils[i] = r_exec(metadata->state[cur_b]) / (float)tasks[i].rp;
                    }
                }
            }
            cur = cur->next;
        }
    }
    free(lpas);

    //now calculate total utilization.
    for (int j=0;j<tasknum;j++){
        total_u += wutils[j];
        total_u += rutils[j];
        total_u += gcutils[j];
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    total_u -= wutils[taskidx];
    total_u += util;
    //printf("tot_u : %f\n",total_u);
    if (total_u <= 1.0){
        return 0;
    } else {
        return -1;
    }
}

void _get_jobnum_interval(long curtime, long offset, rttask* task, int tasknum, int* jobnum_per_task){
    //printf("curtime : %ld, offset : %ld",curtime,offset);
    long start = curtime;
    long end = curtime + offset;
    long point = start;
    long closest_next_release[tasknum];
    long next;
    while(point <= end){
        //check if cur time is job release time
        for(int i=0;i<tasknum;i++){
            if(point % task[i].wp == 0){
                jobnum_per_task[i]++;
            }
        }
        //find closest next time
        next = __LONG_MAX__;
        for(int i=0;i<tasknum;i++){
            closest_next_release[i] = ((point/(long)task[i].wp) + 1) * (long)task[i].wp;
            if(next >= closest_next_release[i]){
                next = closest_next_release[i];
            }
        }
        point = next;
    }

    // opt-ver.
    // Count releases in [curtime, curtime+offset] directly per task.
    // This avoids scanning every release point across tasks.
    // long start = curtime;
    // long end = curtime + offset;
    // for(int i=0;i<tasknum;i++){
    //     long wp = (long)task[i].wp;
    //     if(wp <= 0L){
    //         continue;
    //     }
    //     long from = (start <= 0L) ? 0L : ((start - 1L) / wp) + 1L;
    //     long to = end / wp;
    //     if(to >= from){
    //         jobnum[i] += (int)(to - from + 1L);
    //     }
    // }
}

void  _get_write_reqs(FILE** w_workloads, int tasknum, int taskidx, rttask* task, int* w_lpas, long length, int* jobnum, int** lpas_to_arrange){
    long orig_fp_offset[tasknum];
    for(int i=0;i<tasknum;i++){
        orig_fp_offset[i] = ftell(w_workloads[i]);
    }
    //get lpas to arrange (어떤 순서로 오더라도 상관 없나?)
    for (int i=0;i<tasknum;i++){ 
        int lpas_offset = 0;
        int scan_ret = 0;
        int ret_val;
        for(int j=0;j<jobnum[i];j++){
            if(i == taskidx && j == 0){
                for(int k=0;k<task[i].wn;k++){
                    lpas_to_arrange[i][lpas_offset] = w_lpas[k];
                    //printf("lpas_to_arrange[%d][%d] : %d\n",i,lpas_offset,lpas_to_arrange[i][lpas_offset]);
                    lpas_offset++;
                }
            }
            else {
                for(int k=0;k<task[i].wn;k++){ 
                    scan_ret = fscanf(w_workloads[i],"%d,",&(ret_val));
                    if(scan_ret != EOF){
                        lpas_to_arrange[i][lpas_offset] = ret_val;
                        //printf("lpas_to_arrange[%d][%d] : %d\n",i,lpas_offset,lpas_to_arrange[i][lpas_offset]);
                        
                    } 
                    else {
                        rewind(w_workloads[i]);
                        fscanf(w_workloads[i],"%d,",&(lpas_to_arrange[i][lpas_offset]));
                    }
                    lpas_offset++;
                }
            }
        }
    }

    //return file pointer to original position (for trace read during "real" write req generation)
    for(int i=0;i<tasknum;i++){
        long cur_fp_position = ftell(w_workloads[i]);
        fseek(w_workloads[i],orig_fp_offset[i],SEEK_SET);
    }
    //sleep(3);
}

void _get_write_updateorder(int* jobnum, int** lpas_to_arrange, int tasknum, rttask* task, meta* metadata, long** updateorder_per_task){
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<jobnum[i]*task[i].wn;j++){
            int lpa_cur = lpas_to_arrange[i][j];
            //normally lpa freq will be 0 (lpa X accessed more than twice in single window)
            int lpa_cyc = metadata->write_cnt_per_cycle[lpa_cur];
            if(lpa_cyc <= update_cnt[lpa_cur]-1){
                updateorder_per_task[i][j] = lpa_update_timing[lpa_cur][lpa_cyc+1];
            } else {
                updateorder_per_task[i][j] = WORKLOAD_LENGTH+cur_cp;
            }
            //write the update order in timings_for_write array in metadata.
            metadata->cur_rank_info.timings_for_write[i][j] = updateorder_per_task[i][j];
        }
    }
}

int __calc_time_diff(struct timeval a, struct timeval b){
    int sec = (b.tv_sec - a.tv_sec)*1000000;
    int usec = (b.tv_usec - a.tv_usec);
    return sec+usec;
}

int abs_int(int x){
    if (x < 0){
        return -x;
    } else {
        return x;
    }
}

int __get_rank(int order, meta* metadata){
    int ranknum = metadata->ranknum;
    for(int i=0;i<ranknum;i++){
        if((long)order >= metadata->rank_bounds[i] && (long)order < metadata->rank_bounds[i+1]){
            //printf("inbound order : %d, rank : %d\n",order,i);
            return i;
        }
    }
    //printf("outofbound order : %d, rank : %d(lastrank)\n", order, ranknum);
    return ranknum;
}

int __get_rank_long_dyn(long order, long* rank, int ranknum){
    
    // prev-ver.
    for(int i=0;i<ranknum;i++){
        if(order <= rank[i] && i == 0){
            return 0;
        } 
        else if (order > rank[i-1] && order <= rank[i]){
            return i;
        }
    }
    return ranknum;

    // opt-ver.
    // int left = 0;
    // int right = ranknum - 1;
    // int pos = ranknum;
    // while(left <= right){
    //     int mid = left + (right - left) / 2;
    //     if(order <= rank[mid]){
    //         pos = mid;
    //         right = mid - 1;
    //     } else {
    //         left = mid + 1;
    //     }
    // }
    // return pos;
}

int __calc_invorder_file(int pagenum, meta* metadata, long cur_lpa_timing, long workload_reset_time, int curfp){
    int invalid_per_lpa = 0;
    int ret = 0;
    long other_lpa_timing;
    FILE* other_lpa_file;
    char name[30];
    for(int i=0;i<pagenum;i++){
        //if next update timing of LPA i is before current LPA's next update timing(earlier invalidation)
        if(metadata->next_update[i] + workload_reset_time < cur_lpa_timing){
            invalid_per_lpa = 0;
            sprintf(name,"./timing/%d.csv",i);
            other_lpa_file = fopen(name,"r");
            if(other_lpa_file != NULL){
                while(EOF != fscanf(other_lpa_file,"%ld,",&other_lpa_timing)){
                    //printf("lpa :%d, other_lpa_timing : %ld\n",i,other_lpa_timing);
                    if(other_lpa_timing+workload_reset_time <= cur_lpa_timing && other_lpa_timing+workload_reset_time > cur_cp){
                        invalid_per_lpa++;
                    }
                    if(other_lpa_timing > cur_lpa_timing){
                        break;
                    }
                }
            }
            if(invalid_per_lpa >= 1){
                invalid_per_lpa -= 1; //minus 1 since first invalidation is occured in pre-written block.
            }
            if(other_lpa_file != NULL){
                fclose(other_lpa_file);
            }
        }
        ret += invalid_per_lpa;
        if(ret >= curfp){
            return ret;
        }
    }
    return ret;
}

int __calc_invorder_mem(int pagenum, meta* metadata, long cur_lpa_timing, long workload_reset_time, int curfp){
    int invalid_per_lpa = 0;
    int update_num = 0;
    int ret = 0;
    long cur_update_timing = 0L;
    for(int i=0;i<pagenum;i++){
        cur_update_timing = 0L;
        invalid_per_lpa = 0;
        update_num = 0;
        if(metadata->next_update[i] < cur_lpa_timing){    
            while(cur_update_timing <= cur_lpa_timing){
                cur_update_timing = lpa_update_timing[i][update_num];
                if(cur_update_timing <= cur_lpa_timing && cur_update_timing > cur_cp){
                    invalid_per_lpa++;
                }
                update_num++;
                //printf("update num : %d\n",update_cnt[i]);
                if(update_cnt[i] <= update_num){
                    break;
                }
            }
        }
        if(invalid_per_lpa >= 1){
            invalid_per_lpa -= 1;
        }
        ret += invalid_per_lpa;
    }
    fprintf(updateorder_fp,"%d,\n",ret);
    return ret;
}

// function for write block selection
block* find_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long workload_reset_time, FILE* w_assign_detail){
    // 3 variants
    // MAXINVALID_RANK_DYN :: window-based request clustering. dynamically change range for cluster
    // MAXINVALID_RANK_STAT :: window-based request clutsering, based on pre-assigned threshold
    // MAXINVALID_RANK_FIXED :: request clustering, strictly following absolute request order

    // params to find lpa rank
    char name[30];
    FILE* cur_lpa_timing_file;
    long cur_lpa_timing;
    int cnt = 0;
    int curfp = metadata->total_fp;
    int old = get_blockstate_meta(metadata,OLD); // a function to find oldset block inside whole system
#ifndef TIMING_ON_MEM    
    sprintf(name,"./timing/%d.csv",w_lpas[idx]);
    cur_lpa_timing_file = fopen(name,"r");
#endif

    // params to find block
    block* cur;
    block* ret_targ;
    block* wb_new;
    block* fb_ptr;
    block* left_ptr;
    block* right_ptr;
    int cur_state = -1;
    int yield_pg = 0;
    int youngest = MAXPE;
    int target = -1;
    int longlive = -1;
    int cur_fpnum;
    int* jobnum = NULL;
    int** req_per_task = NULL;
    long** updateorders = NULL;

#ifdef MAXINVALID_RANK_DYN
    // active rank calculation-based method

    // 1. if pre-assigned write is finished, re-assign ranks for future N writes.
    //    find jobs and their corresponding update timing within given interval (cur interval = 500ms)
    if(metadata->cur_rank_info.cur_left_write[taskidx] == 0){
        jobnum = (int*)malloc(sizeof(int)*tasknum);
        req_per_task = (int**)malloc(sizeof(int*)*(unsigned long)tasknum);
        updateorders = (long**)malloc(sizeof(long*)*(unsigned long)tasknum);
        for(int i=0;i<tasknum;i++){
            req_per_task[i] = NULL;
            updateorders[i] = NULL;
        }
        for(int i=0;i<tasknum;i++){
            jobnum[i] = 0;
        }

        // jobnum: task마다 release되는 job 개수
        _get_jobnum_interval(cur_cp,90000000,task,tasknum,jobnum);
    
        for(int i=0;i<tasknum;i++){
            req_per_task[i] = (int*)malloc(sizeof(int)*(unsigned long)jobnum[i]*(unsigned long)task[i].wn);
            updateorders[i] = (long*)malloc(sizeof(long)*(unsigned long)jobnum[i]*(unsigned long)task[i].wn);
        }

        // req_per_task: 각 task의 request LPA 저장
        _get_write_reqs(w_workloads,tasknum,taskidx,task,w_lpas,90000000,jobnum,req_per_task);
        // updateorders: LPA에 대해 다음 request가 올 시점 저장
        _get_write_updateorder(jobnum,req_per_task,tasknum,task,metadata,updateorders);
    
        // 1-(1). put update timing values in single array
        int reqnum = 0;
        for(int i=0;i<tasknum;i++){
            reqnum += jobnum[i] * task[i].wn;
        }
        long* invorders_sort = (long*)malloc(sizeof(long)*reqnum);
        int invorderarr_idx = 0;
        for(int i=0;i<tasknum;i++){
            for(int j=0;j<jobnum[i]*task[i].wn;j++){
                invorders_sort[invorderarr_idx] = updateorders[i][j];
                invorderarr_idx++;
            }
        }
        
        // 1-(2). sort update timing array and find bounds for each rank
        long* bounds = (long*)malloc(sizeof(long)*(metadata->ranknum));
        int cluster_cur = 0;
        int member_num = 0;
        int req_per_rank = reqnum / (metadata->ranknum+1);
        qsort(invorders_sort,reqnum,sizeof(long),compare);
        for(int i=0;i<reqnum;i++){
            member_num++;
            if(member_num == req_per_rank){
                bounds[cluster_cur] = invorders_sort[i];
                cluster_cur++;
                member_num = 0;
            }
            if(cluster_cur == metadata->ranknum){
                break;
            }
        }
        // 1-(2)-1. save current bound info on metadata(note that metadata->rank_bound[0] = 0)
        if(metadata->rank_bounds == NULL){
            metadata->rank_bounds = (long*)malloc(sizeof(long)*(metadata->ranknum + 1));
        }
        metadata->rank_bounds[0] = 0;
        for(int i=0;i<metadata->ranknum;i++){
            metadata->rank_bounds[i+1] = bounds[i];
        }
        
        // 1-(3). update cur_rank_info structure in metadata
        for(int i=0;i<tasknum;i++){
            int record_idx = 0;
            if(metadata->cur_rank_info.cur_left_write[i] != 0){
                // if a task has a leftover rank records, move them to front of rank array.
                for(int j=0;j<metadata->cur_rank_info.cur_left_write[i];j++){
                    int offset = metadata->cur_rank_info.tot_ranked_write[i] - metadata->cur_rank_info.cur_left_write[i] + j;
                    metadata->cur_rank_info.ranks_for_write[record_idx] = metadata->cur_rank_info.ranks_for_write[offset];
                    record_idx++;
                }
            }
            metadata->cur_rank_info.cur_left_write[i] += jobnum[i] * task[i].wn;
            metadata->cur_rank_info.tot_ranked_write[i] = metadata->cur_rank_info.cur_left_write[i];
            for(int j=0;j<jobnum[i]*task[i].wn;j++){
                // find rank of each reqs & make a rank record.
                metadata->cur_rank_info.ranks_for_write[i][record_idx] = __get_rank_long_dyn(updateorders[i][j],bounds,metadata->ranknum);
                record_idx++;
            }
        }
        // 1-(4). free malloc spaces
        free(jobnum);
        free(req_per_task[0]);
        free(updateorders[0]);
        free(req_per_task[1]);
        free(updateorders[1]);
        free(req_per_task[2]);
        free(updateorders[2]);
        free(req_per_task[3]);
        free(updateorders[3]);
        free(req_per_task);
        free(updateorders);
        free(invorders_sort);
        free(bounds);    
    }

    // get rank info from metadata & update left write
    int offset = metadata->cur_rank_info.tot_ranked_write[taskidx] - metadata->cur_rank_info.cur_left_write[taskidx];
    int cur_rank = metadata->cur_rank_info.ranks_for_write[taskidx][offset];
    metadata->cur_rank_info.cur_left_write[taskidx] -= 1;

    // 2. find corresponding block
    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(cur->wb_rank == cur_rank && _find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == 0){

            return cur;
        }

        cur = cur->next;
    } 

    // 3. if block not in wblist, try getting a new block
    // 3-(1). search through free block list
    int ret_b_idx = -1;
    int ret_b_state = __INT_MAX__;
    cur = fblist_head->head;

    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(ret_b_state > metadata->state[cur->idx] && _find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == 0){
            ret_b_state = metadata->state[cur->idx];
            ret_b_idx = cur->idx;
        }
	    cur = cur->next;
    }
    // 3-(2). if free block found, append to write block and return index.
    if(ret_b_idx != -1){
        block* wb_new = ll_remove(fblist_head,ret_b_idx);
        wb_new->wb_rank = cur_rank;
        ll_append(write_head,wb_new);

        return wb_new;
    }
    // 3-(3). if free block not found, find closest cluster in write block list.
    else{
        cur = write_head->head;
        int offset = abs_int(cur->wb_rank - cur_rank);
        ret_b_idx = cur->idx;

        while(cur != NULL){      
            cur_state = metadata->state[cur->idx];
            if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
                cur = cur->next;
                continue;
            }   
            if(offset >= abs_int(cur->wb_rank - cur_rank)){
                offset = abs_int(cur->wb_rank - cur_rank);
                ret_b_idx = cur->idx;
            }
            cur = cur->next;
        }

        return cur;
    }

    // 4. 적절한 블록을 찾지 못한 경우, write_head와 fblist_head 중에서 가장 lowest PEC 블록에 writing
    if (ret_b_idx == -1){
        // printf("[e] no schedulability block found: alloc to lowest block...\n");
        int final_idx   = -1;
        int final_state = __INT_MAX__;

        // 4-(1). write_head 전체 스캔
        cur = write_head->head;
        while (cur != NULL) {
            cur_state = metadata->state[cur->idx];
            if (cur_state < final_state) {
                final_state = cur_state;
                final_idx   = cur->idx;
            }
            cur = cur->next;
        }

        if (final_idx != -1) {
            return cur;
        }

        // 4-(2). free block 리스트도 같이 스캔
        cur = fblist_head->head;
        while (cur != NULL) {
            cur_state = metadata->state[cur->idx];
            if (cur_state < final_state) {
                final_state = cur_state;
                final_idx   = cur->idx;
            }
            cur = cur->next;
        }

        // 여기까지 오면 final_idx에 "PEC가 가장 낮은 블록의 idx"가 들어있어야 함
        if (final_idx == -1) {
            // 4-(3). 진짜로 아무것도 못 찾은 극단 케이스
            fprintf(stderr,
                    "\n[Critical Error] No block found even in fallback.");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        // 4-(4). 이 final_idx가 free list에 있으면 빼서 write_head로 옮긴다
        else {
            wb_new = ll_remove(fblist_head,final_idx);
            wb_new->wb_rank = cur_rank;
            ll_append(write_head,wb_new);

            return wb_new;
        }
    }
    // !end of active rank calculation-based method
#endif

    // find out current lpa's update order 

#ifdef TIMING_ON_MEM   
    int cur_lpa_nextupdatenum = metadata->write_cnt_per_cycle[w_lpas[idx]]+1;
    if(cur_lpa_nextupdatenum >= update_cnt[w_lpas[idx]]){
        cur_lpa_timing = workload_reset_time + WORKLOAD_LENGTH;
    } else {
        cur_lpa_timing = lpa_update_timing[w_lpas[idx]][cur_lpa_nextupdatenum];
    }
#endif
#ifndef TIMING_ON_MEM
    for(int i=0;i<metadata->write_cnt[w_lpas[idx]]+2;i++){
        if(EOF == fscanf(cur_lpa_timing_file,"%ld,",&cur_lpa_timing)){
            cur_lpa_timing = workload_reset_time + WORKLOAD_LENGTH;
        }      
    }
    cur_lpa_timing += workload_reset_time;
    fclose(cur_lpa_timing_file);
#endif

#ifdef TIMING_ON_MEM
    cnt = __calc_invorder_mem(max_valid_pg, metadata, cur_lpa_timing, workload_reset_time, curfp);
#endif
#ifndef TIMING_ON_MEM
    cnt = __calc_invorder_file(max_valid_pg,metadata,cur_lpa_timing,workload_reset_time,curfp);
#endif

    if(cnt >= curfp){
        longlive = 1;
        tot_longlive_cnt++;
    }

#ifdef MAXINVALID_RANK_STAT
    int rank = __get_rank(cnt,metadata);
    int ret_b_idx;
    cur = write_head->head;
    // find a corresponding rank block in wblist 
    while(cur != NULL){ 
        if(cur->wb_rank == rank){
            // printf("[1]rank : %d, wbrank : %d\n",rank,cur->wb_rank);
            return cur->idx;
        }
        cur = cur->next;
    }
    // if block not in wblist, try getting a new block
    if(cur == NULL){
        int temp = find_block_in_list(metadata,fblist_head,YOUNG);
        // if block found in fblist, append to write block & return idx
        if (temp != -1){
            block* wb_new = ll_remove(fblist_head,temp);
            wb_new->wb_rank = rank;
            ll_append(write_head,wb_new);
            //printf("[2]rank : %d, wbrank : %d\n",rank,wb_new->wb_rank);
            return wb_new->idx;
        }
        // if block not in fblist, do edge case handling
        else{
            cur = write_head->head;
            int offset = abs_int(cur->wb_rank - rank);
            ret_b_idx = cur->idx;
            while(cur != NULL){
                if(offset >= abs_int(cur->wb_rank - rank)){
                    offset = abs_int(cur->wb_rank - rank);
                    ret_b_idx = cur->idx;
                }
                cur = cur->next;
            }
            // printf("[e]rank : %d alloc to other block...\n",rank);
            return ret_b_idx;
        }
    }
    printf("[MAXINV]write block alloc error!\n");
    abort();
#endif

#ifdef MAXINVALID_RANK_FIXED
    gettimeofday(&a,NULL);
    if(longlive == 1){                      // Edgecase:: free page is not enough to handle target lpa
        while(fblist_head->blocknum != 0){
            youngest = MAXPE;
            target = -1;
            // search through free block list, finding a youngest block, 
            fb_ptr = fblist_head->head;
            while(fb_ptr != NULL){
                if(youngest > metadata->state[fb_ptr->idx]){
                    target = fb_ptr->idx;
                    youngest = metadata->state[fb_ptr->idx];

                }
                fb_ptr = fb_ptr->next;
            }
            // append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        // give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                gettimeofday(&b,NULL);
                // printf("[3]%d\n",__calc_time_diff(a,b));
                return cur->idx;    
            }
            cur = cur->next;
        }
        longlive = 0;
    }
    
    // write block list is arranged in descending order. write the page w.r.t its update order.
    yield_pg = 0;
    cur = write_head->head;
    while(cur != NULL){
        yield_pg += cur->fpnum;
        if(yield_pg > cnt){
            // if current block has enough page to satisfy update order,
            break;
        } else {
            // search next if not.
            cur = cur->next;
        }
        // printf("[W]yieldpg : %d\n",yield_pg);
    }
    if(cur == NULL){                        // no feasible block in writeblock list.
        while(fblist_head->blocknum != 0){
            youngest = MAXPE;
            target = -1;
            // search through free block list, finding a youngest block, 
            fb_ptr = fblist_head->head;
            while(fb_ptr != NULL){
                if(youngest > metadata->state[fb_ptr->idx]){
                    target = fb_ptr->idx;
                    youngest = metadata->state[fb_ptr->idx];
                }
                fb_ptr = fb_ptr->next;
            }
            // append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
            yield_pg += wb_new->fpnum;
            if(yield_pg > cnt){
                cur = wb_new;
                break;
            } else {
                // do nothing. 
                // repeat while loop until free block runs out.
            }
            // printf("[FB]yieldpg : %d\n",yield_pg);
        }
    }
    if(cur == NULL){                                    // Edgecase:: not found corresponding block
        while(fblist_head->blocknum != 0){
            // search through free block list, finding a youngest block,
            youngest = MAXPE;
            target = -1;
            fb_ptr = fblist_head->head;
            while(fb_ptr != NULL){
                if(youngest > metadata->state[fb_ptr->idx]){
                    target = fb_ptr->idx;
                    youngest = metadata->state[fb_ptr->idx];
                }
                fb_ptr = fb_ptr->next;
            }
            // append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        // give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                gettimeofday(&b,NULL);
                // printf("[3]%d\n",__calc_time_diff(a,b));
                return cur->idx;    
            }
            cur = cur->next;
        }
        longlive = 0;
    }

    // initiate findwritesafe() on current block, and see if allocation leads to utilization overflow.
    if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[cur->idx]),cur->idx,w_lpas) == 0){
        gettimeofday(&b,NULL);
        // printf("[3]%d\n",__calc_time_diff(a,b));
        return cur->idx;
    } else {
        // traverse left and right to find suitable block
        left_ptr = cur->prev;
        right_ptr = cur->next;
        while((left_ptr != NULL) || (right_ptr != NULL)){
            if(left_ptr != NULL){
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[left_ptr->idx]),cur->idx,w_lpas) == 0){
                    gettimeofday(&b,NULL);
                    // printf("[3]%d\n",__calc_time_diff(a,b));
                    return left_ptr->idx;
                } else{
                    left_ptr = left_ptr->prev;
                }
            }
            if(right_ptr != NULL){
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[right_ptr->idx]),cur->idx,w_lpas) == 0){
                    gettimeofday(&b,NULL);
                    // printf("[3]%d\n",__calc_time_diff(a,b));
                    return right_ptr->idx;
                } else{
                    right_ptr = right_ptr->next;
                }
            }
        }
    }
    // if findwritesafe() fails on all block, just return cur.
    gettimeofday(&b,NULL);
    // printf("[3]%d\n",__calc_time_diff(a,b));
    return cur->idx;
#endif
}