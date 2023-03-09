#include "init.h"
void init_metadata(meta* metadata, int tasknum, int cycle){
    FILE* cyc_fp;
    int cyc_b;
    for (int i=0;i<NOP;i++){
        metadata->pagemap[i] = -1;
        metadata->rmap[i] = -1;
        metadata->invmap[i] = 0;
        metadata->vmap_task[i] = -1;
        metadata->read_cnt[i] = 0;
        metadata->write_cnt[i] = 0;
    }
    for (int i=0;i<NOB;i++){
        metadata->invnum[i] = 0;
        metadata->state[i] = cycle;
        metadata->EEC[i]=0;
        metadata->access_window[i] = 0;
    }
    metadata->total_invalid = 0;
    metadata->tot_read_cnt = 0;
    metadata->tot_write_cnt = 0;
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
    //REFACTOR::for per-task tracking, malloc these member variables.
    //because number of task is declared in main, not header.
    metadata->read_cnt_task = (int*)malloc(sizeof(int)*tasknum);
    metadata->write_cnt_task = (int*)malloc(sizeof(int)*tasknum);
    metadata->access_tracker = (char**)malloc(sizeof(char*)*tasknum);
    for(int i=0;i<tasknum;i++){
        metadata->read_cnt_task[i] = 0;
        metadata->write_cnt_task[i] = 0;
        metadata->access_tracker[i] = (char*)malloc(sizeof(char)*NOB);
        for(int j=0;j<NOB;j++){
            metadata->access_tracker[i][j] = 0;
        }
    }
    //index for write = 0, read = 1, GC =2. therefore, 3 double-pointer is hardcoded
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
}

void free_metadata(meta* metadata){
    free(metadata->cur_read_worst);
    free(metadata->read_cnt_task);
    free(metadata->write_cnt_task);
    free(metadata->runutils);
    free(metadata->access_tracker);
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
        ll_append(newlist,new);
    }
    printf("init done\n");
    return newlist;
}

void free_blocklist(bhead* head){
    ll_free(head);
}
void init_utillist(float* rutils, float* wutils, float* gcutils){}
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