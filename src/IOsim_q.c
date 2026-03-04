#include "stateaware.h"
#include "findGC.h"
#include "findRR.h"
#include "assignW.h"
#include "rrsim_q.h"
#include "IOgen.h"
#include "emul_logger.h"

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
                     FILE* fp_w, IOhead* wq, block* cur_target, int wflag, long cur_cp, 
                     FILE* fpovhd_w_release, FILE* fpovhd_w_assign, FILE* w_assign_detail){

    //makes write job according to workload and task parameter
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

    struct timeval a;
    struct timeval b;
    long get_lpa, alloc_blk, gen_IO;
    
    gettimeofday(&a,NULL);
    for(int i=0;i<tasks[taskidx].wn;i++){
        lpas[i] = IOget(fp_w); // fp_w : wr_t%d.csv
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
    gettimeofday(&b,NULL);
    get_lpa = (b.tv_sec - a.tv_sec)*1000000 + (b.tv_usec - a.tv_usec);
    
    gettimeofday(&a,NULL);
    for(int i=0;i<tasks[taskidx].wn;i++){
        //make sure that no full block is in write block list during assign logic.
        //allocate current block to full B.
        // if(cur != NULL){
        //     if(cur->fpnum == 0){
        //         temp = ll_remove(write_head,cur->idx);
        //         ll_append(full_head,temp);
        //         cur = NULL;
        //     }
        // }
        if(wflag == 0){ // argv[2] == NO
            cur = assign_write_FIFO(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else if(wflag == 11){ // argv[2] == MOTIVALLY
            cur = assign_write_dynwl(tasks,taskidx,tasknum,metadata,fblist_head,write_head,cur_target);
        } else if(wflag == 14){ // argv[2] == INVW
            cur = assign_write_maxinvalid(tasks,taskidx,tasknum,metadata,fblist_head,write_head,lpas,i,cur_cp, fpovhd_w_assign, w_assign_detail);
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
                cur = NULL;
            }
        }
    }
    gettimeofday(&b,NULL);
    alloc_blk = (b.tv_sec - a.tv_sec)*1000000 + (b.tv_usec - a.tv_usec);
    
    //assign page write destination
    //generate I/O requests.
    char name[30];
    FILE* timing_fp;

    gettimeofday(&a, NULL);
    for (int i=0;i<tasks[taskidx].wn;i++){
        IO* req = (IO*)malloc(sizeof(IO));
        lpa = lpas[i];
        metadata->write_cnt[lpa]++;
        metadata->write_cnt_task[taskidx]++;
        metadata->tot_write_cnt++;
        metadata->write_cnt_per_cycle[lpa]++;
        metadata->reserved_write++;
        metadata->avg_update[lpa] = (metadata->avg_update[lpa]*(long)(metadata->write_cnt[lpa]-1)+(cur_cp - metadata->recent_update[lpa]))/(long)metadata->write_cnt[lpa];
        //printf("avg : %ld, recent : %ld\n",metadata->avg_update[lpa],metadata->recent_update[lpa]);
        metadata->recent_update[lpa] = cur_cp;
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
        if(i != tasks[taskidx].wn-1){ // last request flag check
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
    }
    gettimeofday(&b,NULL);
    gen_IO = (b.tv_sec - a.tv_sec)*1000000 + (b.tv_usec - a.tv_usec);

    //update runtime utilization
    metadata->runutils[0][taskidx] = exec_sum / period;
    fprintf(fpovhd_w_release, "%ld, %ld, %ld, \n", get_lpa, alloc_blk, gen_IO);
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
                  int write_limit, IOhead* gcq, GCblock* cur_GC, int gcflag, long cur_cp, FILE* gc_detail){
    if(gcq->reqnum != 0){
        //printf("[%ld]queue not empty, dl miss detected. task %d GC\n",cur_cp,taskidx);
        //sleep(3);
        //abort();
    }
    
    // 0. parameter initialization
    block* cur = full_head->head;
    block* vic = NULL;
    block* rsv;
    block* oldest_vic = NULL;
    block* others_vic = NULL;
    int cur_vic_idx = cur->idx;
    int cur_vic_invalid = -1;
    int vic_offset;
    int rsv_offset;
    int gc_limit;
    int vp_count, rtv_lpa;
    int old = get_blockstate_meta(metadata,OLD);
    int yng = get_blockstate_meta(metadata,YOUNG);
    float gc_exec = 0.0, gc_period = (float)tasks[taskidx].gcp;
    int oldest_vic_invalid = -1;
    int others_vic_invalid = -1;
    int oldest_vic_idx;
    int others_vic_idx;
   
    // 1. victim block selection
    // 1-(1). if GC is greedy, select most invalid block
    if (gcflag == 0){
        while(cur != NULL){
            if((metadata->invnum[cur_vic_idx] < metadata->invnum[cur->idx]) &&
               (metadata->state[cur->idx] >= write_limit)){
                cur_vic_idx = cur->idx;
                vic = cur;
            }
            cur = cur->next;
        }
        if(vic==NULL){
            vic = full_head->head;
        }
    }
    
    // 1-(2). if not (UTILGC = 6)
    else if (gcflag == 6){
        // 1-(2)-1. find gc victim block index
        gc_limit = find_gc_utilsort(tasks,taskidx,tasknum,metadata,full_head,rsvlist_head,write_head, gc_detail);

        // 1-(2)-2. choose a victim block from full_block list using index
        while(cur != NULL){
            if(cur->idx == gc_limit){
                cur_vic_idx = cur->idx;
                vic = cur;
                break;
            }
            cur = cur->next;
        }   
    }

    // 2. Edge Case
    // 2-(1). victim block이 없는 경우
    if(vic==NULL){
        printf("[GC]no feasible block\n");
        abort();
    }
    // 2-(2). victim block에 invalid page가 존재하지 않는 경우. 즉, 회수할 수 있는 free page가 0인 경우.
    if(metadata->invnum[vic->idx]==0){
        printf("no fp block\n");
        return;
    }

    
#ifdef GC_ON_WRITEBLOCK
    make_req_gc(metadata,tasks,taskidx,cur_cp,vic,rsv,gcq,full_head,write_head,fblist_head,cur_GC);
#endif

#ifndef GC_ON_WRITEBLOCK
    // 3. victim block을 full block list에서 pop, copy block을 reserved block list에서 pop.
    ll_remove(full_head,vic->idx);
    rsv = ll_pop(rsvlist_head);
    
    // 3-(1). victim block과 copy block을 찾을 수 없는 경우, Error!
    if(rsv == NULL || vic == NULL){
        printf("[%ld]gc fucked up\n");
        abort();
    }

    // 4. valid page copy 과정을 gcq에 enqueue
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

    // 5. victim block을 erase하는 작업을 gcq에 enqueue
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

}

void RR_job_start_q(rttask* tasks, int tasknum, meta* metadata, bhead* fblist_head, bhead* full_head, bhead* hotlist, bhead* coldlist,
                  IOhead* rrq, RRblock* cur_RR, double rrutil, long cur_cp, int skewnum){
    char reloc_w = 0;
    char reloc_r = 0;
    int vic1 = -1;
    int vic2 = -1;
    int v1_state, v2_state;
    int v1_offset, v2_offset, v1_cnt=0, v2_cnt=0;
    int lpa;
    long rrp;
    long execution_time = 0L;
    block *vb1 = NULL, *vb2 = NULL, *cur;
    meta temp;
    
    // 1. find relocation victim blocks
    // 1-(1). Static WL
    if(rrflag == 0){
        find_RR_dualpool(tasks, tasknum, metadata, full_head, hotlist, coldlist, &vic1, &vic2);
    }
    // 1-(2). LaWL
    else if (rrflag == 1){ 
        if (skewnum >= 2){ // write-intensive
            // write-based relocation 
            find_WR_target_simple(tasks, tasknum, metadata, fblist_head,full_head,&vic1,&vic2);
            if(vic1 == -1 || vic2 == -1){
                // read-based relocation
                find_RR_target_simple(tasks, tasknum, metadata, fblist_head,full_head,&vic1,&vic2);
                if(vic1 != -1 || vic2 != -1){
                    reloc_r = 1;
                }
            } 
            else{
                reloc_w = 1;
            }
        }
        else{ // read-intensive
            find_RR_target_simple(tasks, tasknum, metadata, fblist_head,full_head,&vic1,&vic2);
            if(vic1 == -1 || vic2 == -1){
                // read-based relocation
                find_WR_target_simple(tasks, tasknum, metadata, fblist_head,full_head,&vic1,&vic2);
                if(vic1 != -1 || vic2 != -1){
                    reloc_w = 1;
                }
            } 
            else{
                reloc_r = 1;
            }
        }       
    } 

    // 2. if victim block is not found, cancel WL and return.
    if(rrflag == 0 || rrflag == 1){
        if(vic1 == -1 || vic2 == -1){
            //printf("vic1 %d, vic2 %d, skiprr\n",vic1,vic2);
            return;
        }
    }

    // 3. if victim is found, reset access window (or inv window) for future.
    if(rrflag == 1){
        for(int i=0;i<NOB;i++){
            // 왜 access window (아마 read access 아닐까?)는 전체 block에 대해서 초기화 시키고,
            metadata->access_window[i] = 0;
        }
        // invalidation window는 victim block에 대해서만 초기화 하는거지?
        metadata->invalidation_window[vic1] = 0;
        metadata->invalidation_window[vic2] = 0;
    }
    
    // 4. find vb1 and vb2
    // 4-(1). full block list에서 확인
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
    // 4-(2). free block list에서 확인
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
    //4-(3). victim block의 offset, state, cnt 정보 저장
    v1_offset = PPB*vic1;
    v2_offset = PPB*vic2;
    v1_state = metadata->state[vb1->idx];
    v2_state = metadata->state[vb2->idx];
    v1_cnt = PPB - metadata->invnum[vic1];
    v2_cnt = PPB - metadata->invnum[vic2];

    // 5. find data relocation period (slack-based)
    rrp = find_RR_period(vic1,vic2,v1_cnt,v2_cnt,rrutil, metadata);

    // !!! edge case : if read relocation util is lower than 0.0, assign longest possible deadline.
    if(rrutil <= 0.0){
        rrp = __LONG_MAX__ - cur_cp;
    }

    //copy the metadata into temp param and use temp
    //make sure that metadata update do NOT generate concurrency issue.
    memcpy(&temp,metadata,sizeof(meta));

    // 6. generate relocation requests.
    execution_time += gen_read_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);
    //make erase request.
    execution_time += gen_erase_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);
    //make write request
    execution_time += gen_write_rr(vic1,vic2,cur_cp,rrp,metadata,rrq);

    // 7. remove target block from blocklist
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

    // 8. update block info & allocate pointer to tracker
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
