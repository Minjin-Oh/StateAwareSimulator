#include "stateaware.h"

extern int rrflag; 

block* assign_write_greedy(rttask* task, int taskidx, int tasknum, meta* metadata,
                           bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    if(cur_b != NULL){
        if(cur_b->fpnum > 0){
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d\n",target);
            cur = ll_condremove(metadata,fblist_head,OLD);
            if (cur != NULL){
                ll_append(write_head,cur);
            } else {
                cur = write_head->head;
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_condremove(metadata,fblist_head,OLD);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = write_head->head;
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_write_FIFO(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    if(cur_b != NULL){
        if(cur_b->fpnum > 0){
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d\n",target);
            cur = ll_pop(fblist_head);
            if (cur != NULL){
                ll_append(write_head,cur);
            } else {
                cur = write_head->head;
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_pop(fblist_head);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = write_head->head;
        }
    }//if state is different, get another write block
    return cur;
}

block* assign_write_ctrl(rttask* task, int taskidx, int tasknum, meta* metadata, 
                         bhead* fblist_head, bhead* write_head, block* cur_b){
    int target;
    block* cur = NULL;
    target = find_writectrl(task,taskidx,tasknum,metadata,fblist_head,write_head);
    if(cur_b != NULL){
        if(metadata->state[target] == metadata->state[cur_b->idx] && cur_b->fpnum > 0){
        //don't have to change wb? just return current block pointer.
        //remember that when current block runs out of fp, we must change block
            printf("do not change wb, left fp : %d\n",cur_b->fpnum);
            return cur_b;
        } else {
            printf("target block : %d, fp : %d\n",target);
            cur = ll_remove(fblist_head,target);
            if (cur != NULL){
                printf("retreived %d\n",cur->idx);
                ll_append(write_head,cur);
            } else {
                cur = ll_findidx(write_head,target);
            }
        }
    } else { //initial case :: cur_b == NULL. find new block
        printf("[INIT]target block : %d\n",target);
        cur = ll_remove(fblist_head,target);
        if (cur != NULL){
            ll_append(write_head,cur);
        } else {
            cur = ll_findidx(write_head,target);
        }
    }//if state is different, get another write block
    return cur;
}

block* write_job_start(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IO* IOqueue, block* cur_target, int wflag){

    //makes write job according to workload and task parameter
    block *cur, *temp;
    int lpa;
    int ppa_dest[tasks[taskidx].wn];  //array to store target ppa
    int ppa_state[tasks[taskidx].wn]; //array to store target block state
    int cur_offset;
    int bnum = 0;
    float exec_sum = 0.0, period = tasks[taskidx].wp;
    //find a target block whenever job initializes
    //do not actually update the write block in job_start phase
    if(wflag == 1){
        cur = assign_write_ctrl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
    }
    else if (wflag == 0){
        if(cur_target == NULL){
            printf("init\n");
            cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        }
        else{ 
            cur = cur_target;
        }
    } 
    else if(wflag == 2){
        if(cur_target == NULL){
            printf("init\n");
            cur = assign_write_greedy(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        }
        else {
            cur = cur_target;
        }
    }
    //save the destination ppa for each write
    //ONLY update blockmanager (reserve free page)
    //page mapping updated later
    cur_offset = PPB - cur->fpnum;
    for (int i=0;i<tasks[taskidx].wn;i++){
        while(cur->fpnum==0){
            temp = ll_remove(write_head,cur->idx);
            if(temp!=NULL){
                ll_append(full_head,temp);
            }
            if(wflag == 1){
                cur = assign_write_ctrl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            }
            else if (wflag == 0){
                cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            }
            else if (wflag == 2){
                cur = assign_write_greedy(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            }
            if(cur==NULL){

                printf("noFP available\n, totalfp : %d\n");
                abort();
            }
            cur_offset = PPB - cur->fpnum;
            printf("current offset : %d\n",cur_offset);
        }
        ppa_dest[i] = cur->idx*PPB + cur_offset;
        ppa_state[i] = metadata->state[cur->idx];
        cur_offset++;
        cur->fpnum--;
    }
    //generate I/O requests.
    for (int i=0;i<tasks[taskidx].wn;i++){
        lpa = IOget(fp_w);
        IOqueue[i].type = WR;
        IOqueue[i].lpa = lpa;
        IOqueue[i].ppa = ppa_dest[i];
        exec_sum += w_exec(ppa_state[i]);
        //printf("gen:%d\n",lpa,ppa_dest[i]);
    }
    //append current block to freeblock list
    metadata->runutils[0][taskidx] = exec_sum / period; 
    
    return cur;
    
}

void write_job_end(rttask task, meta* metadata, IO* IOqueue, int* total_fp){
    //update metadata for finished write job.
    int lpa, ppa, old_ppa, old_block;
    int idx = task.idx;
    for(int i=0;i<task.wn;i++){
        //printf("[T%d][WQ_E]lpa:%d,ppa:%d\n",task.idx,IOqueue[i].lpa,IOqueue[i].ppa);
        //invalidate old ppa
        lpa = IOqueue[i].lpa;
        ppa = IOqueue[i].ppa;
        old_ppa = metadata->pagemap[lpa];
        old_block = (int)(old_ppa/PPB);
        metadata->invnum[old_block]++;
        metadata->invmap[old_ppa] = 1;
        metadata->vmap_task[old_ppa] = -1;
        metadata->total_invalid++;
        //validate new ppa
        metadata->pagemap[lpa] = ppa;
        metadata->rmap[ppa] = lpa;
        (*total_fp) -= 1;
        //update access profile.
        metadata->access_tracker[idx][ppa/PPB] = 1;
        metadata->vmap_task[ppa] = idx;
    }
}

void read_job_start(rttask task, meta* metadata, FILE* fp_r, IO* IOqueue){
    int lpa = IOget(fp_r);
    int target_block[task.rn];
    float exec_sum = 0.0, period = task.rp;
    for(int i=0;i<task.rn;i++){
        IOqueue[i].lpa = lpa;
        IOqueue[i].ppa = -1;
        IOqueue[i].type = RD;
        target_block[i] = lpa/PPB;
        exec_sum += r_exec(metadata->state[target_block[i]]);
    }
    metadata->runutils[1][task.idx] = exec_sum / period;
}

void read_job_end(rttask task, meta* metadata, IO* IOqueue){
    int target_block[task.rn];
    for(int i=0;i<task.rn;i++){
        target_block[i] = metadata->pagemap[IOqueue[i].lpa]/PPB;
        metadata->access_window[target_block[i]]++;
    }
}

void gc_job_start(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, 
                  int write_limit, IO* IOqueue, GCblock* cur_GC, int gcflag){
    //params
    block* cur = full_head->head;
    block* vic = NULL;
    block* rsv;
    int cur_vic_idx = cur->idx;
    int cur_vic_invalid = -1;
    int vic_offset;
    int rsv_offset;
    int vp_count, rtv_lpa;
    int old = get_blockstate_meta(metadata,OLD);
    int yng = get_blockstate_meta(metadata,YOUNG);
    float gc_exec = 0.0, gc_period = tasks[taskidx].gcp;
    //find gc target
    int gc_limit = find_gcctrl(tasks,taskidx,tasknum,metadata,full_head);
#ifdef FORCEDCONTROL
    while(cur!=NULL){
        if(cur_vic_invalid == -1){//initialize block target
            if((metadata->invnum[cur->idx] >= MINRC) &&
               (metadata->state[cur->idx] != old) &&
               (metadata->state[cur->idx] >= write_limit)){
                //only when state is not oldest & invnum + GC is tolerable, update target.
                cur_vic_invalid = metadata->invnum[cur->idx];
                cur_vic_idx = cur->idx;
                vic = cur;
            }
        } else {
            if((metadata->invnum[cur->idx] >= cur_vic_invalid) &&
               (metadata->state[cur->idx] != old) &&
               (metadata->state[cur->idx] >= write_limit)){
                //only when state is not oldest & invnum + GC is tolerable, update target.
                cur_vic_invalid = metadata->invnum[cur->idx];
                cur_vic_idx = cur->idx;
                vic = cur;
            }
        }
        cur = cur->next;
    }
    if(vic == NULL){//if victim is not found, fallback to NORMAL
        cur = full_head->head;
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit) &&
               (metadata->invnum[cur->idx] >= MINRC)){
                cur_vic_idx = cur->idx;
                vic = cur;
            }
            cur = cur->next;
        }
    }
#endif
    if(gcflag == 0){
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit)){
                cur_vic_idx = cur->idx;
                vic = cur;
            }
            cur = cur->next;
        }
    }
    if(gcflag == 1){
        while(cur != NULL){
            if(cur->idx == gc_limit){
                cur_vic_idx = cur->idx;
                vic = cur;
                break;
            }
            cur = cur->next;
        }
    }

    //!gc target found
    printf("target is %d, inv : %d",vic->idx,metadata->invnum[vic->idx]);
    if(vic==NULL){
        printf("[GC]no feasible block\n");
        abort();
    }
    if(metadata->invnum[vic->idx]==0){
        printf("no fp block\n");
        return;
    }//edge cases

    ll_remove(full_head,vic->idx);
    rsv = ll_pop(rsvlist_head);

    //make relocation I/O jobs.
    vic_offset = PPB*(vic->idx);
    rsv_offset = PPB*(rsv->idx);
    vp_count = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[vic_offset+i]==0){
            IOqueue[vp_count].type = GC;
            rtv_lpa = metadata->rmap[vic_offset+i];
            IOqueue[vp_count].gc_old_lpa = rtv_lpa;
            IOqueue[vp_count].gc_tar_ppa = rsv_offset+vp_count;
            IOqueue[vp_count].gc_vic_ppa = vic_offset+i;
            vp_count++;
            //printf("[GCIO]lpa:%d,mov %d to %d\n",rtv_lpa,vic_offset+i,rsv_offset+vp_count);
            gc_exec += r_exec(metadata->state[vic->idx])+w_exec(metadata->state[rsv->idx]);
        }
    }
    for(int i=vp_count;i<PPB;i++){
        IOqueue[i].type = -1;
    }
    printf("[GC_S]cpb necessary for %d\n",vp_count);
    //update blockmanager (reserve pages)
    cur_GC->cur_rsv = rsv;
    cur_GC->cur_vic = vic;
    rsv->fpnum = PPB - vp_count;
    vic->fpnum = PPB;
    //make sure we use "append", so that it does not immediately get popped.
    //printf("[%d][GC_S]%d to %d\n",taskidx,cur_GC->cur_vic->idx,cur_GC->cur_rsv->idx);
    gc_exec += e_exec(metadata->state[vic->idx]);
    //update runtime util
    metadata->runutils[2][taskidx] = gc_exec / gc_period;
}

void gc_job_end(rttask* tasks, int taskidx, int tasknum, meta* metadata, IO* IOqueue,
                bhead* fblist_head, bhead* rsvlist_head,
                GCblock* cur_GC, int* total_fp){
    int old_ppa, new_ppa, old_lpa, vp_count;
    int vicidx = cur_GC->cur_vic->idx, rsvidx = cur_GC->cur_rsv->idx;
    //printf("[%d][GC_E]%d to %d\n",taskidx,vicidx,rsvidx);
    metadata->invnum[rsvidx] = 0;
    vp_count = 0;
    for(int i=0;i<PPB;i++){
        if(IOqueue[i].type == GC){
            old_lpa = IOqueue[i].gc_old_lpa;
            new_ppa = IOqueue[i].gc_tar_ppa;
            old_ppa = IOqueue[i].gc_vic_ppa;
            if(metadata->pagemap[old_lpa] == old_ppa){
                metadata->pagemap[old_lpa] = new_ppa;
                metadata->rmap[new_ppa] = old_lpa;
                metadata->invmap[new_ppa] = 0;
                metadata->vmap_task[new_ppa] = metadata->vmap_task[old_ppa];
                vp_count++;
            } else{//if data is updated, skip pagemap update
                metadata->rmap[new_ppa] = -1;
                metadata->invmap[new_ppa] = 1;
                metadata->invnum[rsvidx]++;
                metadata->vmap_task[new_ppa] = -1;
                vp_count++;
            }
        }
        //reset victim block
        metadata->rmap[(vicidx)*PPB+i]=-1;
        metadata->invmap[(vicidx)*PPB+i]=0;
        metadata->vmap_task[vicidx*PPB+i]=-1;
    }
    for(int i=vp_count;i<PPB;i++){
        //reset free page info
        metadata->rmap[rsvidx*PPB+i]=-1;
        metadata->invmap[rsvidx*PPB+i]=0;
        metadata->vmap_task[rsvidx*PPB+i] = -1;
    }

    //update metadata
    metadata->total_invalid -= PPB - vp_count;
    metadata->invnum[vicidx] = 0;
    metadata->state[vicidx]++;
    for(int i=0;i<tasknum;i++){
        //transfer access data from vic to rsv.
        //FIXME::simply moving a access tracker is not exact solution
        int temp = metadata->access_tracker[i][vicidx];
        metadata->access_tracker[i][rsvidx] = temp;
        metadata->access_tracker[i][vicidx] = 0;
    }
    *total_fp += PPB - vp_count;
    //release the block to blocklist.
    ll_append(fblist_head,cur_GC->cur_rsv);
    ll_append(rsvlist_head,cur_GC->cur_vic);
    
}

void RR_job_start(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, 
                  IO* IOqueue, RRblock* cur_RR){
    int vic1, vic2, v1_state, v2_state;
    int v1_offset, v2_offset, v1_cnt=0, v2_cnt=0;
    int lpa;
    int queue_ptr=0;
    float execution_time = 0.0;
    block *vb1, *vb2, *cur;
    meta temp;
    printf("rrflag is %d\n",rrflag);
    if(rrflag == 0){
        find_RR_target(tasks, tasknum, metadata,fblist_head,full_head,&vic1,&vic2);
    }
    else if (rrflag == 1){
        find_RR_target_util(tasks, tasknum, metadata,fblist_head,full_head,&vic1,&vic2);
    }
    cur = full_head->head;
    while(cur != NULL){
        if(cur->idx == vic1){
            vb1 = cur;
        }
        if(cur->idx == vic2){
            vb2 = cur;
        }
        cur=cur->next;
    }
    cur = fblist_head->head;
    while(cur != NULL){
        if(cur->idx == vic1){
            vb1 = cur;
        }
        if(cur->idx == vic2){
            vb2 = cur;
        }
        cur=cur->next;
    }
    v1_offset = PPB*vic1;
    v2_offset = PPB*vic2;
    v1_state = metadata->state[vb1->idx];
    v2_state = metadata->state[vb2->idx];
    memcpy(&temp,metadata,sizeof(meta));
    //generate relocation requests.
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v1_offset+i]==0){
            lpa=temp.rmap[v1_offset+i];
            IOqueue[queue_ptr].type = RR;
            IOqueue[queue_ptr].rr_old_lpa = lpa;
            IOqueue[queue_ptr].rr_vic_ppa = v1_offset+i;
            IOqueue[queue_ptr].rr_tar_ppa = v2_offset+v1_cnt;
            IOqueue[queue_ptr].vmap_task = metadata->vmap_task[v1_offset+i];
            v1_cnt++;
            queue_ptr++;
            execution_time += r_exec(v1_state) + w_exec(v2_state);
        }
    }
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v2_offset+i]==0){
            lpa=temp.rmap[v2_offset+i];
            IOqueue[queue_ptr].type = RR;
            IOqueue[queue_ptr].rr_old_lpa = lpa;
            IOqueue[queue_ptr].rr_vic_ppa = v2_offset+i;
            IOqueue[queue_ptr].rr_tar_ppa = v1_offset+v2_cnt;
            IOqueue[queue_ptr].vmap_task = metadata->vmap_task[v2_offset+i];
            v2_cnt++;
            queue_ptr++;
            execution_time += r_exec(v2_state) + w_exec(v1_state);
        }
    }
    for(int i=queue_ptr;i<2*PPB;i++){
        IOqueue[i].type = -1;
    }
    //remove target block from blocklist & update block info.
    if(is_idx_in_list(full_head,vic1)){
        vb1 = ll_remove(full_head,vic1);
    }
    else if(is_idx_in_list(fblist_head,vic1)){
        vb1 = ll_remove(fblist_head,vic1);
    }
    if(is_idx_in_list(full_head,vic2)){
        vb2 = ll_remove(full_head,vic2);
    }
    else if(is_idx_in_list(fblist_head,vic2)){
        vb2 = ll_remove(fblist_head,vic2);    
    }
    vb1->fpnum = PPB-v2_cnt;
    vb2->fpnum = PPB-v1_cnt;
    cur_RR->cur_vic1 = vb1;
    cur_RR->cur_vic2 = vb2;
    cur_RR->execution_time = execution_time;
    execution_time += e_exec(v1_state) + e_exec(v2_state);
    //validation(print the info)
    for(int i=0;i<2*PPB;i++){
        //if(IOqueue[i].type == RR){
        //    printf("[%d]lpa=%d, %d to %d\n",
        //        i,IOqueue[i].rr_old_lpa,IOqueue[i].rr_vic_ppa,IOqueue[i].rr_tar_ppa);
        //}
    }
    printf("[RR_S]swap %d and %d,v1_cnt + v2_cnt = %d\n",cur_RR->cur_vic1->idx,cur_RR->cur_vic2->idx,v1_cnt+v2_cnt);
    printf("[RR_S]%d + %d, %d + %d\n",v1_cnt, cur_RR->cur_vic1->fpnum, v2_cnt,cur_RR->cur_vic2->fpnum);
}

void RR_job_end(meta* metadata, bhead* fblist_head, bhead* full_head, 
                  IO* IOqueue, RRblock* cur_RR, int* total_fp){
    int old_lpa, new_ppa, old_ppa;
    int vb1 = cur_RR->cur_vic1->idx, vb2 = cur_RR->cur_vic2->idx;
    int vb1_count=0, vb2_count=0;

    printf("[RR_E]passed value : %d, %d\n",vb1,vb2);
    printf("[RR_E](bef)fp : %d, inv : %d\n",*total_fp,metadata->total_invalid);
    metadata->invnum[vb1] = 0;
    metadata->invnum[vb2] = 0;
    for(int i=0;i<2*PPB;i++){
        if(IOqueue[i].type == RR){
            old_lpa = IOqueue[i].rr_old_lpa;
            new_ppa = IOqueue[i].rr_tar_ppa;
            old_ppa = IOqueue[i].rr_vic_ppa;
            if(metadata->pagemap[old_lpa] == old_ppa){
                metadata->pagemap[old_lpa] = new_ppa;
                metadata->rmap[new_ppa] = old_lpa;
                metadata->invmap[new_ppa] = 0;
                metadata->vmap_task[new_ppa] = IOqueue[i].vmap_task;
            }
            
            else{//for updated data, skip pagemapping
                metadata->rmap[new_ppa] = -1;
                metadata->invmap[new_ppa] = 1;
                metadata->vmap_task[new_ppa] = -1;
                if(new_ppa/PPB == vb1)
                    metadata->invnum[vb1] += 1;
                else if(new_ppa/PPB == vb2)
                    metadata->invnum[vb2] += 1;
            }
            if(new_ppa/PPB == vb1)
                vb1_count++;
            else if(new_ppa/PPB == vb2)
                vb2_count++;
        }
    }
    for(int i=vb1_count;i<PPB;i++){
        metadata->rmap[vb1*PPB+i] = -1;
        metadata->invmap[vb1*PPB+i] = 0;
        metadata->vmap_task[vb1*PPB+i] = -1;
    }
    for(int i=vb2_count;i<PPB;i++){
        metadata->rmap[vb2*PPB+i] = -1;
        metadata->invmap[vb2*PPB+i] = 0;
        metadata->vmap_task[vb2*PPB+i] = -1;
    }
    printf("[RR_E]%d + %d, %d + %d\n",vb1_count, cur_RR->cur_vic1->fpnum, vb2_count,cur_RR->cur_vic2->fpnum);
    metadata->total_invalid -= PPB - vb1_count;
    metadata->total_invalid -= PPB - vb2_count;
    *total_fp = *total_fp + cur_RR->cur_vic1->fpnum;
    *total_fp = *total_fp + cur_RR->cur_vic2->fpnum;
    printf("[RR_E](aft)mfp : %d, inv : %d\n",*total_fp,metadata->total_invalid);
    metadata->state[vb1] += 1;
    metadata->state[vb2] += 1;

    if(cur_RR->cur_vic1->fpnum != 0){      
        ll_append(fblist_head,cur_RR->cur_vic1);
    }
    else if(cur_RR->cur_vic1->fpnum == 0){
        ll_append(full_head,cur_RR->cur_vic1);
    }
    if(cur_RR->cur_vic2->fpnum != 0){
        ll_append(fblist_head,cur_RR->cur_vic2);
    }
    else if(cur_RR->cur_vic2->fpnum == 0){
        ll_append(full_head,cur_RR->cur_vic2);
    }
}

/*
    for (int i=0;i<tasks[taskidx].wn;i++){
        while(cur->fpnum == 0){ //if page reached its end
            printf("====looping====\n");
            printf("cur block : %d, fp : %d\n",cur->idx,cur->fpnum);
            temp = ll_remove(write_head,cur->idx);
            printf("remove %d from write\n",temp->idx);
            if (temp != NULL){//this is necessary, because multiple I/O can be directed to same wb.
                printf("append %d to full\n",temp->idx);
                ll_append(full_head,temp);
            }
            cur = assign_write(metadata,fblist_head,write_head);
            //cur = assign_write_ctrl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            if(cur == NULL){//if it is still NULL, no free space.
                printf("no free page available\n");
                abort();
            }
            cur_offset = PPB - cur->fpnum;
            printf("====looping end====\n");
        }
        ppa_dest[i] = cur->idx*PPB + cur_offset;
        ppa_state[i] = metadata->state[cur->idx];
        cur_offset++;
        cur->fpnum--;
    }
    */
/*
block* assign_write(meta* metadata,bhead* fblist_head,bhead* write_head){
    block* cur = NULL;
    cur = ll_condremove(metadata,fblist_head,YOUNG);
    //try to find new write block from fblist.
    printf("rtv %d from fb\n",cur->idx);
    if (cur != NULL){
        ll_append(write_head,cur);
    } else if (cur == NULL){
        cur = ll_find(metadata,write_head,YOUNG);
        printf("use %d from wb\n",cur->idx);
    }
    return cur;
}
*/