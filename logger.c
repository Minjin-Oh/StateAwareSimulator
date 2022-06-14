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
    } else {
        fp = fopen("SA_prof_motiv.csv","w");
    }
    return fp;
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

float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state,int getfp){
    //init params
    int cur_read_worst[tasknum];
    
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
    float total_u=0.0;
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //print all infos
    fprintf(fp,"%ld,%d,%f,%f,%f,%f,%f,%d,%d,%d,%d,%d\n",
    cur_cp,taskidx,
    worst_util,total_u,
    metadata->runutils[0][taskidx],
    __calc_ru(&(tasks[taskidx]),cur_read_worst[taskidx]),
    metadata->runutils[2][taskidx],
    old,yng,cur_gc_idx,cur_gc_state,getfp); 
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