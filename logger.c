#include "stateaware.h"

extern double OP;

FILE* open_file_bycase(int gcflag, int wflag, int rrflag){
    FILE* fp;
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fp = fopen("SA_prof_all.csv","w");
    }
    else if (gcflag == 0 && wflag == 1 && rrflag == 1){
        fp = fopen("SA_prof_case1.csv","w");
    }
    else if (gcflag == 1 && wflag == 0 && rrflag == 1){
        fp = fopen("SA_prof_case2.csv","w");
    }
    else if (gcflag == 1 && wflag == 1 && rrflag == 0){
        fp = fopen("SA_prof_case3.csv","w");
    }
    else if (gcflag == 0 && wflag == 0 && rrflag == 1){
        fp = fopen("SA_prof_case4.csv","w");
    }
    else if (gcflag == 0 && wflag == 1 && rrflag == 0){
        fp = fopen("SA_prof_case5.csv","w");
    }
    else if (gcflag == 1 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_case6.csv","w");
    }
    else if (gcflag == 0 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_none.csv","w");
    }
    else if (gcflag == 2 && wflag == 2 && rrflag == 0){
        fp = fopen("SA_prof_greedy.csv","w");
    }
    else if (gcflag == 3 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_gclimit.csv","w");
    }
    else {
        fp = fopen("SA_prof_motiv.csv","w");
    }
    return fp;
}

FILE* open_file_pertask(int gcflag, int wflag, int rrflag, int tasknum){
    //file per each task + 1 file for hot data distribution profiling
    FILE** fps;
    fps = (FILE**)malloc(sizeof(FILE*)*(tasknum+tasknum+2));
    char name[100];
    if(wflag == 0 && gcflag == 0 && rrflag == -1){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_no_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_no_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_no_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_no_invdist.csv","w");
    } 
    else if(wflag == 11 && gcflag == 0 && rrflag == -1){//dynWL
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_dynWL_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_dynWL_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_dynWL_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_dynWL_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 0 && rrflag ==  0){//staticWL
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_statWL_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_statWL_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_statWL_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_statWL_invdist.csv","w");
    }
    else if(wflag == 11 && gcflag == 0 && rrflag ==  0){//WLcomb
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_WLcomb_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_WLcomb_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_WLcomb_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_WLcomb_invdist.csv","w");
    }
    else if(wflag == 12 && gcflag == 6 && rrflag ==  1){//ours
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_ours_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_ours_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_ours_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_ours_invdist.csv","w");
    }
    else if(wflag == 12 && gcflag == 0 && rrflag == -1){//Wonly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wonly_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wonly_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_wonly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_wonly_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 6 && rrflag == -1){//GConly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gconly_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gconly_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_gconly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_gconly_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 0 && rrflag ==  1){//RRonly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_rronly_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_rronly_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_rronly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_rronly_invdist.csv","w");
    }
    else if(wflag == 12 && gcflag == 6 && rrflag == -1){//W+GC
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wgc_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wgc_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_wgc_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_wgc_invdist.csv","w");
    }
    else if (rrflag == 1 && gcflag == 6 && wflag == 0){//GC+RR
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gcrr_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gcrr_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_gcrr_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_gcrr_invdist.csv","w");
    } 
    else if (gcflag == 0 && wflag == 12 && rrflag == 1){//W+RR
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wrr_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wrr_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_wrr_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_wrr_invdist.csv","w");
    }   
    else {
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_new_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_new_wlog%d.csv",i);
            fps[tasknum+i] = fopen(name,"w");
        }
        fps[tasknum+tasknum] = fopen("prof_new_fbdist.csv","w");
        fps[tasknum+tasknum+1] = fopen("prof_new_invdist.csv","w");
    }
    return fps;
}

void update_read_worst(meta* metadata, int tasknum){
    for(int i=0;i<tasknum;i++){
        metadata->cur_read_worst[i] = 0;
        for(int j=0;j<NOP;j++){
            if(metadata->vmap_task[j] == i){
                int cur_b = j/PPB;
                if(metadata->state[cur_b] >= metadata->cur_read_worst[i]){
                    metadata->cur_read_worst[i] = metadata->state[cur_b];
                }
            }
        }
    }
}

float calc_readlatency(rttask* tasks, meta* metadata, int taskidx){
    //calculated expected read latency using read count history.
    //formula : sum(rc of page p / rc of task t * read exec of page p)
    int ppa, lpa;
    float read_prob;

    int cnt_sum = 0;
    float prob_sum = 0.0;
    float exp_read_latency = 0.0;

    for(int i=tasks[taskidx].addr_lb;i<tasks[taskidx].addr_ub;i++){
        read_prob = (float)metadata->read_cnt[i] / (float)metadata->read_cnt_task[taskidx];
        cnt_sum += metadata->read_cnt[i];
        prob_sum += read_prob;
        exp_read_latency += read_prob * r_exec(metadata->state[metadata->pagemap[i]/PPB]);
    }
    /*
    for(int i=0;i<NOP;i++){
        if(metadata->vmap_task[i]==taskidx && metadata->invmap[i]==0){
            lpa = metadata->rmap[i];
            ppa = metadata->pagemap[lpa];
            //printf("lpa : %d, ppa : %d\n",lpa, ppa);
            read_prob = (float)metadata->read_cnt[lpa] / (float)metadata->read_cnt_task[taskidx];
            cnt_sum += metadata->read_cnt[lpa];
            prob_sum += read_prob;
            exp_read_latency += read_prob * metadata->state[ppa/PPB];
        }
    }
    */
    printf("[debug]cnt, task sum, probability sum : %d, %d ,%f\n",
    cnt_sum,metadata->read_cnt_task[taskidx],prob_sum);
    printf("exp_read_latency : %f\n",exp_read_latency);
    //edge case handling
    if(metadata->read_cnt_task[taskidx] == 0){
        exp_read_latency = (float)STARTR;
    }
    return exp_read_latency;
}

float calib_readlatency(meta* metadata, int taskidx, float cur_exp_lat, int old_ppa, int new_ppa){
    //calibrate expected read latency change resulted from page relocation
    int lpa = metadata->rmap[old_ppa];
    float new_exp_lat = cur_exp_lat;
    //edge case handling
    if (metadata->read_cnt[lpa] == 0 || metadata->read_cnt_task[taskidx]==0){
        //printf(" edge case : no calibration\n");
        return new_exp_lat;
    }
    float read_prob = (float)metadata->read_cnt[lpa] / (float)metadata->read_cnt_task[taskidx];
    //printf(" rmap lpa : %d, old : %d, new : %d\n",lpa,old_ppa,new_ppa);
    new_exp_lat = new_exp_lat - read_prob * metadata->state[old_ppa/PPB] + read_prob * metadata->state[new_ppa/PPB];
    return new_exp_lat;
}

float get_totutil(rttask* tasks, int tasknum, int taskidx, meta* metadata, int old){
    float total_u=0.0;
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    return total_u;
}

float calc_weightedread(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas){
    float weighted_read = 0.0;
    float read_prob;
    for(int i=0;i<tasks[taskidx].wn;i++){
        read_prob = (float)metadata->read_cnt[lpas[i]] / (float)metadata->read_cnt_task[taskidx];
        weighted_read += read_prob * metadata->state[tar->idx];
    }
    return weighted_read;
}   

float calc_weightedgc(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas, int w_start_idx, float OP){
    //calculated expected gc latency using write count history
    
    int ppa,lpa;                  //physical, logical page address
    int write_avg;                //average write count of all valid pages
    float gc_prob;                //a weight value for gc latency
    int b_hot = 0;                //a temperature of block
    int w_left = tasks[taskidx].wn - w_start_idx;
    int blockidx = tar->idx;
    int cur_vp = PPB;
    float exp_gc_latency = 0.0;   //expected gc latency

    //calc gc prob as follows, using write hotness
    write_avg = metadata->tot_write_cnt / (int)((1.0-OP)*(float)NOP);
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[blockidx*PPB+i]==1){
            b_hot += write_avg;
        } else if(metadata->write_cnt[blockidx*PPB+i] > write_avg){
            b_hot += metadata->write_cnt[blockidx*PPB+i];
        }
    }
    if(b_hot == 0.0){
        gc_prob = 0.0;
    } else if ((float)metadata->tot_write_cnt == 0){
        gc_prob = 1.0;   
    } else {
        gc_prob = (float)b_hot / (float)metadata->tot_write_cnt;
    }
    //calc gc latency as follows, using write hotness
    cur_vp -= tar->fpnum;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[blockidx*PPB+i]==1){
            cur_vp--;
        } else if(metadata->write_cnt[blockidx*PPB+i] > write_avg){
            cur_vp--;
        }
    }
    //assume that write left go into target block, within limit of 
    for(int i=w_start_idx;i<tasks[taskidx].wn;i++){
        if(metadata->write_cnt[lpas[i]] + 1 >= write_avg){
            /*do nothing*/
        } else if (metadata->write_cnt[lpas[i]] < write_avg){
            cur_vp++;
        }
    }
    exp_gc_latency = (float)cur_vp*(r_exec(metadata->state[blockidx])+w_exec(0)) + e_exec(metadata->state[blockidx]);

    //return weighted gc latency
    return (gc_prob * exp_gc_latency);
}
float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state, block* cur_wb, bhead* fblist_head, bhead*write_head, int getfp, int gcvalidcount){
    //init params
    int cur_read_worst[tasknum];
    int state_tot = 0;
    double state_avg = 0.0;
    double state_var = 0.0;
    float total_u = 0.0;
    float total_r = 0.0;
    float total_w = 0.0;
    float total_gc = 0.0;
    float total_u_noblock = 0.0;
    //find worst case util w.r.t system-wise worst block
    float worst_util = find_worst_util(tasks,tasknum,metadata);
    //find worst block for each task
    
    for(int k=0;k<tasknum;k++){
        cur_read_worst[k] = 0;
        for(int i=0;i<NOP;i++){
            if(metadata->vmap_task[i] == k){
                int cur_b = i/PPB;
                if(metadata->state[cur_b] >= cur_read_worst[k]){
                    cur_read_worst[k] = metadata->state[cur_b];
                }
            }
        }
        //printf("\n[%d]cur_read_worst : %d\n",k,cur_read_worst[k]);
    }
    //printf("system worst : %d\n",old);
    //find current util w.r.t actual blocks
    
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_w += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_r += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        total_gc += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    for(int i=0;i<NOB;i++){
        state_tot += metadata->state[i];
    }
    state_avg = (double)state_tot / (double)NOB;
    for(int i=0;i<NOB;i++){
        state_var += pow((double)metadata->state[i] - (double)state_avg ,2.0);
    }
    state_var = state_var / NOB;
    state_var = sqrt(state_var);
    total_u_noblock = total_u;
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //print all infos
    fprintf(fp,"%ld,%d, %f,%f,%f,%f,%f,%f, %d,%d, %d,%d,%d,%d, %d,%d, %d,%d, %f,%f,%f, %f, %lf\n",
    cur_cp,taskidx,
    worst_util,total_u, total_u_noblock,
    metadata->runutils[0][taskidx],
    metadata->runutils[1][taskidx],
    metadata->runutils[2][taskidx],
    old,yng,
    cur_gc_idx,cur_gc_state,getfp,gcvalidcount,
    cur_wb->idx,metadata->state[cur_wb->idx],
    fblist_head->blocknum,write_head->blocknum,
    total_w,total_r,total_gc,
    state_avg, state_var); 
    return total_u;
}

void print_hotdist_profile(FILE* fp, rttask* tasks, int cur_cp, meta* metadata, int taskidx, int hotness_rw){
    //if hotness & taskindex is specified, profile the distribution of the task
    int dist[MAXPE];
    int avg_cyc = 0;
    int cyc_tot = 0;
    int task_pg_num = 0;
    int hot_pg_num = 0;
    int lb = 0, ub = MAXPE;
    int logi = (int)((1.0-OP)*(float)NOP);
    int write_avg = metadata->tot_write_cnt / (int)((1.0-OP)*(float)NOP);
    int read_avg = metadata->tot_read_cnt / (int)((1.0-OP)*(float)NOP);
    int phy_pg;

    for(int i=0;i<MAXPE;i++){
        dist[i] = 0;
    }

    if(taskidx != -1){
        //if taskidx is fixed, check where valid data of that task exists
        for(int i=tasks[taskidx].addr_lb;i<tasks[taskidx].addr_ub;i++){
            phy_pg = metadata->pagemap[i];
            dist[metadata->state[phy_pg/PPB]] += 1;
            cyc_tot += metadata->state[phy_pg/PPB];
            task_pg_num += 1;
        }
    } else if (hotness_rw==0) {//rec writehot
        //if taskidx is not fixed, search through all valid pages
        for(int i=0;i<logi;i++){
            phy_pg = metadata->pagemap[i];
            if(metadata->write_cnt[i] >= write_avg){
                dist[metadata->state[phy_pg/PPB]] += 1;
                cyc_tot += metadata->state[phy_pg/PPB];
                hot_pg_num += 1; 
            } 
        }
    } else if (hotness_rw==1){//rec readhot
        for(int i=0;i<logi;i++){
            phy_pg = metadata->pagemap[i];
            if(metadata->read_cnt[i] >= read_avg){
                dist[metadata->state[phy_pg/PPB]] += 1;
                cyc_tot += metadata->state[phy_pg/PPB];
                hot_pg_num += 1;
            }
        }
    } else if (hotness_rw==2){//rec writecold
        for(int i=0;i<logi;i++){
            phy_pg = metadata->pagemap[i];
            if(metadata->read_cnt[i] < write_avg){
                dist[metadata->state[phy_pg/PPB]] += 1;
                cyc_tot += metadata->state[phy_pg/PPB];
                hot_pg_num += 1;
            }
        }
    } else if (hotness_rw==3){//rec readhot
        for(int i=0;i<logi;i++){
            phy_pg = metadata->pagemap[i];
            if(metadata->read_cnt[i] < read_avg){
                dist[metadata->state[phy_pg/PPB]] += 1;
                cyc_tot += metadata->state[phy_pg/PPB];
                hot_pg_num += 1;
            }
        }
    } else{
        printf("unknown hotness specification, please write appropriate type.\n");
        sleep(1);
        abort();
    }

    if(taskidx != -1){
        fprintf(fp, "%d, %f, %d",cur_cp,(float)cyc_tot / (float)task_pg_num,task_pg_num);
    } else {
        fprintf(fp, "%d, %f",cur_cp,(float)cyc_tot / (float)hot_pg_num);
    }
    for(int i=0;i<MAXPE;i++){
        fprintf(fp, "%d, ",dist[i]);
    }
    fprintf(fp,"\n");
}

void print_freeblock_profile(FILE* fp, int cur_cp, meta* metadata, bhead* fblist_head, bhead* write_head){
    block* cur = fblist_head->head;
    int fblist_state[MAXPE];
    int max_age = -1;
    int min_age = -1;
    for(int i=0;i<MAXPE;i++){
        fblist_state[i] = 0;
    }
    while(cur != NULL){
        fblist_state[metadata->state[cur->idx]] += 1;
        if (min_age == -1){
            min_age = metadata->state[cur->idx];
        } else if (min_age != -1 && min_age > metadata->state[cur->idx]){
            min_age = metadata->state[cur->idx];
        } else {/*do nothing*/}
        
        if (max_age == -1){
            max_age = metadata->state[cur->idx];
        } else if (max_age != -1 && max_age < metadata->state[cur->idx]){
            max_age = metadata->state[cur->idx];
        } else {/*do nothing*/}
        cur = cur->next;
    }
    cur = write_head->head;
    while(cur != NULL){
        fblist_state[metadata->state[cur->idx]] += 1;
        if (min_age == -1){
            min_age = metadata->state[cur->idx];
        } else if (min_age != -1 && min_age > metadata->state[cur->idx]){
            min_age = metadata->state[cur->idx];
        } else {/*do nothing*/}
        
        if (max_age == -1){
            max_age = metadata->state[cur->idx];
        } else if (max_age != -1 && max_age < metadata->state[cur->idx]){
            max_age = metadata->state[cur->idx];
        } else {/*do nothing*/}
        cur = cur->next;
    }
    fprintf(fp,"%d,%d,%d,%d,",cur_cp,fblist_head->blocknum + write_head->blocknum,min_age,max_age);
    for(int i=0;i<MAXPE;i++){
        fprintf(fp, "%d,",fblist_state[i]);
    }
    fprintf(fp,"\n");
}

void print_invalid_profile(FILE* fp, int cur_cp, meta* metadata){
    int invalid_per_state[MAXPE];
    int sorted_idx[NOB];
    int temp;
    for(int i=0;i<MAXPE;i++){
        invalid_per_state[i] = 0;
    }
    for(int i=0;i<NOB;i++){
        sorted_idx[i] = i;
    }
    for(int i=NOB-1;i>0;i--){
        for(int j=0;j<i;j++){
            if(metadata->state[sorted_idx[j]] > metadata->state[sorted_idx[j+1]]){
                temp = sorted_idx[j];
                sorted_idx[j] = sorted_idx[j+1];
                sorted_idx[j+1] = temp;
            }
        }
    }
    fprintf(fp,"%d,",cur_cp);
    for(int i=0;i<NOB;i++){
        fprintf(fp, "%d(%d),",metadata->invnum[sorted_idx[i]], metadata->state[sorted_idx[i]]);
    }
    fprintf(fp,"\n");
}

void print_writeblock_profile(FILE* fp, long cur_cp, meta* metadata, bhead* fblist_head, bhead* write_head, int write_idx, int target_idx, int type, int rank, float proportion, int bidx, int candnum){
    int oldest_writable = 0, youngest_writable = MAXPE+1;
    block* cur = fblist_head->head;
    while (cur != NULL){
        if (oldest_writable <= metadata->state[cur->idx]){
            oldest_writable = metadata->state[cur->idx];
        }
        if (youngest_writable >= metadata->state[cur->idx]){
            youngest_writable = metadata->state[cur->idx];
        }
        cur = cur->next;
    }
    cur = write_head->head;
    while (cur != NULL){
        if (oldest_writable <= metadata->state[cur->idx]){
            oldest_writable = metadata->state[cur->idx];
        }
        if (youngest_writable >= metadata->state[cur->idx]){
            youngest_writable = metadata->state[cur->idx];
        }
        cur = cur->next;
    }
    fprintf(fp,"%ld,%d,%d, %d,%d,%d,%d, %d, %d,%d, %f,%d,%d,%d,%d\n",
    cur_cp,type,write_idx,
    metadata->write_cnt[write_idx],metadata->tot_write_cnt,metadata->read_cnt[write_idx],metadata->tot_read_cnt,
    rank,
    oldest_writable,youngest_writable,
    proportion, target_idx, metadata->state[target_idx],bidx,candnum);
}
void check_profile(float tot_u, meta* metadata, rttask* tasks, int tasknum, long cur_cp, FILE* fp, FILE* fplife){
    int cur_read_worst[tasknum];
    if(tot_u >= 1.0){    
        printf("[%ld]utilization overflowed(%f) exit in 5 seconds...\n",cur_cp,tot_u);
        printf("block state : ");
        for(int i=0;i<NOB;i++){
            printf("%d(%d) ",i,metadata->state[i]);
        }
        printf("\nread-hot(0~128)\n");
        int readhotworst = 0;
        for(int k=0;k<tasknum;k++){
            cur_read_worst[k] = 0;
            for(int i=0;i<NOP;i++){
                if(metadata->vmap_task[i] == k){
                    int cur_b = i/PPB;
                    if(metadata->state[cur_b] >= cur_read_worst[k]){
                        cur_read_worst[k] = metadata->state[cur_b];
                    }
                }
            }
            printf("[%d]read_worst : %d\n",k,cur_read_worst[k]);
        }
       
        for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
            printf("utils:%f, %f, %f\n",
                    metadata->runutils[0][j],
                    metadata->runutils[1][j],
                    metadata->runutils[2][j]);
        }
        fprintf(fplife,"%ld,",cur_cp);
        fflush(fplife);
        fflush(fp);
        fclose(fp);
        fclose(fplife);
        sleep(1);
        abort();
    }
}

void check_block(float tot_u, meta* metadata, rttask* tasks, int tasknum, long cur_cp, FILE* fp, FILE* fplife){
    int miss = 0;
    int cur_read_worst[tasknum];
    for(int i=0;i<NOB;i++){
        if(metadata->state[i] >= MAXPE){
            miss = 1;
        }
    }
    if(miss==1){
        printf("[%ld]a block reached its lifecycle end(%f). exit in 5 seconds...\n",cur_cp,tot_u);
        printf("block state : ");
        for(int i=0;i<NOB;i++){
            printf("%d(%d) ",i,metadata->state[i]);
        }
        for(int k=0;k<tasknum;k++){
            cur_read_worst[k] = 0;
            for(int i=0;i<NOP;i++){
                if(metadata->vmap_task[i] == k){
                    int cur_b = i/PPB;
                    if(metadata->state[cur_b] >= cur_read_worst[k]){
                        cur_read_worst[k] = metadata->state[cur_b];
                    }
                }
            }
            printf("[%d]read_worst : %d\n",k,cur_read_worst[k]);
        }
        for(int j=0;j<tasknum;j++){
            printf("utils:%f, %f, %f\n",
                        metadata->runutils[0][j],
                        __calc_ru(&(tasks[j]),cur_read_worst[j]),
                        metadata->runutils[2][j]);
        }
        fprintf(fplife,"%ld,",cur_cp);
        fflush(fplife);
        fflush(fp);
        fclose(fp);
        fclose(fplife);
        sleep(1);
        abort();
    }
}

//misc function for motivation data
float print_profile_best(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state){
    //init params
    int cur_read_worst[tasknum];
    
    //find worst case util w.r.t system-wise worst block
    float worst_util = find_worst_util(tasks,tasknum,metadata);

    //find current util w.r.t actual blocks.
    //in this function, assume that read occurs in best block!
    float total_u=0.0;
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_u += __calc_ru(&(tasks[j]),yng);
        total_u += metadata->runutils[2][j];
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //print all infos
    fprintf(fp,"%ld,%d,%f,%f,%f,%f,%f,%d,%d,%d,%d\n",
    cur_cp,taskidx,
    worst_util,total_u,
    metadata->runutils[0][taskidx],
    __calc_ru(&(tasks[taskidx]),cur_read_worst[taskidx]),
    metadata->runutils[2][taskidx],
    old,yng,cur_gc_idx,cur_gc_state); 
    return total_u;
}

float print_profile_timestamp(rttask* tasks, int tasknum, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp){
    //init params
    int cur_read_worst[tasknum];
    int state_tot = 0;
    double state_avg = 0.0;
    double state_var = 0.0;
    float total_u = 0.0;
    float total_r = 0.0;
    float total_w = 0.0;
    float total_gc = 0.0;
    float total_u_noblock = 0.0;
    //find worst case util w.r.t system-wise worst block
    
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_w += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_r += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        total_gc += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //block state profiling
    for(int i=0;i<NOB;i++){
        state_tot += metadata->state[i];
    }
    state_avg = (double)state_tot / (double)NOB;
    for(int i=0;i<NOB;i++){
        state_var += pow((double)metadata->state[i] - (double)state_avg ,2.0);
    }
    state_var = state_var / NOB;
    state_var = sqrt(state_var);
    //print all infos
    fprintf(fp,"%ld,%f,%d,%d,%lf,%lf\n",
    cur_cp,total_u,old,yng,state_avg,state_var); 
    return total_u;
}
