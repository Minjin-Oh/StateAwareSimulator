#include "stateaware.h"
#include "findGC.h"
#include "findRR.h"
#include "assignW.h"
#include "rrsim_q.h"
#include "IOgen.h"

extern int rrflag;
block* write_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp){

    //makes write job according to workload and task parameter
    block *cur, *temp;
    int lpa;
    int ppa_dest[tasks[taskidx].wn];  //array to store target ppa
    int ppa_state[tasks[taskidx].wn]; //array to store target block state
    int lpas[tasks[taskidx].wn];      //array to store lpa of write req
    int cur_offset;
    int bnum = 0;
    float exec_sum = 0.0, period = (float)tasks[taskidx].wp;

    //if current fp cannot handle reserved write + current write, abort releasing new write job.
    
    for(int i=0;i<tasks[taskidx].wn;i++){
        lpas[i] = IOget(fp_w);
    }
    //find a target block whenever job initializes
    //do not actually update the write block in job_start phase
    if(wflag == 1){
        cur = assign_write_ctrl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
    } else if (wflag == 0){
        if(cur_target == NULL){
            cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else { 
            cur = cur_target;
        }
    } else if(wflag == 2){
        if(cur_target == NULL){
            cur = assign_write_greedy(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else {
            cur = cur_target;
        }
    } else if (wflag == 3){
        cur = assign_writelimit(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas);
    } else if (wflag == 4){
        cur = assign_writeweighted(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,0);
    } else if (wflag == 5){
        cur = assign_writefixed(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
    } else if (wflag == 6){
        cur = assign_writehotness(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,0);
    } else if (wflag == 7){
        if(cur_target == NULL){
            cur = assign_write_old(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else {
            cur = cur_target;
        }
    } else if (wflag == 8){//write motiv policies
        cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[0],wflag);
    } else if (wflag == 9){//write motiv policies
        cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[0],wflag);
    } else if (wflag == 10){//write motiv policies
        cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[0],wflag);
    } else if (wflag == 11){//write motiv policies
        cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[0],wflag);
    } else if (wflag == 12){
        cur = assign_write_gradient(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,0);
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
            } else if (wflag == 0){
                cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            } else if (wflag == 2){
                cur = assign_write_greedy(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            } else if (wflag == 3){
                cur = assign_writelimit(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas);
            } else if (wflag == 4){
                cur = assign_writeweighted(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas, i);
            } else if (wflag == 5){
                cur = assign_writefixed(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            } else if (wflag == 6){
                cur = assign_writehotness(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,i);
            } else if (wflag == 7){
                cur = assign_write_old(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
            } else if (wflag == 8){
                cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[i],wflag);
            } else if (wflag == 9){
                cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[i],wflag);
            } else if (wflag == 10){//write motiv policies
                cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[i],wflag);
            } else if (wflag == 11){//write motiv policies
                cur = assign_writehot_motiv(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas[i],wflag);
            } else if (wflag == 12){
                cur = assign_write_gradient(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,i);
            }
            if(cur==NULL){
                printf("noFP available\n, totalfp : %d\n");
                abort();
            }
            cur_offset = PPB - cur->fpnum;
            //printf("current offset : %d\n",cur_offset);
        }
        ppa_dest[i] = cur->idx*PPB + cur_offset;
        ppa_state[i] = metadata->state[cur->idx];
        cur_offset++;
        cur->fpnum--;
    }
    
    //assign page write destination
     //generate I/O requests.
    for (int i=0;i<tasks[taskidx].wn;i++){
        IO* req = (IO*)malloc(sizeof(IO));
        lpa = lpas[i];
        metadata->write_cnt[lpa]++;
        metadata->write_cnt_task[taskidx]++;
        metadata->tot_write_cnt++;
        metadata->reserved_write++;
        req->type = WR;
        req->taskidx = taskidx;
        req->lpa = lpa;
        req->ppa = ppa_dest[i];
        req->IO_start_time = cur_cp;
        req->deadline = (long)cur_cp + (long)tasks[taskidx].wp;
        req->exec = (long)floor((double)w_exec(ppa_state[i]));
        if(i != tasks[taskidx].wn-1){
            req->islastreq = 0;    
        } else {
            req->islastreq = 1;
        }                           
        ll_append_IO(wq,req);
        exec_sum += w_exec(ppa_state[i]);
        if(i==0){
            req->init = 1;
            req->last = 0;    
        } else if (i == tasks[taskidx].wn-1){
            req->init = 0;
            req->last = 1;
        } else {
            req->init = 0;
            req->last = 0;
        }
        //printf("tp:%d,lpa:%d,ppa:%d,dl:%ld,exec:%ld\n",req->type,req->lpa,req->ppa,req->deadline,req->exec);
        
    }
        
    //update runtime utilization
    metadata->runutils[0][taskidx] = exec_sum / period;
    //sleep(1);
    return cur;
}

void read_job_start_q(rttask* task, int taskidx, meta* metadata, FILE* fp_r, IOhead* rq, long cur_cp){
    int lpa;
    int target_block[task[taskidx].rn];
    float exec_sum = 0.0, period = (float)task[taskidx].rp;
    for(int i=0;i<task[taskidx].rn;i++){
        lpa = IOget(fp_r);
        IO* req = (IO*)malloc(sizeof(IO));
        req->type = RD;
        req->taskidx = taskidx;
        req->lpa = lpa;
        req->ppa = metadata->pagemap[lpa];
        req->IO_start_time = cur_cp;
        target_block[i] = req->ppa/PPB;
        
        req->deadline = (long)cur_cp + (long)task[taskidx].rp;
        req->exec = (long)floor((double)r_exec(metadata->state[target_block[i]]));
        exec_sum += (long)floor((double)r_exec(metadata->state[target_block[i]]));
        ll_append_IO(rq,req);
        if(i==0){
            req->init = 1;
            req->last = 0;    
        } else if (i == task[taskidx].rn-1){
            req->init = 0;
            req->last = 1;
        } else {
            req->init = 0;
            req->last = 0;
        }
        metadata->read_cnt[lpa]++;
        metadata->read_cnt_task[taskidx]++;
        metadata->tot_read_cnt++;
    }
    metadata->runutils[1][taskidx] = exec_sum / period;
}

void gc_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, 
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag, long cur_cp){
    if(gcq->reqnum != 0){
        //printf("[%ld]queue not empty, dl miss detected. task %d GC\n",cur_cp,taskidx);
        //sleep(3);
        //abort();
    }
    //params
    block* cur = full_head->head;
    block* vic = NULL;
    block* rsv;
    int cur_vic_idx = cur->idx;
    int cur_vic_invalid = -1;
    int vic_offset;
    int rsv_offset;
    int gc_limit;
    int vp_count, rtv_lpa;
    int old = get_blockstate_meta(metadata,OLD);
    int yng = get_blockstate_meta(metadata,YOUNG);
    float gc_exec = 0.0, gc_period = (float)tasks[taskidx].gcp;

    //find gc target
    if (gcflag == 1){
        gc_limit = find_gcctrl(tasks,taskidx,tasknum,metadata,full_head);
    } else if (gcflag == 2){
        gc_limit = find_gcctrl_greedy(tasks,taskidx,tasknum,metadata,full_head);
    } else if (gcflag == 3){
        gc_limit = find_gcctrl_limit(tasks,taskidx,tasknum,metadata,full_head,rsvlist_head);
    } else if (gcflag == 4){
        gc_limit = find_gcctrl_yng(tasks,taskidx,tasknum,metadata,full_head);
    } else if (gcflag == 5){
        gc_limit = find_gcweighted(tasks,taskidx,tasknum,metadata,full_head,rsvlist_head);
    } else if (gcflag == 6){
        gc_limit = find_gc_utilsort(tasks,taskidx,tasknum,metadata,full_head,rsvlist_head);
    }
    
    //if gc baseline, select most invalid block
    if (gcflag == 0){
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit)){
                cur_vic_idx = cur->idx;
                vic = cur;
            }
            cur = cur->next;
        }
    }
    //if not, choose a victim block from full_block list using index
    else if (gcflag == 1 || gcflag == 2 || gcflag == 3 || gcflag == 4 || gcflag == 5 || gcflag == 6){
        //printf("target block : %d\n",gc_limit);
        while(cur != NULL){
            if(cur->idx == gc_limit){
                cur_vic_idx = cur->idx;
                vic = cur;
                break;
            }
            cur = cur->next;
        }
        //sleep(1);
    }
    //printf("target is %d, inv : %d",vic->idx,metadata->invnum[vic->idx]);

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
    if(rsv == NULL || vic == NULL){
        printf("[%ld]gc fucked up\n");
        abort();
    }

    //make copyback requests
    vic_offset = PPB*(vic->idx);
    rsv_offset = PPB*(rsv->idx);
    vp_count = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[vic_offset+i]==0 && metadata->rmap[vic_offset+i]!=-1){
            IO* req = (IO*)malloc(sizeof(IO));
            req->type = GC;
            req->last = 0;
            req->taskidx = taskidx;
            rtv_lpa = metadata->rmap[vic_offset+i];
            req->gc_old_lpa = rtv_lpa;
            req->gc_tar_ppa = rsv_offset+vp_count;
            req->gc_vic_ppa = vic_offset+i;
            req->IO_start_time = cur_cp;
            req->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
            req->exec = (long)floor((double)r_exec(metadata->state[vic->idx])) + 
                        (long)floor((double)w_exec(metadata->state[rsv->idx]));
            ll_append_IO(gcq,req);
            vp_count++;
            gc_exec += req->exec;
        }
    }

    //append erase operation at the end
    IO* er = (IO*)malloc(sizeof(IO));
    er->type = GCER;
    er->last = 1;
    er->taskidx = taskidx;
    er->vic_idx = vic->idx;
    er->rsv_idx = rsv->idx;
    er->IO_start_time = cur_cp;
    er->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
    er->exec = (long)floor((double)e_exec(metadata->state[vic->idx]));
    er->gc_valid_count = vp_count;
    gc_exec += er->exec;
    ll_append_IO(gcq,er);
    //printf("tp:%d,target block :%d,vcount: %d, exec : %d\n",er->type,er->vic_idx,er->gc_valid_count,er->exec);
    //update blockmanager (reserve pages)
    cur_GC->cur_rsv = rsv;
    cur_GC->cur_vic = vic;
    rsv->fpnum = PPB - vp_count;
    vic->fpnum = PPB;
    
    //update runtime util
    metadata->runutils[2][taskidx] = gc_exec / gc_period;
    //sleep(1);
}

void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, bhead* hotlist, bhead* coldlist,
                  IOhead* rrq, RRblock* cur_RR, double rrutil, long cur_cp){
    int vic1, vic2, v1_state, v2_state;
    int v1_offset, v2_offset, v1_cnt=0, v2_cnt=0;
    int lpa;
    long rrp;
    long execution_time = 0L;
    block *vb1 = NULL, *vb2 = NULL, *cur;
    meta temp;
    
    //find relocation victim blocks
    if(rrflag == 0){
        find_RR_dualpool(tasks, tasknum, metadata, full_head, hotlist, coldlist, &vic1, &vic2);
    }
    else if (rrflag == 1){
        find_RR_target_util(tasks, tasknum, metadata,fblist_head,full_head,&vic1,&vic2);
    }
    //cancel WL
    if(vic1 == -1 || vic2 == -1){
        //printf("skiprr\n");
        return;
    }
    
    //if decided to do WL, reset access window for future.

    for(int i=0;i<NOB;i++){
        metadata->access_window[i] = 0;
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
    v1_cnt = PPB - metadata->invnum[vic1];
    v2_cnt = PPB - metadata->invnum[vic2];
    //find data relocation period
    rrp = find_RR_period(vic1,vic2,v1_cnt,v2_cnt,rrutil, metadata);
    //edge case : if read relocation util is lower than 0.0, assign longest possible deadline.
    if(rrutil <= 0.0){
        rrp = __LONG_MAX__ - cur_cp;
    }
    //copy the metadata into temp param and use temp
    //make sure that metadata update do NOT generate concurrency issue.
    memcpy(&temp,metadata,sizeof(meta));

    //generate relocation requests.
    execution_time += gen_read_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);
    //make erase request.
    execution_time += gen_erase_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);
    //make write request
    execution_time += gen_write_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);

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
    cur_RR->rrcheck = cur_cp + rrp;
    if(rrutil <= 0.0){
        cur_RR->rrcheck = cur_cp + find_RR_period(vic1,vic2,v1_cnt,v2_cnt,0.025,metadata);
    }
    //printf("[RR_S]util : %f, exec : %ld, period : %ld, cur_cp : %ld, rrcheck : %ld\n",rrutil,execution_time,rrp,cur_cp,cur_cp+rrp);
    //printf("[RR_S]swap %d and %d,v1_cnt + v2_cnt = %d\n",cur_RR->cur_vic1->idx,cur_RR->cur_vic2->idx,v1_cnt+v2_cnt);
    //printf("[RR_S]%d + %d, %d + %d\n",v1_cnt, cur_RR->cur_vic1->fpnum, v2_cnt,cur_RR->cur_vic2->fpnum);

}