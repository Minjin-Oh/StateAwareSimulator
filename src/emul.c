#include "stateaware.h"
#include "emul.h"

extern long cur_cp;

void finish_WR(rttask* task, IO* cur_IO, meta* metadata, bhead* full_head){
    int lpa, ppa, old_ppa, old_block;
    lpa = cur_IO->lpa;
    ppa = cur_IO->ppa;
    old_ppa = metadata->pagemap[lpa];
    old_block = (int)(old_ppa/PPB);

    //invalidate old ppa (1 == invalid, 0 == valid)
    metadata->rmap[old_ppa] = -1;
    if(metadata->invmap[old_ppa] == 0){
        metadata->invmap[old_ppa] = 1;
        metadata->invnum[old_block] += 1;
        metadata->invalidation_window[old_block] += 1;
    }
    metadata->vmap_task[old_ppa] = -1;
    metadata->total_invalid++;
    
    //validate new ppa
    metadata->pagemap[lpa] = ppa;
    metadata->rmap[ppa] = lpa;
    metadata->total_fp--;
    metadata->reserved_write--;
    
    //update access profile
    metadata->access_tracker[cur_IO->taskidx][ppa/PPB] = 1;
    metadata->vmap_task[ppa] = cur_IO->taskidx;
}

void finish_BWR(rttask* task, IO* cur_IO, meta* metadata, bhead* full_head){
    //currently, BWR does nothing.(testing sched)
    printf("[BWR]%d goto %d\n",cur_IO->rr_vic_ppa, cur_IO->rr_tar_ppa);
    sleep(1);
    /*
    //similar to WR, but have to check concurrency issue
    int old_ppa, old_lpa, new_ppa, old_block;
    int new_block = cur_IO->rr_tar_ppa / (int)PPB;
    old_lpa = cur_IO->rr_old_lpa;
    new_ppa = cur_IO->rr_tar_ppa;
    old_ppa = cur_IO->rr_vic_ppa;
    old_block = (int)(old_ppa/PPB);

    //invalidate old ppa (1 == invalid, 0 == valid)
    //!! do invalidtion only if the page X relocated before BG write
    if(metadata->invmap[old_ppa] == 0 && metadata->pagemap[old_lpa] == old_ppa){
        metadata->rmap[old_ppa] = -1;
        metadata->vmap_task[old_ppa] = -1;
        metadata->invmap[old_ppa] = 1;
        metadata->invnum[old_block] += 1;
    }
    
    //validate or invalidate new ppa
    if(metadata->pagemap[old_lpa] == old_ppa){
        //if still valid data, update mapping
        metadata->pagemap[old_lpa] = new_ppa;
        metadata->rmap[new_ppa] = old_lpa;
        metadata->invmap[new_ppa] = 0;
        metadata->vmap_task[new_ppa] = cur_IO->taskidx;
    } 
    else {
        //else, invalidate newly written page
        metadata->rmap[new_ppa] = -1;
        metadata->invmap[new_ppa] = 1;
        metadata->invnum[new_block]++;
        metadata->vmap_task[new_ppa] = -1;
    }

    //update total fp and access tracker
    metadata->total_fp--;
    if(metadata->pagemap[old_lpa]==old_ppa){
        metadata->access_tracker[cur_IO->taskidx][new_ppa/PPB] = 1;
    }*/
}

void finish_RD(rttask* tasks, IO* cur_IO, meta* metadata){
    int target_block;
    target_block = metadata->pagemap[cur_IO->lpa] / PPB;
    metadata->access_window[target_block]++;
}

void finish_GC(rttask* task, IO* cur_IO, meta* metadata){
    int taridx, old_lpa, new_ppa, old_ppa;
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
#ifdef GC_ON_WRITEBLOCK
    //when GC relocates page on writeblock, total fp is decreased & access tracker is updated
    metadata->total_fp--;
    if(metadata->pagemap[old_lpa]==old_ppa){
        metadata->access_tracker[cur_IO->taskidx][new_ppa/PPB] = 1;
    }
#endif
}

void finish_GCER(rttask* task, IO* cur_IO, meta* metadata, bhead* fblist_head, bhead* rsvlist_head, GCblock* cur_GC){
    int vicidx,rsvidx;
    vicidx = cur_IO->vic_idx;
    rsvidx = cur_IO->rsv_idx;

    //reset metadata for pages
    for(int i=0;i<PPB;i++){
        metadata->rmap[(vicidx)*PPB+i]=-1;
        metadata->invmap[(vicidx)*PPB+i]=0;
        metadata->vmap_task[(vicidx)*PPB+i]=-1;
    }
    //reset metadata for block
    metadata->invnum[vicidx] = 0;
    metadata->access_window[vicidx] = 0;
    metadata->state[vicidx]++;
    metadata->EEC[vicidx]++;
    metadata->total_invalid -= PPB - cur_IO->gc_valid_count;

#ifdef GC_ON_WRITEBLOCK
    //when GC relocates page on writeblock, victim block directly becomes free block
    metadata->total_fp += PPB;
    if(cur_GC->cur_vic == NULL){
        printf("vic null!\n");
        abort();
    }
    ll_append(fblist_head,cur_GC->cur_vic);
    cur_GC->cur_vic = NULL;
#endif
#ifndef GC_ON_WRITEBLOCK
    metadata->total_fp += PPB - cur_IO->gc_valid_count;
    if(cur_GC->cur_rsv == NULL){
        printf("rsv null!\n");
        abort();
    } else if (cur_GC->cur_vic == NULL){
        printf("vic null!\n");
        abort();
    }
    ll_append(fblist_head,cur_GC->cur_rsv);
    ll_append(rsvlist_head,cur_GC->cur_vic);
    cur_GC->cur_rsv = NULL;
    cur_GC->cur_vic = NULL;
#endif
    
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
    //update block state
    metadata->state[vicidx]++;
    metadata->access_window[vicidx] = 0;
}

void finish_RRWR(rttask* task, IO* cur_IO, meta* metadata, bhead* fblist_head, bhead* full_head, RRblock* cur_RR){
    int taridx, vicidx, old_lpa, new_ppa, old_ppa, lastreq, tot_inv, temp;
    taridx = cur_IO->tar_idx;
    vicidx = cur_IO->vic_idx;
    old_lpa = cur_IO->rr_old_lpa;
    new_ppa = cur_IO->rr_tar_ppa;
    old_ppa = cur_IO->rr_vic_ppa;
    lastreq = cur_IO->islastreq;
    tot_inv=0;
    temp = 0;
    //if still valid data, update mapping
    if(metadata->pagemap[old_lpa]==old_ppa){
        metadata->pagemap[old_lpa] = new_ppa;
        metadata->rmap[new_ppa] = old_lpa;
        metadata->invmap[new_ppa] = 0;
        metadata->vmap_task[new_ppa] = metadata->vmap_task[old_ppa];
    }
    else{ //if it is not valid data, invalidate map
        metadata->rmap[new_ppa] = -1;
        metadata->invmap[new_ppa] = 1;
        metadata->vmap_task[new_ppa] = -1;
    }
    //printf("[RRWR-fin]tar:%d,inv?:%d\n",cur_IO->rr_tar_ppa,metadata->invmap[cur_IO->rr_tar_ppa]);
    if(lastreq==1){
        //last write request means metadata update!
        metadata->total_invalid -= PPB - cur_IO->rr_valid_count;
        metadata->total_fp += PPB - cur_IO->rr_valid_count;
        //check current invalid number
        for(int i=0;i<PPB;i++){
            if(metadata->invmap[taridx*PPB+i]==1){
                tot_inv += 1;
            }
        }
        metadata->invnum[taridx] = tot_inv;
        //printf("[RRWR-fin]move completed,tar:%d,invnum:%d\n",taridx,metadata->invnum[taridx]);
        
        //if vic1->vic2 movement is completed,
        //printf("cur->tar : %d, vic1 : %d, vic2 : %d\n",cur_IO->tar_idx, cur_RR->cur_vic1->idx,cur_RR->cur_vic2->idx);
        if(cur_RR->cur_vic2->idx == cur_IO->tar_idx){    
            if(cur_IO->rr_valid_count == PPB){
                ll_append(full_head,cur_RR->cur_vic2);
                //printf("app %d to full, fullnum : %d\n",cur_RR->cur_vic2->idx,full_head->blocknum);
            } 
            else {
                ll_append(fblist_head,cur_RR->cur_vic2);
                //printf("app %d to fb, fbnum : %d\n",cur_RR->cur_vic2->idx,full_head->blocknum);
            }
            
        } 
        //if vic2->vic1 movement is completed,
        else if (cur_RR->cur_vic1->idx == cur_IO->tar_idx){
            if(cur_IO->rr_valid_count == PPB){
                ll_append(full_head,cur_RR->cur_vic1);
                //printf("app %d to full, fullnum : %d\n",cur_RR->cur_vic1->idx,full_head->blocknum);
            } 
            else {
                ll_append(fblist_head,cur_RR->cur_vic1);
                //printf("app %d to fb, fbnum : %d\n",cur_RR->cur_vic1->idx,full_head->blocknum);
            }
        }
    }
}

void finish_req(rttask* task, IO* cur_IO, meta* metadata, 
                bhead* fblist_head, bhead* rsvlist_head, bhead* full_head,
                GCblock* cur_GC, RRblock* cur_RR){
    
    //called by main function to process I/O request 
    if(cur_IO->type==WR){
        finish_WR(task,cur_IO,metadata,full_head);
    }
    else if (cur_IO->type==RD){
        finish_RD(task,cur_IO, metadata);
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
    else if(cur_IO->type==BWR){
        finish_BWR(task,cur_IO,metadata,full_head);
        printf("finish BWR, timing : %ld, exec : %ld\n",cur_cp,cur_IO->exec);
    }
}

long find_closenum(long cur_cp, long period){
    double mult = (double)cur_cp / (double)period;
    long close_num = (long)ceil(mult) * period;
    //printf("mult : %lf, closest period : %ld\n",mult,close_num);
    return close_num;
}

long find_next_time(rttask* tasks, int tasknum, long cur_dl, long rr_check, long cur_cp, 
                    long* next_w_release, long* next_r_release, long* next_gc_release){
    //find 
    double mult;
    long periods[tasknum];
    long rp, wp, gcp, rrp, temp, temp2, wear_dl;
    long res = -1, task_min = -1;
    int res_task = -1;
    //find closest I/O release period (within task)
    for(int i=0;i<tasknum;i++){
        rp = next_r_release[i];
        wp = next_w_release[i];  
        gcp = next_gc_release[i];
        temp = wp >= rp ? rp : wp;
        temp2 = temp >= gcp ? gcp : temp;
        periods[i] = temp2;
        //printf("[FNT-task][%d] %ld, %ld %ld %ld\n",i,periods[i],rp,wp,gcp);
    }
    //get closest wear leveling period
    rrp = find_closenum(cur_cp+1,rr_check);
    
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
    //printf("task_min : %ld, cur_dl:%ld,rrp:%ld\n",task_min,cur_dl,rrp);
    //compare with I/O deadline and wear leveling release time

    res = res >= rrp ? rrp : res;
    res = res >= cur_dl ? cur_dl : res;
    //printf("res : %ld\n",res);
    return res;
}