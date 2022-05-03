#include "stateaware.h"

extern int rrflag;

block* write_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp){

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
        IO* req = (IO*)malloc(sizeof(IO));
        req->type = WR;
        req->lpa = lpa;
        req->ppa = ppa_dest[i];
        req->deadline = (long)cur_cp + (long)tasks[taskidx].wp;
        ll_append_IO(wq,req);
        exec_sum += w_exec(ppa_state[i]);
    }
    //append current block to freeblock list
    metadata->runutils[0][taskidx] = exec_sum / period; 
    return cur;
}

void read_job_start_q(rttask task, meta* metadata, FILE* fp_r, IOhead* rq){
    int lpa = IOget(fp_r);
    int target_block[task.rn];
    float exec_sum = 0.0, period = task.rp;
    for(int i=0;i<task.rn;i++){
        IO* req = (IO*)malloc(sizeof(IO));
        req->type = RD;
        req->lpa = lpa;
        req->ppa = -1;
        ll_append_IO(rq,req);
        target_block[i] = lpa/PPB;
        exec_sum += r_exec(metadata->state[target_block[i]]);
    }
    metadata->runutils[1][task.idx] = exec_sum / period;
}

void gc_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, 
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag){
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

    if(gcflag == 0){
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit)){
                cur_vic_idx = cur->idx;
                vic = cur;
            }
            cur = cur->next;
        }
    } else if(gcflag == 1){
        while(cur != NULL){
            if(cur->idx == gc_limit){
                cur_vic_idx = cur->idx;
                vic = cur;
                break;
            }
            cur = cur->next;
        }
    }//!gc target found
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
            IO* req = (IO*)malloc(sizeof(IO));
            req->type = GC;
            rtv_lpa = metadata->rmap[vic_offset+i];
            req->gc_old_lpa = rtv_lpa;
            req->gc_tar_ppa = rsv_offset+vp_count;
            req->gc_vic_ppa = vic_offset+i;
            ll_append_IO(gcq,req);
            vp_count++;
            //printf("[GCIO]lpa:%d,mov %d to %d\n",rtv_lpa,vic_offset+i,rsv_offset+vp_count);
            gc_exec += r_exec(metadata->state[vic->idx])+w_exec(metadata->state[rsv->idx]);
        }
    }
    IO* er = (IO*)malloc(sizeof(IO));
    er->type = ER;
    ll_append_IO(gcq,er);
    
    //update blockmanager (reserve pages)
    cur_GC->cur_rsv = rsv;
    cur_GC->cur_vic = vic;
    rsv->fpnum = PPB - vp_count;
    vic->fpnum = PPB;
    gc_exec += e_exec(metadata->state[vic->idx]);
    
    //update runtime util
    metadata->runutils[2][taskidx] = gc_exec / gc_period;
}

void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, 
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