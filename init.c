#include "stateaware.h"

void init_metadata(meta* metadata){
    for (int i=0;i<NOP;i++){
        metadata->pagemap[i] = -1;
        metadata->rmap[i] = -1;
        metadata->invmap[i] = 0;
    }
    for (int i=0;i<NOB;i++){
        metadata->invnum[i] = 0;
        metadata->state[i] = 0;
        metadata->access_window[i] = 0;
    }
}

void free_metadata(meta* metadata){
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
void init_task(rttask* task,int idx, int wp, int wn, int rp ,int rn, int gcp){
    task->idx = idx;
    task->wp = wp;
    task->wn = wn;
    task->rp = rp;
    task->rn = rn;
    task->gcp  = gcp;
}