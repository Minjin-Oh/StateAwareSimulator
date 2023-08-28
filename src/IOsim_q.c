#include "stateaware.h"
#include "findGC.h"
#include "findRR.h"
#include "assignW.h"
#include "rrsim_q.h"
#include "IOgen.h"

extern int rrflag;
extern bhead* glob_yb;
extern bhead* glob_ob;

void make_req_gc(meta* metadata, rttask* tasks, int taskidx, long cur_cp, block* vic, block* rsv, IOhead* gcq, bhead* full_head, bhead* write_head, bhead* fblist_head, GCblock* cur_GC){
    //a function which 1. makes gc request and 2. inserts request into gc request queue
    //in this function, copyback operation is done on writeblock, using find_gc_destination() function
    int rtv_lpa;
    int vic_offset = PPB*(vic->idx);
    int vp_count = 0;
    long gc_exec = 0;
    block* cur = NULL;

    ll_remove(full_head,vic->idx);

    for(int i=0;i<PPB;i++){
        if(metadata->invmap[vic_offset+i]==0 && metadata->rmap[vic_offset+i]!=-1){
            IO* req = (IO*)malloc(sizeof(IO));
            req->type = GC;
            req->last = 0;
            req->taskidx = taskidx;
            rtv_lpa = metadata->rmap[vic_offset+i];
            req->gc_old_lpa = rtv_lpa;
            if(cur != NULL){
                if(cur->fpnum == 0){
                    ll_remove(write_head,cur->idx);
                    ll_append(full_head,cur);
                    cur = NULL;
                }
            }
            //printf("rtv_lpa : %d\n",rtv_lpa);
            cur = find_gc_destination(metadata,rtv_lpa,cur_cp,fblist_head,write_head);  
            req->gc_tar_ppa = (PPB*cur->idx) + PPB - cur->fpnum;
            cur->fpnum--;
            //printf("target block : %d, ppa : %d\n",cur->idx,req->gc_tar_ppa);
            if(cur->fpnum == 0){
                ll_remove(write_head,cur->idx);
                ll_append(full_head,cur);
                cur = NULL;
            }
            req->gc_vic_ppa = vic_offset+i;
            req->IO_start_time = cur_cp;
            req->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
            req->exec = (long)floor((double)r_exec(metadata->state[vic->idx])) + 
                        (long)floor((double)w_exec(metadata->state[req->gc_tar_ppa / PPB]));
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
    er->IO_start_time = cur_cp;
    er->deadline = (long)cur_cp + (long)(tasks[taskidx].gcp);
    er->exec = (long)floor((double)e_exec(metadata->state[vic->idx]));
    er->gc_valid_count = vp_count;
    gc_exec += er->exec;
    ll_append_IO(gcq,er);
    cur_GC->cur_vic = vic;

    //update blockmanager (reserve pages)
    vic->fpnum = PPB;
    metadata->runutils[2][taskidx] = (double)gc_exec / (double)tasks[taskidx].gcp;
}

block* write_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                     bhead* fblist_head, bhead* full_head, bhead* write_head,
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp){

    //makes write job according to workload and task parameter
    printf("[%d]write start, time : %d\n",taskidx,cur_cp);
    block *cur = NULL;
    block *temp = NULL;
    block *last_access_block = NULL;
    int lpa;
    int ppa_dest[tasks[taskidx].wn];  //array to store target ppa
    int ppa_state[tasks[taskidx].wn]; //array to store target block state
    int lpas[tasks[taskidx].wn];      //array to store lpa of write req
    int cur_offset;
    int bnum = 0;
    float exec_sum = 0.0, period = (float)tasks[taskidx].wp;
    
    for(int i=0;i<tasks[taskidx].wn;i++){
        lpas[i] = IOget(fp_w);
        if(lpas[i] == EOF){
            //if returned value is EOF
            //1. reset file & read first data.
            rewind(fp_w);
            fscanf(fp_w,"%ld,",&(lpas[i]));
            //2. add offset to expected update timing.
            add_offset_for_timing(metadata,taskidx,tasks[taskidx].addr_lb,tasks[taskidx].addr_ub,cur_cp);
            reset_IO_update(metadata,tasks[taskidx].addr_lb,tasks[taskidx].addr_ub,cur_cp);
        }
    }
    
    for(int i=0;i<tasks[taskidx].wn;i++){
        //make sure that no full block is in write block list during assign logic.
        //allocate current block to full B.
        if(cur != NULL){
            if(cur->fpnum == 0){
                temp = ll_remove(write_head,cur->idx);
                ll_append(full_head,temp);
                //printf("append %d, fullbnum : %d, wbnum : %d, freebnum: %d\n",temp->idx,full_head->blocknum,write_head->blocknum,fblist_head->blocknum);
                //printf("free page left : %d\n",metadata->total_fp);
                //printf("cur_cp : %ld\n",cur_cp);
                //print_maxinvalidation_block(metadata, temp->idx);
                cur = NULL;
            } else {
                //do nothing
            }
        }
        if(wflag == 0){
            cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else if(wflag == 11){
            cur = assign_write_dynwl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else if(wflag == 12 || wflag == 13){
            cur = assign_write_gradient(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,i,wflag);
        } else if(wflag == 14){
            cur = assign_write_maxinvalid(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target,lpas,i,cur_cp);
        }
        cur_offset = PPB - cur->fpnum;
        ppa_dest[i] = cur->idx*PPB + cur_offset;
        ppa_state[i] = metadata->state[cur->idx];
        cur->fpnum--;
        last_access_block = cur;
        //allocate current block to full B.
        if(cur != NULL){
            if(cur->fpnum == 0){
                temp = ll_remove(write_head,cur->idx);                
                ll_append(full_head,temp);
                //printf("append %d, fullbnum : %d, wbnum : %d, freebnum: %d\n",temp->idx,full_head->blocknum,write_head->blocknum,fblist_head->blocknum);
                //printf("free page left : %d\n",metadata->total_fp);
                //printf("cur_cp : %ld\n",cur_cp);
                //print_maxinvalidation_block(metadata, temp->idx);
                cur = NULL;
            } else {
                //do nothing
            }
        }
    }
    
    //assign page write destination
    //generate I/O requests.
    char name[30];
    FILE* timing_fp;
    for (int i=0;i<tasks[taskidx].wn;i++){
        IO* req = (IO*)malloc(sizeof(IO));
        lpa = lpas[i];
        metadata->write_cnt[lpa]++;
        metadata->write_cnt_task[taskidx]++;
        metadata->tot_write_cnt++;
        metadata->write_cnt_per_cycle[lpa]++;
        metadata->reserved_write++;
        metadata->avg_update[lpa] = (metadata->avg_update[lpa]*(metadata->write_cnt[lpa]-1)+(cur_cp - metadata->recent_update[lpa]))/(long)metadata->write_cnt[lpa];
        metadata->recent_update[lpa] = cur_cp;
        //printf("avg : %ld, recent : %ld\n",metadata->avg_update[lpa],metadata->recent_update[lpa]);
        req->type = WR;
        req->taskidx = taskidx;
        req->lpa = lpa;
        req->ppa = ppa_dest[i];
        req->IO_start_time = cur_cp;
        req->deadline = (long)cur_cp + (long)tasks[taskidx].wp;
        req->exec = (long)floor((double)w_exec(ppa_state[i]));
#ifdef IOTIMING
        IO_timing_update(metadata,lpa,metadata->write_cnt_per_cycle[lpa],cur_cp);
#endif
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
        if(tasks[taskidx].wn == 1){
            req->init = 1;
            req->last = 1;
        }
        //printf("tp:%d,lpa:%d,ppa:%d,dl:%ld,exec:%ld\n",req->type,req->lpa,req->ppa,req->deadline,req->exec);
    }
        
    //update runtime utilization
    metadata->runutils[0][taskidx] = exec_sum / period;
    //sleep(1);
    return last_access_block;
}

void read_job_start_q(rttask* task, int taskidx, meta* metadata, FILE* fp_r, IOhead* rq, long cur_cp){
    int lpa;
    int target_block[task[taskidx].rn];
    float exec_sum = 0.0, period = (float)task[taskidx].rp;
    for(int i=0;i<task[taskidx].rn;i++){
        lpa = IOget(fp_r);
        if(lpa == EOF){
            //if returned value is EOF
            //1. reset file & read first data.
            rewind(fp_r);
            fscanf(fp_r,"%ld,",&lpa);
        }
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
        if(task[taskidx].rn == 1){
            req->init = 1;
            req->last = 1;
        }
        metadata->read_cnt[lpa]++;
        metadata->read_cnt_task[taskidx]++;
        metadata->tot_read_cnt++;
    }
    metadata->runutils[1][taskidx] = exec_sum / period;
}

void gc_job_start_q(rttask* tasks, int taskidx, int tasknum, meta* metadata, 
                  bhead* fblist_head, bhead* full_head, bhead* rsvlist_head, bhead* write_head,
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
        gc_limit = find_gc_utilsort(tasks,taskidx,tasknum,metadata,full_head,rsvlist_head,write_head);
    }
    print_blocklist_info(write_head,metadata);
    print_blocklist_info(full_head,metadata);
    //if gc baseline, select most invalid block
    if (gcflag == 0){
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit)){
                if(metadata->invnum[cur_vic_idx] >= PPB/4*3){
                    cur_vic_idx = cur->idx;
                    vic = cur;
                }
            }
            cur = cur->next;
        }
        if(vic==NULL){
            //printf("!!!no feasible GC. fall back to original!!!\n");
            cur = full_head->head;
            while(cur != NULL){
                if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
                (metadata->state[cur->idx] >= write_limit)){
                    cur_vic_idx = cur->idx;
                    vic = cur;
                }
                cur = cur->next;
            }
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

    
#ifdef GC_ON_WRITEBLOCK
    make_req_gc(metadata,tasks,taskidx,cur_cp,vic,rsv,gcq,full_head,write_head,fblist_head,cur_GC);
#endif
#ifndef GC_ON_WRITEBLOCK
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
    metadata->runutils[2][taskidx] = gc_exec / gc_period;    
#endif
    //printf("fbnum : %d, wbnum : %d, fpnum : %d, invnum : %d\n",fblist_head->blocknum,write_head->blocknum,metadata->total_fp,metadata->total_invalid);
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
        //printf("vic1 %d, vic2 %d, skiprr\n",vic1,vic2);
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