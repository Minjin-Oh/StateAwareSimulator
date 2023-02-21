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

//Determine rank of current lpa using task & locality information.
//assuming that locality is fixed, rank of lpa is determined statically.
void _find_rank_lpa(rttask* tasks, int tasknum){
    //calculate w/r intensity
    //even idx 2*i = intensity of hot proportion of task i.
    //odd idx (2*i)+1 = intensity of cold proportion of task i.
    
    //init params
    int hotspace = (int)((float)(tasks[0].addr_ub - tasks[0].addr_lb)*sploc);
    double w_gradient = (double)(ENDW - STARTW) / (double)MAXPE;
    double r_gradient = (double)(ENDR - STARTR) / (double)MAXPE;
    int temp_int;
    double temp_d;

    //init intensity arrays
    double write_intensity[tasknum * 2];
    int write_idx[tasknum*2];
    double read_intensity[tasknum * 2];
    int read_idx[tasknum*2];
    double wi_sum_arr[tasknum * 2];
    double ri_sum_arr[tasknum * 2];
    double w_proportion[tasknum * 2];
    double r_proportion[tasknum * 2];
    double w_weight_sum = 0.0, r_weight_sum = 0.0, upper_weight = 0.0;
    
    //init index and intensity values
    //intensity is # of access per unit time, calculated as task (I/O speed * address pick probability) 
    for(int i=0;i<tasknum;i++){
        write_intensity[2*i] =   (double)tasks[i].wn / (double)tasks[i].wp * (1.0/(double)hotspace) * (double)tploc;
        write_intensity[(2*i)+1] =  (double)tasks[i].wn / (double)tasks[i].wp * (1.0/(double)((tasks[i].addr_ub - tasks[i].addr_lb - hotspace))) * (1.0 - (double)tploc);
        read_intensity[2*i] = (double)tasks[i].rn / (double)tasks[i].rp * (1/(double)hotspace) * tploc;
        read_intensity[(2*i)+1] = (double)tasks[i].rn / (double)tasks[i].rp * (1/(double)((tasks[i].addr_ub - tasks[i].addr_lb - hotspace))) * (1.0 - (double)tploc);
        write_idx[2*i] = 2*i;
        write_idx[2*i+1] = 2*i+1;
        read_idx[2*i] = 2*i;
        read_idx[2*i+1] = 2*i+1;
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

    //check code
    /*
    for(int i=0;i<tasknum*2;i++){
        printf("[W]section : %d, intensity : %.10lf\n",write_idx[i],write_intensity[i]);
    }
    for(int i=0;i<tasknum*2;i++){
        printf("[R]section : %d, intensity : %.10lf\n",read_idx[i],read_intensity[i]);
    }*/
    //with the sorted intensity, calculate proportion of each section.
    //a proportion is calculated as (sum of weight of upper rank lpa section / total sum of weight)
    
    //calculate sum of weight for each section. weight of section is calc as (num of lpa * intensity)
    for(int i=0;i<tasknum*2;i++){
        if(write_idx[i] % 2 == 0){//hotspace
            wi_sum_arr[i] = (double)hotspace * write_intensity[i];
        } else { //coldspace
            wi_sum_arr[i] = (double)(tasks[i/2].addr_ub - tasks[i/2].addr_lb - hotspace) * write_intensity[i];
        }

        if(read_idx[i] % 2 == 0){
            ri_sum_arr[i] = (double)hotspace * read_intensity[i];
        } else {
            ri_sum_arr[i] = (double)(tasks[i/2].addr_ub - tasks[i/2].addr_lb - hotspace) * read_intensity[i];
        }
    }
    /*for(int i=0;i<tasknum*2;i++){
        printf("[W]intensity_sum : %.10lf\n",wi_sum_arr[i]);
    }
    for(int i=0;i<tasknum*2;i++){
        printf("[R]intensity_sum : %.10lf\n",ri_sum_arr[i]);
    }*/

    //calculate total sum of intensity
    for(int i=0;i<tasknum*2;i++){
        w_weight_sum += wi_sum_arr[i];
        r_weight_sum += ri_sum_arr[i];
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
    }
    /*
    for(int i=0;i<tasknum*2;i++){
        printf("[W]proportion(sorted) : %.10lf\n",w_proportion[i]);
    }
    for(int i=0;i<tasknum*2;i++){
        printf("[R]proportion(sorted) : %.10lf\n",r_proportion[i]);
    }*/
    for(int i=0;i<tasknum*2;i++){
        w_prop[write_idx[i]] = w_proportion[i];
        r_prop[read_idx[i]] = r_proportion[i];
        printf("[W]%d-th section : %f\n",write_idx[i],w_proportion[i]);
        printf("[R]%d-th section : %f\n",read_idx[i],r_proportion[i]);
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
    //printf("tot_u : %f, old_tot_u : %f\n",total_u,old_total_u);
    if (total_u <= 1.0){
        return 0;
    } else if (total_u > 1.0){
        return -1;
    }
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
    
    //printf("best block : %d, state : %d\n",best_idx,metadata->state[best_idx]);
   
    return best_idx;
}


int find_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx){
    //params
    int* candidate_arr = (int*)malloc(sizeof(int)*(write_head->blocknum+fblist_head->blocknum));
    int* cand_state_arr = (int*)malloc(sizeof(int)*(write_head->blocknum+fblist_head->blocknum));
    int old = get_blockstate_meta(metadata,OLD);
    int candidate_num = 0;
    int cur_state, temp;
    int blockidx;
    int res;
    int _offset = offset;
    int hotspace = (int)((float)(task[taskidx].addr_ub - task[taskidx].addr_lb)*sploc);
    int whot_bound = task[taskidx].addr_lb;
    int rhot_bound = task[taskidx].addr_lb + _offset;
    
    //temp values for intensity comparison.
    int whotspace = 0, rhotspace = 0;
    int w_hot = 0, r_hot = 0;
    double w_weight = 0.0, r_weight = 0.0;
    double w_gradient, r_gradient;
    

    //init, find & sort candidate block list.
    block* cur = NULL;
    cur = write_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
            cur = cur->next;
            continue;
        }
        candidate_arr[candidate_num] = cur->idx;
        candidate_num++;
        cur = cur->next;
    }
    cur = fblist_head->head;
    while(cur != NULL){
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
            cur = cur->next;
            continue;
        }
        candidate_arr[candidate_num] = cur->idx;
        candidate_num++;
        cur = cur->next;
    }
    for(int i=candidate_num-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(metadata->state[candidate_arr[j]] > metadata->state[candidate_arr[j+1]]){
                temp = candidate_arr[j];
                candidate_arr[j] = candidate_arr[j+1];
                candidate_arr[j+1] = temp;
            }
        }
    }
    //printf("candidate num : %d\n",candidate_num);
    //for(int i=0;i<candidate_num;i++){
    //    printf("idx : %d, cyc : %d\n",candidate_arr[i],metadata->state[candidate_arr[i]]);
    //}    
    //compare the weight of write/read

    //EDGECASE: when candidate num is 0 (no feasible block)
    if(candidate_num == 0){
        cur = write_head->head;
        if(cur == NULL){
            cur = fblist_head->head;
        }
        free(candidate_arr);
        free(cand_state_arr);
        return cur->idx;
    }
    //
    if(w_lpas[idx] < whot_bound + hotspace && w_lpas[idx] > whot_bound){
        whotspace = 1;
    }
    if(w_lpas[idx] < rhot_bound + hotspace && w_lpas[idx] > rhot_bound){
        rhotspace = 1;
    }
    if(whotspace == 1){
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)hotspace) * (double)tploc;
    } else {
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)tploc);  
    }
    if(rhotspace == 1){
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)hotspace) * tploc;
    } else { 
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)tploc);  
    }
    //weight defined
    //code for checking page characteristic
    w_gradient = (double)(ENDW - STARTW) / (double)MAXPE;
    r_gradient = (double)(ENDR - STARTR) / (double)MAXPE;
    
    if (w_gradient * w_weight + r_gradient * r_weight < 0.0){
        w_hot = 1; //prefers old block
    } else if (w_gradient * w_weight + r_gradient * r_weight >= 0.0){
        r_hot = 1; //prefers young block
    }
    
    //using pre-defined proportion, find a proper block index.
    if(w_hot == 1){
        if (whotspace == 1){
            blockidx = (int)(w_prop[taskidx*2] * candidate_num);
            //printf("[W]hot part of task %d, prop : %f, idx : %d\n",taskidx,w_prop[taskidx*2],blockidx);
        } else if (whotspace == 0){
            blockidx = (int)(w_prop[taskidx*2+1] * candidate_num);
            //printf("[W]cold part of task %d, prop : %f, idx : %d\n",taskidx,w_prop[taskidx*2+1],blockidx);
        }
    }
    else if (r_hot == 1){
        if (rhotspace == 1){
            blockidx = candidate_num - (int)(r_prop[taskidx*2] * candidate_num);
            //printf("[R]hot part of task %d, prop : %f, idx : %d\n",taskidx,r_prop[taskidx*2],blockidx);
        } else if (rhotspace == 0){
            blockidx = candidate_num - (int)(r_prop[taskidx*2+1] * candidate_num);
            //printf("[R]cold part of task %d, prop : %f, idx : %d\n",taskidx,r_prop[taskidx*2+1],blockidx);
        }
    }
    //edgecase:: if blockidx == candidate_num(prop == 1.00), return candidate_num-1, a last possible block.
    if(blockidx == candidate_num){
        blockidx = candidate_num-1;
    }
    //printf("w_hot(w_hotspace) = %d(%d), r_hot(r_hotspace) = %d(%d)\n",w_hot,whotspace,r_hot,rhotspace);
    //printf("blockidx = %d, max = %d\n",blockidx,candidate_num);
    
    res = candidate_arr[blockidx];
    //printf("return idx = %d, blockidx = %d\n",res,blockidx);
    free(candidate_arr);
    free(cand_state_arr);
    return res;
}
/*
int find_write_gradient(rttask* task, int taskidx, int tasknum, meta* metadata, bhead* fblist_head, bhead* write_head, int* w_lpas, int idx){
    
    //params
    block* cur;
    int whotspace = 0, rhotspace = 0;       //check if current lpa is in hotspace or not
    int w_hot = 0, r_hot = 0;               //check if current lpa "favors" write or read
    int preference;
    int cur_state, best_state;
    int best_idx = -1;
    int old = get_blockstate_meta(metadata,OLD);
    double w_weight = 0.0, r_weight = 0.0;
    double w_gradient, r_gradient;
    
    //FIXME:: references global variables to specify hotness.
    //printf("sploc : %f, tploc : %f, offset : %d\n",sploc,tploc,offset);
    float _sploc = sploc;
    float _tploc = tploc;
    int _offset = (int)((float)(task[0].addr_ub - task[0].addr_lb)*sploc);
    int hotspace = (int)((float)(task[taskidx].addr_ub - task[taskidx].addr_lb)*_sploc);
    int whot_bound = task[taskidx].addr_lb;
    int rhot_bound = task[taskidx].addr_lb + _offset;

    //printf("whot_bound : %d ~ %d\n",whot_bound, whot_bound+hotspace);
    //printf("rhot bound : %d ~ %d\n",rhot_bound, rhot_bound + hotspace);
    if(w_lpas[idx] < whot_bound + hotspace && w_lpas[idx] > whot_bound){
        whotspace = 1;
    }
    if(w_lpas[idx] < rhot_bound + hotspace && w_lpas[idx] > rhot_bound){
        rhotspace = 1;
    }
    //printf("lpa is %d, w = %d, r = %d\n",w_lpas[idx],whotspace,rhotspace);
    //code for finding write & read weight
    if(whotspace == 1){
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)hotspace) * (double)_tploc;
        //printf("[H]w = %d / %d * (1/%d) * %lf = %lf\n",task[taskidx].wn , task[taskidx].wp, hotspace, _tploc,w_weight);
    } else {
        w_weight = (double)task[taskidx].wn / (double)task[taskidx].wp * (1.0/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)_tploc);  
        //printf("[C]w = %d / %d * (1/%d) * %lf = %lf\n",task[taskidx].wn , task[taskidx].wp, (task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace), 1.0 - _tploc, w_weight);
    }
    if(rhotspace == 1){
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)hotspace) * _tploc;
        //printf("[H]r = %d / %d * (1/%d) * %f = %f\n",task[taskidx].rn , task[taskidx].rp, hotspace, _tploc,r_weight);
    } else { 
        r_weight = (double)task[taskidx].rn / (double)task[taskidx].rp * (1/(double)((task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace))) * (1.0 - (double)_tploc);  
        //printf("[C]r = %d / %d * (1/%d) * %f = %f\n",task[taskidx].rn , task[taskidx].rp, (task[taskidx].addr_ub - task[taskidx].addr_lb - hotspace), 1.0 - _tploc,r_weight);
    }
    //weight defined
    //code for checking page characteristic
    w_gradient = (double)(ENDW - STARTW) / (double)MAXPE;
    r_gradient = (double)(ENDR - STARTR) / (double)MAXPE;
    //printf("w_grad : %lf, w_weight : %lf, r_grad : %lf, r_weight : %lf, calc : %.10lf\n",
    //            w_gradient,w_weight,r_gradient,r_weight,w_gradient * w_weight + r_gradient * r_weight);
    if (w_gradient * w_weight + r_gradient * r_weight < 0.0){
        w_hot = 1; //prefers old block
    } else if (w_gradient * w_weight + r_gradient * r_weight > 0.0){
        r_hot = 1; //prefers young block
    }
    //printf("w_hot = %d, r_hot = %d\n",w_hot,r_hot);
    //characteristic defined

    //select block
    //init best block if possible
    cur = fblist_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        //check if current block violates utilization bound or not
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        if(w_hot == 1){
            //find oldest block possible
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (r_hot == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }
    cur = write_head->head;
    if(cur != NULL && best_idx == -1){
        best_state = metadata->state[cur->idx];
        best_idx = cur->idx;
    }
    while(cur != NULL){
        //check if current block violates utilization bound or not
        cur_state = metadata->state[cur->idx];
        if(_find_write_safe(task,tasknum,metadata,old,taskidx,WR,__calc_wu(&(task[taskidx]),cur_state),cur->idx,w_lpas) == -1){
            //printf("block: %d, util check fail\n",cur->idx);
            cur = cur->next;
            continue;
        }
        if(w_hot == 1){
            //find oldest block possible
            if(metadata->state[cur->idx] > best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        } else if (r_hot == 1){
            //find youngest block possible
            if(metadata->state[cur->idx] < best_state){
                best_state = metadata->state[cur->idx];
                best_idx = cur->idx;
            }
        }
        cur = cur->next;
    }
    //block selection finished
    return best_idx;
}*/