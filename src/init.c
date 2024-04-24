#include "init.h"
#include "IOgen.h"
void init_metadata(meta* metadata, int tasknum, int cycle){
    FILE* cyc_fp;
    FILE* timing_fp;
    FILE* rank_fp;
    char name[30];
    int cyc_b;
    long next_update_time;
    for (int i=0;i<NOP;i++){
        metadata->pagemap[i] = -1;
        metadata->rmap[i] = -1;
        metadata->invmap[i] = 0;
        metadata->vmap_task[i] = -1;
        metadata->read_cnt[i] = 0;
        metadata->write_cnt[i] = 0;
        metadata->write_cnt_per_cycle[i] = 0;
        metadata->avg_update[i] = 0;
        metadata->recent_update[i] = 0L;
#ifdef IOTIMING
        IO_timing_update(metadata,i,0,0L);
#endif
    }
    for (int i=0;i<NOB;i++){
        metadata->invnum[i] = 0;
        metadata->state[i] = cycle;
        metadata->EEC[i]=0;
        metadata->access_window[i] = 0;
        metadata->GC_locktime[i] = 0L;
    }
    for (int i=0;i<LIFESPAN_WINDOW;i++){
        metadata->data_lifespan[i] = 0L;
    }
    metadata->total_invalid = 0;
    metadata->tot_read_cnt = 0;
    metadata->tot_write_cnt = 0;
    metadata->left_rankwrite_num = LIFESPAN_WINDOW;
    metadata->lifespan_record_num = 0;
    metadata->total_fp = NOP-PPB*tasknum;
    metadata->reserved_write = 0;

    //if input cyc value is -1, update init cyc as randomly generated value, stored in cyc.csv.
    if(cycle == -1){
        cyc_fp = fopen("cyc.csv","r");
        for(int i=0;i<NOB;i++){
            fscanf(cyc_fp,"%d,",&cyc_b);
            metadata->state[i] = cyc_b;
        }
    }
    //if writerank csv file exists, import the rank
    //since rank_bounds can be either "update order" or "update time", type should be long.
    rank_fp = fopen("writerank.csv","r");
    if(rank_fp != NULL){
        fscanf(rank_fp,"%d, ",&(metadata->ranknum));
        metadata->rank_bounds = (long*)malloc(sizeof(long)*(metadata->ranknum + 1));
        metadata->rank_write_count = (int*)malloc(sizeof(int)*(metadata->ranknum + 1));
        metadata->rank_bounds[0] = 0;
        metadata->rank_write_count[0] = 0L;
        for(int i=1;i<metadata->ranknum+1;i++){
            int temp_rank;
            fscanf(rank_fp,"%d, ", &(temp_rank));
            metadata->rank_bounds[i] = (long)temp_rank;
            metadata->rank_write_count[i] = 0;
        }
    }
    
    //malloc these member variable for per-task tracking
    //this is because number of task is declared in main, not header.
    metadata->read_cnt_task = (int*)malloc(sizeof(int)*tasknum);
    metadata->write_cnt_task = (int*)malloc(sizeof(int)*tasknum);
    metadata->access_tracker = (char**)malloc(sizeof(char*)*tasknum);
    metadata->rewind_time_per_task = (long*)malloc(sizeof(long)*tasknum);
    for(int i=0;i<tasknum;i++){
        metadata->read_cnt_task[i] = 0;
        metadata->write_cnt_task[i] = 0;
        metadata->access_tracker[i] = (char*)malloc(sizeof(char)*NOB);
        for(int j=0;j<NOB;j++){
            metadata->access_tracker[i][j] = 0;
        }
        metadata->rewind_time_per_task[i] = 0L;
    }
    //index for write = 0, read = 1, GC = 2. therefore, 3 double-pointer is hardcoded
    metadata->runutils = (float**)malloc(sizeof(float*)*3);
    for(int i=0;i<3;i++){
        metadata->runutils[i] = (float*)malloc(sizeof(float)*tasknum);
        for(int j=0;j<tasknum;j++){
            metadata->runutils[i][j] = 0.0;
        }
    }
    metadata->cur_read_worst = (int*)malloc(sizeof(int)*tasknum);
    for(int i=0;i<tasknum;i++){
        metadata->cur_read_worst[i] = 0;
    }
    metadata->cur_rank_info.cur_left_write = (int*)malloc(sizeof(int)*tasknum);
    metadata->cur_rank_info.tot_ranked_write = (int*)malloc(sizeof(int)*tasknum);
    metadata->cur_rank_info.ranks_for_write = (int**)malloc(sizeof(int*)*tasknum);
    metadata->cur_rank_info.timings_for_write = (long**)malloc(sizeof(long*)*tasknum);
    for(int i=0;i<tasknum;i++){
        metadata->cur_rank_info.cur_left_write[i] = 0;
        metadata->cur_rank_info.tot_ranked_write[i] = 0; 
        metadata->cur_rank_info.ranks_for_write[i]=(int*)malloc(sizeof(int)*20000);
        metadata->cur_rank_info.timings_for_write[i] = (long*)malloc(sizeof(long)*20000);
        for(int j=0;j<20000;j++){
            metadata->cur_rank_info.ranks_for_write[i][j] = 0;
            metadata->cur_rank_info.timings_for_write[i][j] = 0;
        }
    }
    sleep(1);
}

void free_metadata(meta* metadata){
    free(metadata->cur_read_worst);
    free(metadata->read_cnt_task);
    free(metadata->write_cnt_task);
    free(metadata->runutils);
    free(metadata->access_tracker);
    free(metadata->rank_bounds);
    free(metadata);
}

bhead* init_blocklist(int start, int end){
    bhead* newlist =  ll_init();
    for (int i=start;i<end+1;i++){
        block* new = (block*)malloc(sizeof(block));
        new->fpnum = PPB;
        new->idx = i;
        new->prev = NULL;
        new->next = NULL;
        new->lb = 0;
        new->ub = 0;
        new->wb_rank = 0;
        ll_append(newlist,new);
    }
    printf("init done\n");
    return newlist;
}

void free_blocklist(bhead* head){
    ll_free(head);
}

void init_prof_exec(prof_exec* pe){
    FILE* fpexec = fopen("execution_profile.csv","r");
    if(fpexec == NULL){
        printf("no profile result available. Please specify csv file or try linear mode.\n");
        abort();
    }
    pe->pe_steps = (int*)malloc(sizeof(int)*OPTYPENUM);
    pe->pe_values = (int**)malloc(sizeof(int*)*OPTYPENUM);
    pe->pe_thres = (int**)malloc(sizeof(int*)*OPTYPENUM);
    for(int i=0;i<OPTYPENUM;i++){
        fscanf(fpexec,"%d, ",&(pe->pe_steps[i]));
        pe->pe_values[i] = (int*)malloc(sizeof(int)*(pe->pe_steps[i]+1));
        pe->pe_thres[i] = (int*)malloc(sizeof(int)*(pe->pe_steps[i]+1));
        for(int j=0;j<pe->pe_steps[i]+1;j++){
            fscanf(fpexec,"%d, ",&(pe->pe_values[i][j]));
        }
        for(int j=0;j<pe->pe_steps[i];j++){
            fscanf(fpexec,"%d, ",&(pe->pe_thres[i][j]));
        }
        pe->pe_thres[i][pe->pe_steps[i]] = MAXPE;
    }
    //check exec time structure
    printf("testing exec profile\n");
    for(int i=0;i<OPTYPENUM;i++){
        printf("%d, ",pe->pe_steps[i]);
        for(int j=0;j<pe->pe_steps[i]+1;j++){
            printf("%d until %d, ",pe->pe_values[i][j],pe->pe_thres[i][j]);
        }
        printf("\n");
    }
}

void free_prof_exec(prof_exec* pe){
    free(pe->pe_steps);
    for(int i=0;i<OPTYPENUM;i++){
        free(pe->pe_values[i]);
        free(pe->pe_thres[i]);
    }
    free(pe->pe_values);
    free(pe->pe_thres);
}

void init_task(rttask* task,int idx, int wp, int wn, int rp ,int rn, int gcp,int lb, int ub){
    task->idx = idx;
    task->wp = wp;
    task->wn = wn;
    task->rp = rp;
    task->rn = rn;
    task->gcp  = gcp;
    task->addr_lb = lb;
    task->addr_ub = ub;
}