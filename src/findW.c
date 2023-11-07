#include "findW.h"

extern double OP;
extern int MINRC;

//FIXME:: a temporary solution to expose locality variables to find_write_gradient function
extern float sploc;
extern float tploc;
extern int offset;

//FIXME:: a temporary solution to expose queue to find_util_safe function.
extern IOhead** wq;
extern IOhead** rq;
extern IOhead** gcq;

//FIXME:: a temporary solution to expose proportion to find_write_gradient function.
extern double* w_prop;
extern double* r_prop;
extern double* gc_prop;

//FIXME:: a temporary solution to expose experiment values to find_write_gradient function.
extern long cur_cp;
extern int max_valid_pg;
extern FILE **fps;

//FIXME:: a temporary solution to expose update timing & info to find_write_maxinvalid function.
extern long* lpa_update_timing[NOP];
extern int update_cnt[NOP];
extern int tot_longlive_cnt;
extern FILE *longliveratio_fp;
extern FILE *updateorder_fp;
extern FILE **w_workloads;
extern FILE **r_workloads;

//Determine rank of current lpa using task & locality information.
//assuming that locality is fixed, rank of lpa is determined statically.
void _find_rank_lpa(rttask* tasks, int tasknum){
    //calculate w/r intensity
    //even idx 2*i = intensity of hot proportion of task i.
    //odd idx (2*i)+1 = intensity of cold proportion of task i.
    
    //init params
    double w_gradient = (double)(ENDW - STARTW) / (double)MAXPE;
    double r_gradient = (double)(ENDR - STARTR) / (double)MAXPE;
    int temp_int;
    int hotspace[tasknum];
    double temp_d;

    //init intensity arrays
    double write_intensity[tasknum * 2];
    int write_idx[tasknum*2];
    double read_intensity[tasknum * 2];
    int read_idx[tasknum*2];
    double gc_intensity[tasknum * 2];
    int gc_idx[tasknum*2];
    double wi_sum_arr[tasknum * 2];
    double ri_sum_arr[tasknum * 2];
    double gci_sum_arr[tasknum * 2];
    double w_proportion[tasknum * 2];
    double r_proportion[tasknum * 2];
    double gc_proportion[tasknum * 2];
    double w_weight_sum = 0.0, r_weight_sum = 0.0, gc_weight_sum = 0.0, upper_weight = 0.0;
    
    
    //init index and intensity values
    //intensity is # of access per unit time, calculated as task (I/O speed * address pick probability) 
    for(int i=0;i<tasknum;i++){
        hotspace[i] = (int)((float)(tasks[i].addr_ub - tasks[i].addr_lb)*tasks[i].sploc);
        printf("%f, %f, %d\n",tasks[i].tploc,tasks[i].sploc,hotspace[i]);
        write_intensity[2*i] =   (double)tasks[i].wn / (double)tasks[i].wp * (1.0/(double)hotspace[i]) * (double)tasks[i].tploc;
        write_intensity[(2*i)+1] =  (double)tasks[i].wn / (double)tasks[i].wp * (1.0/(double)((tasks[i].addr_ub - tasks[i].addr_lb - hotspace[i]))) * (1.0 - (double)tasks[i].tploc);
        read_intensity[2*i] = (double)tasks[i].rn / (double)tasks[i].rp * (1/(double)hotspace[i]) * tasks[i].tploc;
        read_intensity[(2*i)+1] = (double)tasks[i].rn / (double)tasks[i].rp * (1/(double)((tasks[i].addr_ub - tasks[i].addr_lb - hotspace[i]))) * (1.0 - (double)tasks[i].tploc);
        gc_intensity[2*i] = write_intensity[2*i] / (double)MINRC;
        gc_intensity[(2*i)+1] = write_intensity[(2*i)+1] / (double)MINRC;
        write_idx[2*i] = 2*i;
        write_idx[2*i+1] = 2*i+1;
        read_idx[2*i] = 2*i;
        read_idx[2*i+1] = 2*i+1;
        gc_idx[2*i] = 2*i;
        gc_idx[2*i+1] = 2*i+1;
    }


    //sort the intensity for each lpa section.
    for(int i=tasknum*2-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(write_intensity[j] > write_intensity[j+1]){
                temp_d = write_intensity[j];
                write_intensity[j] = write_intensity[j+1];
                write_intensity[j+1] = temp_d;
                temp_int = write_idx[j];
                write_idx[j] = write_idx[j+1];
                write_idx[j+1] = temp_int;
            }
        }
    }
    for(int i=tasknum*2-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(read_intensity[j] > read_intensity[j+1]){
                temp_d = read_intensity[j];
                read_intensity[j] = read_intensity[j+1];
                read_intensity[j+1] = temp_d;
                temp_int = read_idx[j];
                read_idx[j] = read_idx[j+1];
                read_idx[j+1] = temp_int;
            }
        }
    }
    for(int i=tasknum*2-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(gc_intensity[j] > gc_intensity[j+1]){
                temp_d = gc_intensity[j];
                gc_intensity[j] = gc_intensity[j+1];
                gc_intensity[j+1] = temp_d;
                temp_int = gc_idx[j];
                gc_idx[j] = gc_idx[j+1];
                gc_idx[j+1] = temp_int;
            }
        }
    }
  
    //with the sorted intensity, calculate proportion of each section.
    //calculated as (sum of weight of upper rank lpa section / total sum of weight)
    //calculate sum of weight for each section. weight of section is calc as (num of lpa * intensity)
    for(int i=0;i<tasknum*2;i++){
        if(write_idx[i] % 2 == 0){//hotspace
            wi_sum_arr[i] = (double)hotspace[i/2] * write_intensity[i];
        } else { //coldspace
            wi_sum_arr[i] = (double)(tasks[i/2].addr_ub - tasks[i/2].addr_lb - hotspace[i]) * write_intensity[i];
        }
        if(read_idx[i] % 2 == 0){
            ri_sum_arr[i] = (double)hotspace[i/2] * read_intensity[i];
        } else {
            ri_sum_arr[i] = (double)(tasks[i/2].addr_ub - tasks[i/2].addr_lb - hotspace[i]) * read_intensity[i];
        }
        if(gc_idx[i] % 2 == 0){
            gci_sum_arr[i] = (double)hotspace[i/2] * gc_intensity[i];
        } else {
            gci_sum_arr[i] = (double)(tasks[i/2].addr_ub - tasks[i/2].addr_lb - hotspace[i]) * gc_intensity[i];
        }
    }

    //calculate total sum of intensity
    for(int i=0;i<tasknum*2;i++){
        w_weight_sum += wi_sum_arr[i];
        r_weight_sum += ri_sum_arr[i];
        gc_weight_sum += gci_sum_arr[i];
    }

    for(int i=0;i<tasknum*2;i++){
        upper_weight = 0.0;
        for( int j=i+1;j<tasknum*2;j++){
            upper_weight += wi_sum_arr[j];
        }
        w_proportion[i] = upper_weight / w_weight_sum;

        upper_weight = 0.0;
        for( int j=i+1;j<tasknum*2;j++){
            upper_weight += ri_sum_arr[j];
        }
        r_proportion[i] = upper_weight / r_weight_sum;

        upper_weight = 0.0;
        for( int j=i+1;j<tasknum*2;j++){
            upper_weight += gci_sum_arr[j];
        }
        gc_proportion[i] = upper_weight / gc_weight_sum;
    }
    
    for(int i=0;i<tasknum*2;i++){
        w_prop[write_idx[i]] = w_proportion[i];
        r_prop[read_idx[i]] = r_proportion[i];
        gc_prop[gc_idx[i]] = gc_proportion[i];
        printf("[W]%d-th section : %f\n",write_idx[i],w_proportion[i]);
        printf("[R]%d-th section : %f\n",read_idx[i],r_proportion[i]);
       printf("[G]%d-th section : %f\n",gc_idx[i],gc_proportion[i]);
    }
    //as proportion is determined, a task will use (length * proportion)-th flash block for their allocation.
    //e.g., if proportion is 0.5, it will be allocated to (length/2)-th flash block.
    //small proportion =  no upper-ranked lpa exist
    //high proportion = many upper-ranked lpa exist
    //as proportion is determined, the constant value is utilized at find_write function.
}

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
    float old_total_u;
    //malloc variables
    lpas = (int*)malloc(sizeof(int)*PPB);
    for(int i=0;i<tasks[taskidx].wn;i++){
        lpas[i] = w_lpas[i];
    }

    //test code:: compare with old find_util_safe function
    old_total_u = find_cur_util(tasks,tasknum,metadata,old);
    if(type == WR){
        old_total_u -= metadata->runutils[0][taskidx];
    } else if (type == RD){
        old_total_u -= metadata->runutils[1][taskidx];
    } else if (type == GC){
        old_total_u -= metadata->runutils[2][taskidx];
    }
    old_total_u += util;

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
        while (cur != NULL){
           for(int j=0;j<tasks[taskidx].wn;j++){
                if(cur->lpa == lpas[j]){
                    //printf("collision detected, %d vs %d ",cur->lpa,lpas[j]);
                    read_b = metadata->pagemap[lpas[j]]/PPB;
                    //printf("state : %d, %d\n",metadata->state[read_b],metadata->state[cur_b]);
                    if(metadata->state[read_b] < metadata->state[cur_b]){
                        rutils[i] -= r_exec(metadata->state[read_b]) / (float)tasks[i].rp;
                        rutils[i] += r_exec(metadata->state[cur_b]) / (float)tasks[i].rp;
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
    } else if (total_u > 1.0){
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
}

void  _get_write_reqs(FILE** w_workloads, int tasknum, int taskidx, rttask* task, int* w_lpas, long length, int* jobnum, int** lpas_to_arrange){
    long orig_fp_offset[tasknum];
    for(int i=0;i<tasknum;i++){
        orig_fp_offset[i] = ftell(w_workloads[i]);
    }
    //for(int i=0;i<tasknum;i++){
    //    printf("reqnums=%d X %d = %d\n",jobnum[i],task[i].wn,task[i].wn * jobnum[i]);
    //}
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
    //for(int i=0;i<tasknum;i++){
    //    printf("task %d :: ",i);
    //    for(int j=0;j<task[i].wn * jobnum[i];j++){
    //        printf("%d, ",lpas_to_arrange[i][j]);
    //    }
    //    printf("\n");
    //}

    //return file pointer to original position (for trace read during "real" write req generation)
    for(int i=0;i<tasknum;i++){
        long cur_fp_position = ftell(w_workloads[i]);
        fseek(w_workloads[i],orig_fp_offset[i],SEEK_SET);
    }
    //sleep(3);
}

void _get_write_updateorder(int* jobnum, int** lpas_to_arrange, int tasknum, rttask* task, meta* metadata, long** updateorder_per_task){
    long *temp;
    for(int i=0;i<tasknum;i++){
        temp = (long*)malloc(sizeof(long)*task[i].wn*jobnum[i]);
        for(int j=0;j<jobnum[i]*task[i].wn;j++){
            int lpa_cur = lpas_to_arrange[i][j];
            //check if current lpa is repetitively accessed in current window.
            char lpa_freq = 0;
            for(int k=0;k<j-1;k++){
                if(temp[k]==lpa_cur){
                    lpa_freq++;
                }
            }
            //normally lpa freq will be 0 (lpa X accessed more than twice in single window)
            int lpa_cyc = metadata->write_cnt_per_cycle[lpa_cur];
            if(lpa_cyc+lpa_freq <= update_cnt[lpa_cur]-1){
                updateorder_per_task[i][j] = lpa_update_timing[lpa_cur][lpa_cyc+1+lpa_freq];
            } else {
                updateorder_per_task[i][j] = WORKLOAD_LENGTH+cur_cp;
            }
            //write the update order in timings_for_write array in metadata.
            metadata->cur_rank_info.timings_for_write[i][j] = updateorder_per_task[i][j];
        }
        free(temp);
    }
}

void __swap_int_array(int* arr, int idx, int idx2){
    int temp;
    temp = arr[idx];
    arr[idx2] = arr[idx];
    arr[idx] = temp;
}

int __calc_time_diff(struct timeval a, struct timeval b){
    int sec = (b.tv_sec - a.tv_sec)*1000000;
    int usec = (b.tv_usec - a.tv_usec);
    return sec+usec;
}
long abs_long(long x){
    if (x < 0L){
        return -x;
    } else {
        return x;
    }
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
    for(int i=0;i<ranknum;i++){
        if(order <= rank[i] && i == 0){
            return 0;
        } 
        else if (order > rank[i-1] && order <= rank[i]){
            return i;
        }
    }
    return ranknum;
}

int find_writectrl(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head){
    //find a optimized value for worst case utilization
    
    //init params
    int yng, old, ret, iter;
    float cur_worst, cur_write, new_write, margin;
    int cyc[NOB];
    float rw_util[NOB];
    yng = get_blockstate_meta(metadata,YOUNG);
    old = get_blockstate_meta(metadata,OLD);
    cur_worst = find_worst_util(task,tasknum,metadata);
    iter = 0;
    //find a worst read block for task
    //FIXME:: we can add worst-block for task in metadata array.
    int cur_read_worst = 0;
    for(int i=0;i<NOP;i++){
        if(metadata->vmap_task[i] == taskidx){
            int cur_b = i/PPB;
            if(metadata->state[cur_b] >= cur_read_worst){
                cur_read_worst = metadata->state[cur_b];
            }
        }
    }
    printf("task read worst : %d, system worst : %d\n",cur_read_worst,old);

    //draw a write-read profile according to cur_read_worst
    //printf("state(util):");
    for(int i=yng;i<=old;i++){
        cyc[iter] = i;
        if(i<cur_read_worst){
            rw_util[iter] = __calc_wu(&(task[taskidx]),i) + __calc_ru(&(task[taskidx]),cur_read_worst) + __calc_gcu(&task[taskidx],MINRC,yng,i,i);
        }
        else if (i >= cur_read_worst){
            rw_util[iter] = __calc_wu(&(task[taskidx]),i) + __calc_ru(&(task[taskidx]),i) + __calc_gcu(&task[taskidx],MINRC,yng,i,i);;
        }
        //printf("%d(%f) ",i,rw_util[iter]);
        iter++;   
    }
    printf("\n");
    //find a suitable block for best utilization
    block* cur = fblist_head->head;
    float cur_util = -1.0;
    int best_idx = -1;
    int cur_state;
    //printf("cur bnum : %d\n",fblist_head->blocknum);
    while(cur != NULL){
        //check if the block is OK for write
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) ) == -1){
            cur = cur->next;
            continue;
        }
        //if block is OK, test of it is optimal block
        //printf("[F]candidate : %d, %d, %d, %f\n",cur->idx,cur_state,cur->fpnum,rw_util[cur_state-yng]);
        if(cur_util == -1.0 || cur_util >= rw_util[cur_state-yng]){
            cur_util = rw_util[cur_state-yng];
            best_idx = cur->idx;
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    //printf("cur bnum : %d\n",write_head->blocknum);
    while(cur != NULL){
        //check if the block is OK for write
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) ) == -1){
            cur = cur->next;
            continue;
        }
        //printf("[W]candidate : %d, %d, %d, %f\n",cur->idx,cur_state,cur->fpnum,rw_util[cur_state-yng]);
        if(cur_util == -1.0 || cur_util >= rw_util[cur_state-yng]){
            cur_util = rw_util[cur_state-yng];
            best_idx = cur->idx;
        }
        cur = cur->next;
    }//!writelist search end
    printf("best block : %d, state : %d, util : %f\n",best_idx,metadata->state[best_idx],cur_util);
    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    return best_idx;
}

int find_writelimit(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas){
    //use expected read utilization & GC utilization to caculate value for each flash block

    //params
    int cur_state;
    float ru, gcu;
    block* cur;
    float cur_best_util = 1.0;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);
    float cur_read_lat = calc_readlatency(task, metadata, taskidx);
    
    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        
        //check if current block is OK for write operation w.r.t util restriction
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        //calc new expected read util : assume invalidated pages are all moved to new block
        for(int i=0;i<task[taskidx].wn;i++){
            //printf("given lpa : %d", lpas[i]);
            cur_read_lat = calib_readlatency(metadata,taskidx,cur_read_lat,metadata->pagemap[lpas[i]],cur->idx*PPB);
        }
        ru = cur_read_lat * (float)task[taskidx].rn / (float)task[taskidx].rp;
        
        //calc expected gc util : assume that target block joined candidate pool with minimum invalids.
        gcu = __calc_gcu(&(task[taskidx]),MINRC,0,cur_state,cur_state);
        //printf("block %d, ru+gcu : %f\n",cur->idx,ru+gcu);
        //summate read util + GC util. compare with optimal block.
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated\n",cur->idx);
        } else {
            printf("block %d is passed\n",cur->idx);
        }
        //iterate through all blocks and check thier values.
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        
        //check if current block is OK for write operation w.r.t util restriction
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        //calc new expected read util : assume invalidated pages are all moved to new block
        for(int i=0;i<task[taskidx].wn;i++){
            cur_read_lat = calib_readlatency(metadata,taskidx,cur_read_lat,metadata->rmap[lpas[i]],cur->idx*PPB);
        }
        ru = cur_read_lat * (float)task[taskidx].rn / (float)task[taskidx].rp;
        
        //calc expected gc util : assume that target block joined candidate pool with minimum invalids.
        gcu = __calc_gcu(&(task[taskidx]),MINRC,0,cur_state,cur_state);
        printf("block %d, ru+gcu : %f\n",cur->idx,ru+gcu);
        //summate read util + GC util. compare with optimal block.
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated\n",cur->idx);
        } else {
            printf("block %d is passed\n",cur->idx);
        }
        //iterate through all blocks and check thier values.
        cur = cur->next;
    }//!writelist search end

    if(best_idx == -1){
        cur = write_head->head;
        if(cur->fpnum == 0){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_writeweighted(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* lpas, int write_start_idx){
    //use expected read utilization & GC utilization to caculate value for each flash block

    //params
    int cur_state;
    float ru, gcu, weighted_r, weighted_gc;
    block* cur;
    float cur_best_util = 1.0;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);

    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        weighted_r = calc_weightedread(task,metadata,cur,taskidx,lpas);
        weighted_gc = calc_weightedgc(task,metadata,cur,taskidx,lpas,write_start_idx,OP);
        ru = (float)task[taskidx].rn * weighted_r / (float)task[taskidx].rp;
        gcu = weighted_gc / (float)task[taskidx].gcp;
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        } else {
            printf("block %d is passed, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        weighted_r = calc_weightedread(task,metadata,cur,taskidx,lpas);
        weighted_gc = calc_weightedgc(task,metadata,cur,taskidx,lpas,write_start_idx,OP);
        ru = task[taskidx].rn * weighted_r / task[taskidx].rp;
        gcu = weighted_gc / task[taskidx].gcp;
        if(cur_best_util >= ru+gcu){
            cur_best_util = ru+gcu;
            best_idx = cur->idx;
            printf("block %d is updated, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        } else {
            printf("block %d is passed, wr : %f, ru : %f, wgc : %f, wgcu : %f, util : %f\n",weighted_r, ru, weighted_gc, gcu, cur->idx,ru+gcu);
        }
        cur = cur->next;
    }//!writelist search end

    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_write_taskfixed(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head){
    //params
    block* cur;
    int best_state;
    int cur_state;
    int best_idx = -1;
    int yng = get_blockstate_meta(metadata,YOUNG);
    int old = get_blockstate_meta(metadata,OLD);
    
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        //choose a block if block state is younger (task 0,1) || state is older (task 2)
        if(taskidx == 0 || taskidx == 1){
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (taskidx == 2){
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!fblist search end

    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        //choose a block if state is younger (task 0,1) || state is older(task2)
        if(taskidx == 0 || taskidx == 1){
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (taskidx == 2){
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        //printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_write_hotness(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas,int idx){
    block* cur;
    int best_state,  cur_state;
    int avg_w_cnt;
    int avg_r_cnt;
    int hotness = -1;
    int best_idx = -1;
    int logi_pg = (int)((1.0-OP)*(float)NOP);
    int old = get_blockstate_meta(metadata,OLD);
    avg_w_cnt = (int)((float)metadata->tot_write_cnt / (float)(logi_pg));
    avg_r_cnt = (int)((float)metadata->tot_read_cnt / (float)(logi_pg));
    
    //distinguish hotness first
    if(metadata->read_cnt[w_lpas[idx]] >= avg_r_cnt || metadata->write_cnt[w_lpas[idx]] >= avg_w_cnt){    
        hotness = 1;
    }
    //init cur if possible
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state),cur->idx,w_lpas)==-1){
        //if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        if(hotness == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else {
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }

        cur = cur->next;
    }//!fblist search end

    //init cur if possible
    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state),cur->idx,w_lpas)==-1){
        //if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        } 
        if(hotness == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else {
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        //printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    //printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
    return best_idx;
}

int find_write_hotness_motiv(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int lpa, int policy){
    block* cur;
    int best_state,  cur_state;
    int avg_w_cnt;
    int avg_r_cnt;
    int hotness = -1;
    int best_idx = -1;
    int logi_pg = (int)((1.0-OP)*(float)NOP);
    int old = get_blockstate_meta(metadata,OLD);
	int youngest_writable = MAXPE+1, oldest_writable = 0;
    avg_w_cnt = (int)((float)metadata->tot_write_cnt / (float)(logi_pg));
    avg_r_cnt = (int)((float)metadata->tot_read_cnt / (float)(logi_pg));
    
    //distinguish hotness first
    if(metadata->read_cnt[lpa] >= avg_r_cnt || metadata->write_cnt[lpa] >= avg_w_cnt){    
        hotness = 1;
    }
    //init cur if possible
    cur = fblist_head->head;
    if(cur != NULL){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        if(policy == 8){
            if(hotness == 1){
                //find youngest block possible
                if(metadata->state[cur->idx] < best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] >= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 9){
            if(hotness == 1){
                //find oldest block possible
                if(metadata->state[cur->idx] > best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] <= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 10){
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (policy == 11){
            if(metadata->state[cur->idx] <= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
	        if(metadata->state[cur->idx] < youngest_writable){
		        youngest_writable = metadata->state[cur->idx];
	        }
	        if(metadata->state[cur->idx] > oldest_writable){
		        oldest_writable = metadata->state[cur->idx];
	        }
        }
        cur = cur->next;
    }//!fblist search end

    //init cur if possible
    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        /* select block */
        cur_state = metadata->state[cur->idx];
        if(find_util_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]), cur_state) )== -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        
        if(policy == 8){
            if(hotness == 1){
                //find youngest block possible
                if(metadata->state[cur->idx] < best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] >= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 9){
            if(hotness == 1){
                //find oldest block possible
                if(metadata->state[cur->idx] > best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } else {
                if(metadata->state[cur->idx] <= best_state){
                    best_state = metadata->state[cur->idx];
                    best_idx = cur->idx;
                }
            } 
        } else if (policy == 10){
            if(metadata->state[cur->idx] >= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (policy == 11){
            if(metadata->state[cur->idx] <= best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
	        if(metadata->state[cur->idx] < youngest_writable){
		        youngest_writable = metadata->state[cur->idx];
	        }
	        if(metadata->state[cur->idx] > oldest_writable){
		        oldest_writable = metadata->state[cur->idx];
	        }
        }
        cur = cur->next;
    }//!writelist search end

    //edge case handling(all alloc fail)
    if(best_idx == -1){
        printf("wnum, fbnum : %d, %d\n",write_head->blocknum,fblist_head->blocknum);
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        best_idx = cur->idx;
        return best_idx;
    }
    
    //printf("best block : %d, state : %d,oldest_writable:%d,youngest_writable:%d\n",
	//	    best_idx,metadata->state[best_idx],oldest_writable,youngest_writable);
    //print_writeblock_profile(fps[taskidx+tasknum],cur_cp,metadata,fblist_head,write_head,lpa,best_idx,-1,-1,-1.0, -1, -1);

    return best_idx;
}


int find_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, int flag){
    //params
    int* candidate_arr = (int*)malloc(sizeof(int)*(write_head->blocknum+fblist_head->blocknum));
    int* cand_state_arr = (int*)malloc(sizeof(int)*(write_head->blocknum+fblist_head->blocknum));
    int old = get_blockstate_meta(metadata,OLD);
    int candidate_num = 0;
    int cur_state, temp;
    int blockidx;
    int res;
    int type;
    int rank;
    int youngest_writable = MAXPE+1, oldest_writable = 0;
    int hotspace = (int)((float)(task[taskidx].addr_ub - task[taskidx].addr_lb)*task[taskidx].sploc);
    int _offset = (int)((float)(task[taskidx].addr_ub - task[taskidx].addr_lb)*task[taskidx].sploc / 2.0);
    int whot_bound = task[taskidx].addr_lb;
    int rhot_bound = task[taskidx].addr_lb + _offset;
    int highrank_lpa_counts = 0;
    int highestrank_lpa_counts = 0;
    double proportion;
    //params for intensity comparison.
    int whotspace = 0, rhotspace = 0;
    int w_hot = 0, r_hot = 0;
    double w_weight = 0.0, r_weight = 0.0, gc_weight = 0.0;
    double w_gradient, r_gradient, gc_gradient;
    double w_intensity, r_intensity, gc_intensity;
    double max_intensity;

    //init, find & sort candidate block list.
    block* cur = NULL;
    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
        //if(0){
            cur = cur->next;
            continue;
        }
        candidate_arr[candidate_num] = cur->idx;
        candidate_num++;
        if(youngest_writable > metadata->state[cur->idx]){
            youngest_writable = metadata->state[cur->idx];
        }
        if(oldest_writable < metadata->state[cur->idx]){
            oldest_writable = metadata->state[cur->idx];
        }
        cur = cur->next;
    }
    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
        //if(0){
            cur = cur->next;
            continue;
        }
        candidate_arr[candidate_num] = cur->idx;
        candidate_num++;
        if(youngest_writable > metadata->state[cur->idx]){
            youngest_writable = metadata->state[cur->idx];
        }
        if(oldest_writable < metadata->state[cur->idx]){
            oldest_writable = metadata->state[cur->idx];
        }
        cur = cur->next;
    }
    
    
    //EDGECASE: when candidate num is 0 (no feasible block)
    if(candidate_num == 0){
        //ignore find_write_safe and add candidate block.
        cur = write_head->head;
        while(cur!=NULL){
            candidate_arr[candidate_num] = cur->idx;
            candidate_num++;
            if(youngest_writable > metadata->state[cur->idx]){
                youngest_writable = metadata->state[cur->idx];
            }
            if(oldest_writable < metadata->state[cur->idx]){
                oldest_writable = metadata->state[cur->idx];
            }
            cur = cur->next;
        }
        cur = fblist_head->head;
        while(cur!=NULL){
            candidate_arr[candidate_num] = cur->idx;
            candidate_num++;
            
            if(youngest_writable > metadata->state[cur->idx]){
                youngest_writable = metadata->state[cur->idx];
            }
            if(oldest_writable < metadata->state[cur->idx]){
                oldest_writable = metadata->state[cur->idx];
            }
            cur = cur->next;
        }
    }
    //!EDGECASE

    for(int i=candidate_num-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(metadata->state[candidate_arr[j]] > metadata->state[candidate_arr[j+1]]){
                temp = candidate_arr[j];
                candidate_arr[j] = candidate_arr[j+1];
                candidate_arr[j+1] = temp;
            }
        }
    }
    //printf("addr_lb : %d, addr_ub :%d, addr slize : %d, sploc : %f\n",task[taskidx].addr_lb, task[taskidx].addr_ub, task[taskidx].addr_ub - task[taskidx].addr_lb, task[taskidx].sploc);
    //printf("whot_bound : %d, hotspace : %d, offset : %d, rhot_bound : %d\n",whot_bound,hotspace,_offset,rhot_bound);
    
    //define weight
    /*
    if(w_lpas[idx] < whot_bound + hotspace && w_lpas[idx] > whot_bound){
        whotspace = 1;
    }
    if(w_lpas[idx] < rhot_bound + hotspace && w_lpas[idx] > rhot_bound){
        rhotspace = 1;
    }
    if(whotspace == 1){
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)hotspace) * (double)task[taskidx].tploc;
    } else {
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)task[taskidx].tploc);  
    }
    if(rhotspace == 1){
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)hotspace) * task[taskidx].tploc;
    } else { 
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)task[taskidx].tploc);  
    }
    gc_weight = w_weight / (double)MINRC;
    */

    
    w_weight = (double)metadata->write_cnt[w_lpas[idx]] / (double)cur_cp;
    r_weight = (double)metadata->read_cnt[w_lpas[idx]] / (double)cur_cp;
    gc_weight = w_weight / (double)MINRC;
    if(cur_cp == 0){
        w_weight = 0.0;
        r_weight = 0.0;
        gc_weight = 0.0;
    }
    //printf("[onlineweight]%lf,%lf,%lf\n",w_weight,r_weight,gc_weight);
    
    //check intensity
    w_gradient = (double)(ENDW - STARTW) / (double)MAXPE;
    r_gradient = (double)(ENDR - STARTR) / (double)MAXPE;
    gc_gradient = (double)MINRC * (w_gradient + r_gradient) + (double)(ENDE-STARTE)/(double)MAXPE;
    w_intensity = w_weight * w_gradient;
    if(w_intensity < 0.0){
        w_intensity = -w_intensity;
    }
    r_intensity = r_weight * r_gradient;
    gc_intensity = gc_weight * gc_gradient;
    max_intensity = find_max_double(w_intensity,r_intensity,gc_intensity);
    if(w_intensity == 0.0 && r_intensity == 0.0 && gc_intensity == 0.0){
        max_intensity = 0.0;
    }
    //printf("[intensity]%lf,%lf,%lf\n",w_intensity,r_intensity,gc_intensity);
    
    //using pre-defined proportion, find a proper block index.
    /*
    if(max_intensity == w_intensity){
        //printf("w focused\n");
        if (whotspace == 1){
            if(flag == 12){
                blockidx = candidate_num - (int)(w_prop[taskidx*2] * candidate_num);
            } else if (flag == 13){
                blockidx = (int)(w_prop[taskidx*2] * candidate_num);
            } else {
                printf("unknown flag, aborting\n");
                abort();
            }
            printf("[W]hot part of task %d, prop : %f, idx : %d\n",taskidx,w_prop[taskidx*2],blockidx);
        } else if (whotspace == 0){
            if(flag == 12){
                blockidx = candidate_num - (int)(w_prop[taskidx*2+1] * candidate_num);
            } else if (flag == 13){
                blockidx = (int)(w_prop[taskidx*2+1] * candidate_num);
            } else {
                printf("unknown flag, aborting\n");
                abort();
            }
            printf("[W]cold part of task %d, prop : %f, idx : %d\n",taskidx,w_prop[taskidx*2+1],blockidx);
        }
        printf("w focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    else if (max_intensity == r_intensity){
        //printf("r focused\n");
        if (rhotspace == 1){
            blockidx = (int)(r_prop[taskidx*2] * candidate_num);
            printf("[R]hot part of task %d, prop : %f, idx : %d\n",taskidx,r_prop[taskidx*2],blockidx);
        } else if (rhotspace == 0){
            blockidx = (int)(r_prop[taskidx*2+1] * candidate_num);
            printf("[R]cold part of task %d, prop : %f, idx : %d\n",taskidx,r_prop[taskidx*2+1],blockidx);
        }
        printf("r focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    else if (max_intensity == gc_intensity){
        //printf("gc focused\n");
        if (whotspace == 1){
            blockidx = (int)(gc_prop[taskidx*2]*candidate_num);
            printf("[GC]hot part of task %d, prop : %f, idx : %d\n",taskidx,gc_prop[taskidx*2],blockidx);
        } else if (whotspace == 0){
            blockidx = (int)(gc_prop[taskidx*2+1]*candidate_num);
            printf("[GC]cold part of task %d, prop : %f, idx : %d\n",taskidx,gc_prop[taskidx*2+1],blockidx);
        }
        printf("gc focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    */
    
    //calculating proportions in online manner.
    /*
    if(max_intensity == w_intensity){
        for(int i=0;i<max_valid_pg;i++){
            if (metadata->write_cnt[i]>metadata->write_cnt[w_lpas[idx]]){
                highrank_lpa_counts += metadata->write_cnt[i];
            }
        }
        blockidx = (int)((double)candidate_num*(1.0 - (double)highrank_lpa_counts / (double)metadata->tot_write_cnt));
        if(metadata->tot_write_cnt == 0){
            blockidx = 0;
        }
        //printf("highrank count : %d, tot_write_cnt : %d\n",highrank_lpa_counts,metadata->tot_write_cnt);
        //printf("w focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    else if (max_intensity == r_intensity){
        for(int i=0;i<max_valid_pg;i++){
            if (metadata->read_cnt[i]>metadata->read_cnt[w_lpas[idx]]){
                highrank_lpa_counts += metadata->read_cnt[i];
            }
        }
        blockidx = (int)((double)candidate_num*((double)highrank_lpa_counts / (double)metadata->tot_read_cnt));
        if(metadata->tot_read_cnt == 0){
            blockidx = 0;
        }
        printf("highrank count : %d, tot_read_cnt : %d\n",highrank_lpa_counts,metadata->tot_read_cnt);
        printf("r focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    else if (max_intensity == gc_intensity){
        for(int i=0;i<max_valid_pg;i++){
            if (metadata->write_cnt[i]>metadata->write_cnt[w_lpas[idx]]){
                highrank_lpa_counts += metadata->write_cnt[i];
            }
        }
        blockidx = (int)((double)candidate_num*((double)highrank_lpa_counts / (double)metadata->tot_write_cnt));
        if(metadata->tot_write_cnt == 0){
            blockidx = 0;
        }
        printf("highrank count : %d, tot_write_cnt : %d\n",highrank_lpa_counts,metadata->tot_write_cnt);
        printf("gc focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    */
    //proportion calculation method 2.
    rank = 1;
    if(max_intensity == w_intensity){
        type = WR;
	    for(int i=0;i<max_valid_pg;i++){
		    if(metadata->write_cnt[i] > highestrank_lpa_counts){
			    highestrank_lpa_counts = metadata->write_cnt[i];
		    }
            if(metadata->write_cnt[i] > metadata->write_cnt[w_lpas[idx]]){
                rank++;
            }
	    }
        //proportion = (double)metadata->write_cnt[w_lpas[idx]]/(double)highestrank_lpa_counts;
        proportion = 1.0 - (double)rank/(double)max_valid_pg;
	    blockidx = (int)((double)candidate_num*proportion);
	    if(highestrank_lpa_counts == 0){
		    blockidx = 0;
	    }
    }
    else if(max_intensity == r_intensity){
        type = RD;
	    for(int i=0;i<max_valid_pg;i++){
		    if(metadata->read_cnt[i] > highestrank_lpa_counts){
			    highestrank_lpa_counts = metadata->read_cnt[i];
		    }
            if(metadata->read_cnt[i] > metadata->read_cnt[w_lpas[idx]]){
                rank++;
            }
	    }
        //proportion = 1.0 - (double)metadata->read_cnt[w_lpas[idx]]/(double)highestrank_lpa_counts;
	    proportion = (double)rank / (double)max_valid_pg;    
	    blockidx = (int)((double)candidate_num*proportion);
	    if(highestrank_lpa_counts == 0){
		    blockidx = 0;
	    }
        //printf("highest : %d, cur : %d\n",highestrank_lpa_counts,metadata->read_cnt[w_lpas[idx]]);
        //printf("[2]r focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }
    else if(max_intensity == gc_intensity){
        type = GC;
	    for(int i=0;i<max_valid_pg;i++){
		    if(metadata->write_cnt[i] > highestrank_lpa_counts){
			    highestrank_lpa_counts = metadata->write_cnt[i];
		    }
            if(metadata->write_cnt[i] > metadata->write_cnt[w_lpas[idx]]){
                rank++;
            }
	    }
        //proportion = 1.0 - (double)metadata->write_cnt[w_lpas[idx]]/(double)highestrank_lpa_counts;
	    proportion = (double)rank/(double)max_valid_pg;
	    blockidx = (int)((double)candidate_num*proportion);
	    if(metadata->write_cnt[w_lpas[idx]] == 0){
		    blockidx = candidate_num - 1;
	    }
	    if(highestrank_lpa_counts == 0){
		    blockidx = 0;
	    }
        //printf("highest : %d, cur : %d\n",highestrank_lpa_counts,metadata->write_cnt[w_lpas[idx]]);
        //printf("[2]gc focused, bidx=%d,candnum=%d\n",blockidx,candidate_num);
    }

    //edgecase:: if blockidx == candidate_num(prop == 1.00), return candidate_num-1, a last possible block.
    if(blockidx == candidate_num){
        blockidx = candidate_num-1;
    }

    res = candidate_arr[blockidx];
    //print_writeblock_profile(fps[taskidx+tasknum],cur_cp,metadata,fblist_head,write_head,w_lpas[idx],candidate_arr[blockidx],type,rank,proportion,blockidx,candidate_num);
    free(candidate_arr);
    free(cand_state_arr);
    return res;
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

int find_write_maxinvalid(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long workload_reset_time){

    //params to find lpa rank
    char name[30];
    FILE* cur_lpa_timing_file;
    long cur_lpa_timing;
    int cnt = 0;
    int curfp = metadata->total_fp;
    int old = get_blockstate_meta(metadata,OLD);
#ifndef TIMING_ON_MEM    
    sprintf(name,"./timing/%d.csv",w_lpas[idx]);
    cur_lpa_timing_file = fopen(name,"r");
#endif
    //params to find block
    block* cur;
    block* ret_targ;
    block* wb_new;
    block* fb_ptr;
    block* left_ptr;
    block* right_ptr;
    int yield_pg = 0;
    int youngest = MAXPE;
    int target = -1;
    int longlive = -1;
    int cur_fpnum;
    int* jobnum;
    int** req_per_task;
    long** updateorders;
#ifdef MAXINVALID_RANK_DYN
    //active rank calculation-based method
    if(metadata->cur_rank_info.cur_left_write[taskidx] == 0){
        //if pre-assigned write is finished, re-assign ranks for future N writes.
        //1. find jobs and their corresponding update timing within given interval (cur interval = 500ms)
        jobnum = (int*)malloc(sizeof(int)*tasknum);
        req_per_task = (int**)malloc(sizeof(int*)*(unsigned long)tasknum);
        updateorders = (long**)malloc(sizeof(long*)*(unsigned long)tasknum);
        for(int i=0;i<tasknum;i++){
            jobnum[i] = 0;
        }

        _get_jobnum_interval(cur_cp,9000000,task,tasknum,jobnum);
        for(int i=0;i<tasknum;i++){
            req_per_task[i] = (int*)malloc(sizeof(int)*(unsigned long)jobnum[i]*(unsigned long)task[i].wn);
            updateorders[i] = (long*)malloc(sizeof(long)*(unsigned long)jobnum[i]*(unsigned long)task[i].wn);
        }
        _get_write_reqs(w_workloads,tasknum,taskidx,task,w_lpas,9000000,jobnum,req_per_task);
        _get_write_updateorder(jobnum,req_per_task,tasknum,task,metadata,updateorders);
    
        //1-1. put update timing values in single array
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
        
        //2. sort update timing array and find bounds for each rank
        long* bounds = (long*)malloc(sizeof(long)*(metadata->ranknum));
        int cluster_cur = 0;
        int member_num = 0;
        int req_per_rank = reqnum / (metadata->ranknum+1);
        printf("reqnum : %d, req_per_rank : %d, ranknum : %d\n",reqnum,req_per_rank,metadata->ranknum);
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
        //2-1. save current bound info on metadata(note that metadata->rank_bound[0] = 0)
        metadata->rank_bounds[0] = 0;
        for(int i=0;i<metadata->ranknum;i++){
            metadata->rank_bounds[i+1] = bounds[i];
        }
        
        //3. update cur_rank_info structure in metadata
        for(int i=0;i<tasknum;i++){
            int record_idx = 0;
            if(metadata->cur_rank_info.cur_left_write[i] != 0){
                //if a task has a leftover rank records, move them to front of rank array.
                for(int j=0;j<metadata->cur_rank_info.cur_left_write[i];j++){
                    //printf("[leftover]cur rec idx : %d\n",record_idx);
                    int offset = metadata->cur_rank_info.tot_ranked_write[i] - metadata->cur_rank_info.cur_left_write[i] + j;
                    metadata->cur_rank_info.ranks_for_write[record_idx] = metadata->cur_rank_info.ranks_for_write[offset];
                    record_idx++;
                }
            }
            metadata->cur_rank_info.cur_left_write[i] += jobnum[i] * task[i].wn;
            metadata->cur_rank_info.tot_ranked_write[i] = metadata->cur_rank_info.cur_left_write[i];
            for(int j=0;j<jobnum[i]*task[i].wn;j++){
                //find rank of each reqs & make a rank record.
                //printf("cur rec idx : %d\n",record_idx);
                metadata->cur_rank_info.ranks_for_write[i][record_idx] = __get_rank_long_dyn(updateorders[i][j],bounds,metadata->ranknum);
                record_idx++;
            }
        }
        //4. free malloc spaces
        free(jobnum);
        for(int i=1;i<tasknum;i++){
            free(req_per_task[i]);
            free(updateorders[i]);
        }
        free(req_per_task);
        free(updateorders);
        free(invorders_sort);
        free(bounds);
    }

    //[1]get rank info from metadata & update left write
    int offset = metadata->cur_rank_info.tot_ranked_write[taskidx] - metadata->cur_rank_info.cur_left_write[taskidx];
    int cur_rank = metadata->cur_rank_info.ranks_for_write[taskidx][offset];
    metadata->cur_rank_info.cur_left_write[taskidx] -= 1;
    //printf("[%ld]cur_rank : %d\n",cur_cp,cur_rank);
    //[2]find corresponding block
    cur = write_head->head;
    while(cur != NULL){ 
        if(cur->wb_rank == cur_rank){
            return cur->idx;
        }
        cur = cur->next;
    }
    //if block not in wblist, try getting a new block
    int ret_b_idx;
    if(cur == NULL){
        int temp = find_block_in_list(metadata,fblist_head,YOUNG);
        //if block found in fblist, append to write block & return idx
        if (temp != -1){
            block* wb_new = ll_remove(fblist_head,temp);
            wb_new->wb_rank = cur_rank;
            ll_append(write_head,wb_new);
            //printf("[2]rank : %d, wbrank : %d\n",rank,wb_new->wb_rank);
            return wb_new->idx;
        }
        //if block not in fblist, do edge case handling
        else{
            cur = write_head->head;
            int offset = abs_int(cur->wb_rank - cur_rank);
            ret_b_idx = cur->idx;
            while(cur != NULL){
                if(offset >= abs_int(cur->wb_rank - cur_rank)){
                    offset = abs_int(cur->wb_rank - cur_rank);
                    ret_b_idx = cur->idx;
                }
                cur = cur->next;
            }
            //printf("[e]rank : %d alloc to other block...\n",rank);
            return ret_b_idx;
        }
    }
    //!end of active rank calculation-based method
#endif

    //overhead measurement values
    struct timeval a;
    struct timeval b;
    //find out current lpa's update order 
    gettimeofday(&a,NULL);
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
    gettimeofday(&b,NULL);
    gettimeofday(&a,NULL);
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
    gettimeofday(&b,NULL);
#ifdef MAXINVALID_RANK_STAT
    int rank = __get_rank(cnt,metadata);
    int ret_b_idx;
    cur = write_head->head;
    //find a corresponding rank block in wblist 
    while(cur != NULL){ 
        if(cur->wb_rank == rank){
            //printf("[1]rank : %d, wbrank : %d\n",rank,cur->wb_rank);
            return cur->idx;
        }
        cur = cur->next;
    }
    //if block not in wblist, try getting a new block
    if(cur == NULL){
        int temp = find_block_in_list(metadata,fblist_head,YOUNG);
        //if block found in fblist, append to write block & return idx
        if (temp != -1){
            block* wb_new = ll_remove(fblist_head,temp);
            wb_new->wb_rank = rank;
            ll_append(write_head,wb_new);
            //printf("[2]rank : %d, wbrank : %d\n",rank,wb_new->wb_rank);
            return wb_new->idx;
        }
        //if block not in fblist, do edge case handling
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
            //printf("[e]rank : %d alloc to other block...\n",rank);
            return ret_b_idx;
        }
    }
    printf("[MAXINV]write block alloc error!\n");
    abort();
#endif
#ifdef MAXINVALID_RANK_FIXED
    gettimeofday(&a,NULL);
    if(longlive == 1){//edgecase:: free page is not enough to handle target lpa
        while(fblist_head->blocknum != 0){
            youngest = MAXPE;
            target = -1;
            //search through free block list, finding a youngest block, 
            fb_ptr = fblist_head->head;
            while(fb_ptr != NULL){
                if(youngest > metadata->state[fb_ptr->idx]){
                    target = fb_ptr->idx;
                    youngest = metadata->state[fb_ptr->idx];

                }
                fb_ptr = fb_ptr->next;
            }
            //append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        //give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                gettimeofday(&b,NULL);
                //printf("[3]%d\n",__calc_time_diff(a,b));
                return cur->idx;    
            }
            cur = cur->next;
        }
        longlive = 0;
    }
    
    //write block list is arranged in descending order. write the page w.r.t its update order.
    yield_pg = 0;
    cur = write_head->head;
    while(cur != NULL){
        yield_pg += cur->fpnum;
        if(yield_pg > cnt){
            //if current block has enough page to satisfy update order,
            break;
        } else {
            //search next if not.
            cur = cur->next;
        }
        //printf("[W]yieldpg : %d\n",yield_pg);
    }
    if(cur == NULL){//no feasible block in writeblock list.
        while(fblist_head->blocknum != 0){
            youngest = MAXPE;
            target = -1;
            //search through free block list, finding a youngest block, 
            fb_ptr = fblist_head->head;
            while(fb_ptr != NULL){
                if(youngest > metadata->state[fb_ptr->idx]){
                    target = fb_ptr->idx;
                    youngest = metadata->state[fb_ptr->idx];
                }
                fb_ptr = fb_ptr->next;
            }
            //append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
            yield_pg += wb_new->fpnum;
            if(yield_pg > cnt){
                cur = wb_new;
                break;
            } else {
                //do nothing. 
                //repeat while loop until free block runs out.
            }
            //printf("[FB]yieldpg : %d\n",yield_pg);
        }
    }
    if(cur == NULL){//edgecase:: not found corresponding block
        while(fblist_head->blocknum != 0){
            //search through free block list, finding a youngest block,
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
            //append it, and see if freepage is enough to handle current write.
            wb_new = ll_remove(fblist_head,target);
            ll_append(write_head,wb_new);
        }
        //give last write block.
        cur = write_head->head;
        while(cur != NULL){
            if (cur->next == NULL){
                gettimeofday(&b,NULL);
                //printf("[3]%d\n",__calc_time_diff(a,b));
                return cur->idx;    
            }
            cur = cur->next;
        }
        longlive = 0;
    }

    //initiate findwritesafe() on current block, and see if allocation leads to utilization overflow.
    if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[cur->idx]),cur->idx,w_lpas) == 0){
        gettimeofday(&b,NULL);
        //printf("[3]%d\n",__calc_time_diff(a,b));
        return cur->idx;
    } else {
        //traverse left and right to find suitable block
        left_ptr = cur->prev;
        right_ptr = cur->next;
        while((left_ptr != NULL) || (right_ptr != NULL)){
            if(left_ptr != NULL){
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[left_ptr->idx]),cur->idx,w_lpas) == 0){
                    gettimeofday(&b,NULL);
                    //printf("[3]%d\n",__calc_time_diff(a,b));
                    return left_ptr->idx;
                } else{
                    left_ptr = left_ptr->prev;
                }
            }
            if(right_ptr != NULL){
                if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),metadata->state[right_ptr->idx]),cur->idx,w_lpas) == 0){
                    gettimeofday(&b,NULL);
                    //printf("[3]%d\n",__calc_time_diff(a,b));
                    return right_ptr->idx;
                } else{
                    right_ptr = right_ptr->next;
                }
            }
        }
    }
    //if findwritesafe() fails on all block, just return cur.
    gettimeofday(&b,NULL);
    //printf("[3]%d\n",__calc_time_diff(a,b));
    return cur->idx;
#endif
}

int find_write_maxinv_prac(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx, long workload_reset_time){
    if(metadata->left_rankwrite_num == 0){
        int req_per_rank = LIFESPAN_WINDOW / (metadata->ranknum+1);
        int cur_num = 0;
        int cluster_cur = 1;

        //0-1. sort data_lifespan_window
        qsort(metadata->data_lifespan,LIFESPAN_WINDOW,sizeof(long),compare);
        //0-2. assign interval for each rank
        for(int i=0;i<LIFESPAN_WINDOW;i++){
            cur_num++;
            if(cur_num == req_per_rank){
                metadata->rank_bounds[cluster_cur] = metadata->data_lifespan[i];
                cluster_cur++;
                cur_num = 0;
            }
            if(cluster_cur == metadata->ranknum+1){
                break;
            }
        }
        //printf("[cur_bounds]\n");
        //for(int i=0;i<metadata->ranknum+1;i++){
        //    printf("%ld, %d\n",metadata->rank_bounds[i],metadata->rank_write_count[i]);
        //}
        //sleep(1);
        //0-3. reset data_lifespan_window and window_cnt
        for(int i=0;i<LIFESPAN_WINDOW;i++){
            metadata->data_lifespan[i] = 0L;
        }
        for(int i=0;i<metadata->ranknum+1;i++){
            metadata->rank_write_count[i] = 0;
        }
        metadata->left_rankwrite_num = LIFESPAN_WINDOW;
        metadata->lifespan_record_num = 0;
    }
    //1. calculate current data's lifespan
    long real_lifespan;
    long cur_lifespan = cur_cp - metadata->recent_update[w_lpas[idx]];
    long avg_lifespan = metadata->avg_update[w_lpas[idx]] * (long)metadata->write_cnt[w_lpas[idx]] + cur_lifespan;
    avg_lifespan = avg_lifespan / (long)(metadata->write_cnt[w_lpas[idx]] + 1);
    if(update_cnt[w_lpas[idx]] > metadata->write_cnt_per_cycle[w_lpas[idx]]+1){
        real_lifespan = lpa_update_timing[w_lpas[idx]][metadata->write_cnt_per_cycle[w_lpas[idx]]+1]-cur_cp;
    }
    else {
        real_lifespan = WORKLOAD_LENGTH; 
    }
    long expected_lifespan = cur_lifespan;
    //printf("lifespan record: cur:%ld, avg:%ld, real:%ld\n",cur_lifespan,avg_lifespan,real_lifespan);
    //printf("cur cp : %ld, recent : %ld, next : %ld\n",cur_cp, metadata->recent_update[w_lpas[idx]],lpa_update_timing[w_lpas[idx]][metadata->write_cnt_per_cycle[w_lpas[idx]]+1]);
    //2. get rank of current write
    int cur_rank = 0;
    for(int i=0;i<metadata->ranknum;i++){
        if(expected_lifespan < metadata->rank_bounds[i+1] && expected_lifespan >= metadata->rank_bounds[i]){
            cur_rank = i;
        }
    }
    if(expected_lifespan >= metadata->rank_bounds[metadata->ranknum]){
        cur_rank = metadata->ranknum;
    }
    metadata->rank_write_count[cur_rank]++;
    //printf("[%d]expected_lifespan : %ld, cur_rank : %d, cur - recent : %ld - %ld\n",w_lpas[idx],expected_lifespan, cur_rank, cur_cp, metadata->recent_update[w_lpas[idx]]);
    //3. assign corresponding block
    block* cur = write_head->head;
    while(cur != NULL){ 
        if(cur->wb_rank == cur_rank){
            return cur->idx;
        }
        cur = cur->next;
    }
    //if block not in wblist, try getting a new block
    int ret_b_idx;
    if(cur == NULL){
        int temp = find_block_in_list(metadata,fblist_head,YOUNG);
        //if block found in fblist, append to write block & return idx
        if (temp != -1){
            block* wb_new = ll_remove(fblist_head,temp);
            wb_new->wb_rank = cur_rank;
            ll_append(write_head,wb_new);
            return wb_new->idx;
        }
        //if block not in fblist, do edge case handling
        else{
            cur = write_head->head;
            int offset = abs_int(cur->wb_rank - cur_rank);
            ret_b_idx = cur->idx;
            while(cur != NULL){
                if(offset >= abs_int(cur->wb_rank - cur_rank)){
                    offset = abs_int(cur->wb_rank - cur_rank);
                    ret_b_idx = cur->idx;
                }
                cur = cur->next;
            }
            return ret_b_idx;
        }
    }
}