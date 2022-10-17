#include "stateaware.h"

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
    FILE** fps;
    fps = (FILE**)malloc(sizeof(FILE*)*tasknum);
    char name[100];
    if(wflag == 5){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_FIX_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (wflag == 1 && gcflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_do_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    }  else if (wflag == 0 && gcflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_no_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (wflag == 3 && gcflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_h1_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (wflag == 4 && gcflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_h2_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (wflag == 6 && gcflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_hot_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (gcflag == 1 && wflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_DOgc_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else if (gcflag == 4 && wflag == 0){
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_ygc_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
    } else {
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_new_t%d.csv",i);
            fps[i] = fopen(name,"w");
        }
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
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state, block* cur_wb, bhead* fblist_head, bhead*write_head, int getfp){
    //init params
    int cur_read_worst[tasknum];
    float total_u = 0.0;
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
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    total_u_noblock = total_u;
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //print all infos
    fprintf(fp,"%ld,%d, %f,%f,%f,%f,%f,%f, %d,%d, %d,%d,%d, %d,%d, %d,%d\n",
    cur_cp,taskidx,
    worst_util,total_u, total_u_noblock,
    metadata->runutils[0][taskidx],
    metadata->runutils[1][taskidx],
    metadata->runutils[2][taskidx],
    old,yng,
    cur_gc_idx,cur_gc_state,getfp,
    cur_wb->idx,metadata->state[cur_wb->idx],
    fblist_head->blocknum,write_head->blocknum); 
    return total_u;
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