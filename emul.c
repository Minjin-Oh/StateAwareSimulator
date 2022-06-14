#include "stateaware.h"
#include "emul.h"

void finish_WR(rttask* task, IO* cur_IO, meta* metadata){
    int lpa, ppa, old_ppa, old_block;
    
    lpa = cur_IO->lpa;
    ppa = cur_IO->ppa;
    old_ppa = metadata->pagemap[lpa];
    old_block = (int)(old_ppa/PPB);

    //invalidate old ppa
    metadata->invnum[old_block]++;
    metadata->invmap[old_ppa] = 1;
    metadata->vmap_task[old_ppa] = -1;
    metadata->total_invalid++;
    
    //validate new ppa
    metadata->pagemap[lpa] = ppa;
    metadata->rmap[ppa] = lpa;
    metadata->total_fp--;
    
    //update access profile
    metadata->access_tracker[task->idx][ppa/PPB] = 1;
    metadata->vmap_task[ppa] = task->idx;
}

void finish_RD(){
    /*do nothing*/
}
void finish_GC(rttask* task, IO* cur_IO, meta* metadata){
    int rsvidx, old_lpa, new_ppa, old_ppa;
    rsvidx = cur_IO->gc_tar_ppa / (int)PPB;
    old_lpa = cur_IO->gc_old_lpa;
    new_ppa = cur_IO->gc_tar_ppa;
    old_ppa = cur_IO->gc_vic_ppa;

    //if still valid data, update mapping
    if(metadata->pagemap[old_lpa]==old_ppa){
        metadata->pagemap[old_lpa] = new_ppa;
        metadata->rmap[new_ppa] = old_lpa;
        metadata->invmap[new_ppa] = 0;
        metadata->vmap_task[new_ppa] = metadata->vmap_task[old_ppa];
    }
    //if it is not valid data, invalidate map
    else{
        metadata->rmap[new_ppa] = -1;
        metadata->invmap[new_ppa] = 1;
        metadata->invnum[rsvidx]++;
        metadata->vmap_task[new_ppa] = -1;
    }
}

void finish_GCER(rttask* task, IO* cur_IO, meta* metadata, bhead* fblist_head, bhead* rsvlist_head, GCblock* cur_GC){
    int vicidx;
    vicidx = cur_IO->vic_idx;
    //reset metadata for pages
    for(int i=0;i<PPB;i++){
        metadata->rmap[(vicidx)*PPB+i]=-1;
        metadata->invmap[(vicidx)*PPB+i]=0;
        metadata->vmap_task[(vicidx)*PPB+i]=-1;
    }
    //reset metadata for block
    metadata->invnum[vicidx] = 0;
    metadata->state[vicidx]++;
    metadata->total_invalid -= PPB - cur_IO->gc_valid_count;
    metadata->total_fp += PPB - cur_IO->gc_valid_count;
    ll_append(fblist_head,cur_GC->cur_rsv);
    ll_append(rsvlist_head,cur_GC->cur_vic);
    cur_GC->cur_rsv = NULL;
    cur_GC->cur_vic = NULL;
    printf("listcheck:fb %d rsv %d\n",fblist_head->blocknum,rsvlist_head->blocknum);
}

void finish_RRRE(){
    /*nothing*/
}

void finish_RRER(rttask* task, IO* cur_IO, meta* metadata){
    int vicidx;
    vicidx = cur_IO->vic_idx;
    for(int i=0;i<PPB;i++){
        metadata->rmap[(vicidx)*PPB+i]=-1;
        metadata->invmap[(vicidx)*PPB+i]=0;
        metadata->vmap_task[(vicidx)*PPB+i]=-1;
    }
    //reset metadata for block
    metadata->invnum[vicidx] = 0;
    metadata->state[vicidx]++;
}

void finish_RRWR(rttask* task, IO* cur_IO, meta* metadata, bhead* fblist_head, bhead* full_head, RRblock* cur_RR){
    int taridx, old_lpa, new_ppa, old_ppa, lastreq;
    taridx = cur_IO->gc_tar_ppa / (int)PPB;
    old_lpa = cur_IO->gc_old_lpa;
    new_ppa = cur_IO->gc_tar_ppa;
    old_ppa = cur_IO->gc_vic_ppa;

    //if still valid data, update mapping
    if(metadata->pagemap[old_lpa]==old_ppa){
        metadata->pagemap[old_lpa] = new_ppa;
        metadata->rmap[new_ppa] = old_lpa;
        metadata->invmap[new_ppa] = 0;
        metadata->vmap_task[new_ppa] = metadata->vmap_task[old_ppa];
    }
    //if it is not valid data, invalidate map
    else{
        metadata->rmap[new_ppa] = -1;
        metadata->invmap[new_ppa] = 1;
        metadata->invnum[taridx]++;
        metadata->vmap_task[new_ppa] = -1;
    }
    if(lastreq==1){
        metadata->total_invalid -= PPB - cur_IO->rr_valid_count;
        metadata->total_fp += PPB - cur_IO->rr_valid_count;
        if(cur_RR->cur_vic1->idx == cur_IO->vic_idx){
            if(cur_IO->rr_valid_count == PPB){
                ll_append(full_head,cur_RR->cur_vic1);
            } 
            else {
                ll_append(fblist_head,cur_RR->cur_vic1);
            }
            cur_RR->cur_vic1 = NULL;
        } else if (cur_RR->cur_vic2->idx == cur_IO->vic_idx){
            if(cur_IO->rr_valid_count == PPB){
                ll_append(full_head,cur_RR->cur_vic2);
            } 
            else {
                ll_append(fblist_head,cur_RR->cur_vic2);
            }
            cur_RR->cur_vic2 = NULL;
        }
        
    }
}

void finish_req(rttask* task, IO* cur_IO, meta* metadata, 
                bhead* fblist_head, bhead* rsvlist_head, bhead* full_head,
                GCblock* cur_GC, RRblock* cur_RR){
    
    //called by main function to process I/O request 
    if(cur_IO->type==WR){
        finish_WR(task,cur_IO,metadata);
    }
    else if (cur_IO->type==RD){
        finish_RD();
    }
    else if(cur_IO->type==GC){
        finish_GC(task, cur_IO,metadata);
    }
    else if(cur_IO->type==GCER){
        finish_GCER(task,cur_IO,metadata,fblist_head,rsvlist_head,cur_GC);
        //sleep(1);  
    }
    else if(cur_IO->type==RRRE){
        finish_RRRE();
    }
    else if(cur_IO->type==RRER){
        finish_RRER(task,cur_IO,metadata);
    }
    else if(cur_IO->type==RRWR){
        finish_RRWR(task,cur_IO,metadata,fblist_head,full_head,cur_RR);
    }
    free(cur_IO);
}

long find_closenum(long cur_cp, long period){
    double mult = (double)cur_cp / (double)period;
    long close_num = (long)ceil(mult) * period;
    //printf("mult : %lf, closest period : %ld\n",mult,close_num);
    return close_num;
}

long find_next_time(rttask* tasks, int tasknum, long cur_dl, long wl_dl, long cur_cp){
    //find 
    double mult;
    long periods[tasknum];
    long rp, wp, gcp, temp, temp2;
    long res = -1, task_min = -1;
    int res_task = -1;
    //find closest I/O release period (within task)
    for(int i=0;i<tasknum;i++){
        rp = find_closenum(cur_cp,(long)tasks[i].rp);
        wp = find_closenum(cur_cp,(long)tasks[i].wp);
        gcp = find_closenum(cur_cp,(long)tasks[i].gcp);
        temp = wp >= rp ? rp : wp;
        temp2 = temp >= gcp ? gcp : temp;
        periods[i] = temp2;
        //printf("[FNT-task][%d] %ld, %ld %ld %ld\n",i,periods[i],rp,wp,gcp);
    }
    //find closest I/O release period (within taskset)
    for(int i=0;i<tasknum;i++){
        if(res == -1){
            res = periods[i];
            res_task = i;
        }
        else{
            if(res > periods[i]){
                res = periods[i];
                res_task = i;
            }
        }
    }
    task_min = res;
    //compare with req dl and rr dl
    res = res >= wl_dl ? wl_dl : res;
    res = res >= cur_dl ? cur_dl : res;
    //printf("picked %ld, task min : %ld, IO min : %ld, rr min : %ld\n",res, task_min, cur_dl, wl_dl);
    return res;
}