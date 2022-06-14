#include "stateaware.h"
#include "findGC.h"
#include "findRR.h"
#include "rrsim_q.h"

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
            //printf("init\n");
            cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        }
        else{ 
            cur = cur_target;
        }
    } 
    else if(wflag == 2){
        if(cur_target == NULL){
            //printf("init\n");
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
        lpa = IOget(fp_w);
        req->type = WR;
        req->taskidx = taskidx;
        req->lpa = lpa;
        req->ppa = ppa_dest[i];
        req->deadline = (long)cur_cp + (long)tasks[taskidx].wp;
        req->exec = (long)floor((double)w_exec(ppa_state[i]));
        ll_append_IO(wq,req);
        exec_sum += w_exec(ppa_state[i]);
        printf("tp:%d,lpa:%d,ppa:%d,dl:%ld,exec:%ld\n",req->type,req->lpa,req->ppa,req->deadline,req->exec);
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
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag, int cur_cp){
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
    }

    //edge cases
    printf("target is %d, inv : %d",vic->idx,metadata->invnum[vic->idx]);

    if(vic==NULL){
        printf("[GC]no feasible block\n");
        abort();
    }
    if(metadata->invnum[vic->idx]==0){
        printf("no fp block\n");
        return;
    }

    //remove the block from blocklist. prevent concurrency issue
    ll_remove(full_head,vic->idx);
    rsv = ll_pop(rsvlist_head);

    //make copyback requests
    vic_offset = PPB*(vic->idx);
    rsv_offset = PPB*(rsv->idx);
    vp_count = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[vic_offset+i]==0){
            IO* req = (IO*)malloc(sizeof(IO));
            req->type = GC;
            req->taskidx = taskidx;
            rtv_lpa = metadata->rmap[vic_offset+i];
            req->gc_old_lpa = rtv_lpa;
            req->gc_tar_ppa = rsv_offset+vp_count;
            req->gc_vic_ppa = vic_offset+i;
            req->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
            req->exec = (long)floor((double)r_exec(metadata->state[vic->idx])) + 
                        (long)floor((double)w_exec(metadata->state[rsv->idx]));
            ll_append_IO(gcq,req);
            vp_count++;
            printf("tp:%d,lpa:%d,ppa:%d,dl:%ld,exec:%ld\n",req->type,req->gc_old_lpa,req->gc_tar_ppa,req->deadline,req->exec);
            gc_exec += req->exec;
        }
    }

    //append erase operation at the end
    IO* er = (IO*)malloc(sizeof(IO));
    er->type = GCER;
    er->taskidx = taskidx;
    er->vic_idx = vic->idx;
    er->rsv_idx = rsv->idx;
    er->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
    er->exec = (long)floor((double)e_exec(metadata->state[vic->idx]));
    er->gc_valid_count = vp_count;
    gc_exec += er->exec;
    ll_append_IO(gcq,er);
    printf("tp:%d,target block :%d,vcount: %d, exec : %d\n",er->type,er->vic_idx,er->gc_valid_count,er->exec);
    //update blockmanager (reserve pages)
    cur_GC->cur_rsv = rsv;
    cur_GC->cur_vic = vic;
    rsv->fpnum = PPB - vp_count;
    vic->fpnum = PPB;
    
    //update runtime util
    metadata->runutils[2][taskidx] = gc_exec / gc_period;
    //sleep(1);
}

void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, 
                  IOhead* rrq, RRblock* cur_RR, double rrutil, int cur_cp){
    int vic1, vic2, v1_state, v2_state;
    int v1_offset, v2_offset, v1_cnt=0, v2_cnt=0;
    int lpa;
    long rrp;
    float execution_time = 0.0;
    block *vb1 = NULL, *vb2 = NULL, *cur;
    meta temp;
    
    //find relocation victim blocks
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
    
    //find data relocation period
    rrp = find_RR_period(vic1,vic2,PPB - metadata->invnum[vic1], PPB - metadata->invnum[vic2], 0.05, metadata);

    //copy the metadata into temp param and use temp
    //make sure that metadata update do NOT generate concurrency issue.
    memcpy(&temp,metadata,sizeof(meta));

    //generate relocation requests.
    execution_time += gen_read_rr(vic1,vic2,cur_cp,rrp,temp,rrq);
    //make erase request.
    execution_time += gen_erase_rr(vic1,vic2,cur_cp,rrp,temp,rrq);
    //make write request
    execution_time += gen_write_rr(vic1,vic2,cur_cp,rrp,temp,rrq);

    //remove target block from blocklist & update block info.
    if(is_idx_in_list(full_head,vic1)){
        vb1 = ll_remove(full_head,vic1);
    }
    if(is_idx_in_list(full_head,vic2)){
        vb2 = ll_remove(full_head,vic2);
    }
    if(vb1 == NULL || vb2 == NULL){
        printf("block selection fucked up\n");
        abort();
    }

    //update block info & allocate pointer to tracker
    vb1->fpnum = PPB-v2_cnt;
    vb2->fpnum = PPB-v1_cnt;
    cur_RR->cur_vic1 = vb1;
    cur_RR->cur_vic2 = vb2;
    cur_RR->execution_time = execution_time;

    printf("[RR_S]swap %d and %d,v1_cnt + v2_cnt = %d\n",cur_RR->cur_vic1->idx,cur_RR->cur_vic2->idx,v1_cnt+v2_cnt);
    printf("[RR_S]%d + %d, %d + %d\n",v1_cnt, cur_RR->cur_vic1->fpnum, v2_cnt,cur_RR->cur_vic2->fpnum);

}