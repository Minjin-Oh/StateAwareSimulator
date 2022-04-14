#include "stateaware.h"

block* write_simul(rttask task, meta* metadata, int* g_cur, 
                   bhead* fblist_head, bhead* write_head, bhead* full_head, 
                   block* cur_fb, int* total_fp, float* tracker, FILE* fp_w, int write_limit){
    /* FIXME: write_simul must give out current target block (for next write)*/
    //printf("[WSIM]Writing target is %d, fp %d, tot fp %d\n",cur_fb->idx,cur_fb->fpnum,*total_fp);
    float hot = -1.0;
    int lpa = -1;
    block* cur = cur_fb;
    int* target_block;
    int logispace = (int)(NOP*(1-OP));
    int hotspace = (int)(logispace*SPLOCAL);
    target_block = (int*)malloc(sizeof(int)*task.wn);
    float run_exec = 0.0;
    for (int i=0;i<task.wn;i++){
        //locality based lpa generation
        hot = (float)(rand()%100)/100.0;
        if (hot < TPLOCAL){
            lpa = rand()%hotspace;
        }
        else {
            lpa = rand()%(logispace-hotspace)+hotspace;
        }
        //OVERRIDE:: get lpa with given workload file.
        lpa = IOget(fp_w);
        //invalidate old
        int old_ppa = metadata->pagemap[lpa];
        int old_block = (int)(old_ppa/PPB);
        metadata->invnum[old_block]++;
        metadata->invmap[old_ppa] = 1;
        metadata->total_invalid++;
        //get new free block if necessary
        if(cur->fpnum <= 0){
            if(write_head->head == NULL){
                printf("oh, why is this null?\n");
                sleep(1);
                abort();
            }
            //append the current block in full block list
            ll_remove(write_head,cur->idx);
            ll_append(full_head,cur);

            //get a new block         
            if(write_limit == -1){ //write control not necessary
                cur = ll_condremove(metadata,fblist_head,YOUNG);
            } else if(write_limit == -2){ //write limit cannot make taskset feasible
                printf("write limit cannot be enforced\n");
                sleep(3);
                abort();
            } else{ //get a new block w.r.t write_limit
                cur = ll_condremove(metadata,fblist_head,write_limit);
                printf("write limit enforced, condition : %d\n",metadata->state[cur->idx]);
            }
            //if there's no feasible free block, abort.
            if(cur==NULL){
                printf("no free block exist. cur fp : %d\n",*total_fp);
                sleep(3);
                abort();
            }
            ll_append(write_head,cur);
            *g_cur = PPB*cur->idx + (PPB-cur->fpnum);
            
        }

        //validate new page
        target_block[i] = cur->idx;
        metadata->pagemap[lpa] = *g_cur;
        metadata->rmap[*g_cur] = lpa;
        if(metadata->access_tracker[task.idx][cur->idx]==0)
            metadata->access_tracker[task.idx][cur->idx] = 1;

        //move physical pg pointer & update metadata
        *g_cur += 1;
        cur->fpnum -= 1;
        *total_fp -= 1;
    }

    //profiling
    for (int i=0;i<task.wn;i++){
        run_exec += w_exec(metadata->state[target_block[i]]);
    }
    *tracker = run_exec/(float)task.wp;
    free(target_block);
    return cur;
}

void read_simul(rttask task, meta* metadata, float* tracker, int offset, FILE* fp_r){
    float hot = -1.0;
    int lpa = -1;
    int target_block[task.rn];
    int logispace = (int)(NOP*(1-OP));
    int hotspace = (int)(logispace*SPLOCAL);
    float run_exec = 0.0;
    for (int i=0;i<task.rn;i++){
        //generate target lpa based on locality.
        hot = (float)(rand()%100)/100.0;
        if (hot < TPLOCAL){
            lpa = rand()%hotspace+offset;//offset can be given
        }
        else {
            lpa = rand()%(logispace);
            while(lpa <= hotspace+offset && lpa >= offset){
                lpa = rand()%(logispace);
            }
        }
        //OVERRIDE:: get lpa with given workload file.
        lpa = IOget(fp_r);
        //printf("[READ]lpa get %d\n",lpa);
        //record the target block to access history
        target_block[i] = metadata->pagemap[lpa]/PPB;
        //if(target_block[i] == 99){
        //printf("accessed lpa : %d, accessed ppa : %d, accessed block : %d\n",
        //    lpa,metadata->pagemap[lpa],metadata->pagemap[lpa]/PPB);
        //}
        metadata->access_window[target_block[i]]++;
    }
    //profiling 
    for (int i=0;i<task.rn;i++){
        run_exec += r_exec(metadata->state[target_block[i]]);
    }
    *tracker = run_exec/(float)task.rp;
}

void gc_simul(rttask task, int tasknum, meta* metadata, bhead* fblist_head, 
             bhead* full_head, bhead* rsvlist_head, int* total_fp, float* tracker, 
             int gc_limit, int write_limit, int* targetblockhistory){
    //params
    block* cur = full_head->head;
    block* vic = NULL;
    block* rsv;
    int cur_vic_idx = cur->idx;
    int cur_vic_invalid = -1;
    int vic_offset;
    int rsv_offset;
    int vp_count;
    float run_exec = 0.0;
    float run_period = 0.0;
    int vic_state[PPB];
    int tar_state[PPB];

    //find victim & rsv
    int old = get_blockstate_meta(metadata,OLD);
    int yng = get_blockstate_meta(metadata,YOUNG);

#ifdef FORCEDNOTHRES
    int tolerable;
    float gc_worst, gc_exec, gc_period, gc_curutil;
    while(cur!=NULL){
        tolerable = 0;
        gc_worst = __calc_gcu(&task,MINRC,yng,old,old);
        gc_exec = (PPB-(metadata->invnum[cur->idx]))*(w_exec(yng)+r_exec(metadata->state[cur->idx])) + e_exec(metadata->state[cur->idx]);
        gc_period = _gc_period(&task,metadata->invnum[cur->idx]);
        gc_curutil = gc_exec/gc_period;
        if(gc_worst >= gc_curutil){
            tolerable = 1;
        }
        if(cur_vic_invalid == -1){//initialize block target
            if((tolerable == 1) &&
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
               (metadata->state[cur->idx] >= write_limit) &&
               (tolerable == 1)){
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
#ifdef NORMAL
    while(cur != NULL){
        if((metadata->invnum[cur_vic_idx] <= metadata->invnum[cur->idx]) &&
           (metadata->state[cur->idx] >= write_limit)){
            cur_vic_idx = cur->idx;
            vic = cur;
        }
        cur = cur->next;
    }
#endif
//if GC gets controlled by external func, index of target block is given

#if defined(DOGCCONTROL) || defined(DOGCNOTHRES)
    while(cur != NULL){
        if(cur->idx == gc_limit){
            vic = cur;
            break;
        }
        cur = cur->next;
    }
#endif

    printf("[GC]expected target : %d",vic->idx);
    *targetblockhistory = metadata->state[vic->idx];
    if(vic == NULL){
        printf("[GC]no feasible block %d\n");
        abort();
    }
    if(metadata->invnum[cur_vic_idx]==0){
        printf("[GC]no fp block\n");
        return;
    }
    //rmv victim from full
    ll_remove(full_head,vic->idx); 
    rsv = ll_pop(rsvlist_head);

    //relocation
    vic_offset = PPB*(vic->idx);
    rsv_offset = PPB*(rsv->idx);
    vp_count = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[vic_offset+i] == 0){ //if valid data, move to rsv
            int rtv_lpa = metadata->rmap[vic_offset+i];
            metadata->pagemap[rtv_lpa] = rsv_offset+vp_count;
            metadata->rmap[rsv_offset+vp_count] = rtv_lpa;
            metadata->invmap[rsv_offset+vp_count]=0;
            //update runtime utilization(copyback overhead)
            vic_state[vp_count] = vic->idx;
            tar_state[vp_count] = rsv->idx;
            run_exec += r_exec(metadata->state[vic->idx])+w_exec(metadata->state[rsv->idx]);
            vp_count++;
        }
        //reset old page info
        metadata->rmap[vic_offset+i]= -1;
        metadata->invmap[vic_offset+i]= 0;
    }
    //set free page info in rsv block
    for(int i=vp_count;i<PPB;i++){
        metadata->rmap[rsv_offset+i] = -1;
        metadata->invmap[rsv_offset+i] = 0;
    }
    //finalize runtime execution
    run_exec += e_exec(metadata->state[vic->idx]);

    //update victim block metadata
    metadata->total_invalid -= metadata->invnum[vic->idx];
    metadata->invnum[vic->idx] = 0;
    metadata->state[vic->idx]++;
    //transfer access_tracking information & reset victim access info
    for(int i=0;i<tasknum;i++){
        metadata->access_tracker[i][rsv->idx] = metadata->access_tracker[i][vic->idx];
        metadata->access_tracker[i][vic->idx] = 0;
    }
    
#ifdef GCDEBUG
    printf("====GC unit function test====\n");
    for(int i=0;i<PPB;i++){
        printf("[GC][vic]pg %d: rmap %d, inv %d\n",vic_offset+i,metadata->rmap[vic_offset+i],metadata->invmap[vic_offset+i]);
    }
    for(int i=0;i<PPB;i++){
        printf("[GC][rsv]pg %d: rmap %d, inv %d\n",rsv_offset+i,metadata->rmap[rsv_offset+i],metadata->invmap[rsv_offset+i]);
    }
    printf("[GC][meta]vic %d invnum %d state %d rsv %d invnum %d state %d\n",
    vic->idx,metadata->invnum[vic->idx],metadata->state[vic->idx],
    rsv->idx,metadata->invnum[rsv->idx],metadata->state[rsv->idx]);

    printf("[GC]moved vnum = %d, vic = %d, rsv = %d\n",vp_count,vic->idx,rsv->idx);
    printf("[GC]states: ");
    for(int i=0;i<NOB;i++) printf("%d ",metadata->state[i]);
    printf("\n");
    printf("[GC]block tracking: %d,%d,%d\n",fblist_head->blocknum,rsvlist_head->blocknum,full_head->blocknum);
    printf("====GC unit function ends====\n");
#endif
    //insert each block to corresponding blocklist
    rsv->fpnum = PPB - vp_count;
    vic->fpnum = PPB;
    *total_fp += rsv->fpnum;
    ll_append(fblist_head,rsv);
    ll_append(rsvlist_head,vic);
    *tracker = run_exec / (float)task.gcp;
}

void wl_simul(meta* metadata, int tasknum, bhead* fblist_head, bhead* full_head,
              bhead* hotlist, bhead* coldlist, 
              int vic1, int vic2, int* total_fp){
    printf("-------------------------------------\n");
    printf("list length fu : %d, fb : %d\n",full_head->blocknum,fblist_head->blocknum);
    
    //find vic1 and vic2
    block* vic_1=NULL;
    block* vic_2=NULL;
    block* cur = full_head->head;
    //checking fblist and full list
    printf("[FULL]");
    while(cur != NULL){
        printf(" %d",cur->idx);
        if(cur->idx == vic1){
            printf("(vic1.");
            vic_1 = cur;
            printf("test:%d)",vic_1->idx);
        }
        if(cur->idx == vic2){
            printf("(vic2.");
            vic_2 = cur;
            printf("test:%d)",vic_2->idx);
        }    
        cur = cur->next;
    }
    printf("\n");
    printf("[FREE]");
    cur = fblist_head->head;
    while(cur != NULL){
        printf(" %d",cur->idx);
        if(cur->idx == vic1){
            printf("(vic1.");
            vic_1 = cur;
            printf("test:%d)",vic_1->idx);
        }
        if(cur->idx == vic2){
            printf("(vic2.");
            vic_2 = cur;
            printf("test:%d)",vic_2->idx);
        }   
        cur = cur->next;
    }
    printf("\n");
    printf("want to swap %d and %d\n",vic1,vic2);
    printf("victim blocks %d %d\n",vic_1->idx,vic_2->idx);
    //found.
    
    //params
    int vic1_offset = PPB*vic1;
    int vic2_offset = PPB*vic2;
    int vp1_count = 0;
    int vp2_count = 0;
    int prev_fp_vic1, prev_fp_vic2;

    //for metadata update, save previous freepage number.
    prev_fp_vic1 = vic_1->fpnum;
    prev_fp_vic2 = vic_2->fpnum;
    printf("[WL]victim fp : %d, %d\n",prev_fp_vic1,prev_fp_vic2);
    
    //load metadata to temp
    meta temp;
    memcpy(&temp,metadata,sizeof(meta)); //copy current metadata status
    
    //data relocation
    for(int i=0;i<PPB;i++){
        if((temp.invmap[vic1_offset+i] == 0) && 
           (temp.rmap[vic1_offset+i] != -1)){ //mov ONLY valid data
            int rtv_lpa = temp.rmap[vic1_offset+i]; //retrieve lpa from reversemap,
            //update metadata
            metadata->pagemap[rtv_lpa] = vic2_offset+vp1_count;
            metadata->rmap[vic2_offset+vp1_count] = rtv_lpa;
            metadata->invmap[vic2_offset+vp1_count]=0;
            //printf("[WL1]map %d, mov %d to %d\n",rtv_lpa,vic1_offset+i,vic2_offset+vp1_count);
            vp1_count++;
        }
    }
    for(int i=0;i<PPB;i++){
        if((temp.invmap[vic2_offset+i] == 0) && 
           (temp.rmap[vic2_offset+i] != -1)){
            int rtv_lpa = temp.rmap[vic2_offset+i]; //retrieve lpa from rmap
            //update metadata
            metadata->pagemap[rtv_lpa] = vic1_offset+vp2_count;
            metadata->rmap[vic1_offset+vp2_count] = rtv_lpa;
            metadata->invmap[vic1_offset+vp2_count] = 0;
            //printf("[WL2]map %d, mov %d to %d\n",rtv_lpa,vic2_offset+i,vic1_offset+vp2_count);
            vp2_count++;
        }
    }
    printf("checking. %d vs %d, %d vs %d\n",temp.invnum[vic1],vp1_count,temp.invnum[vic2],vp2_count);
    //reset rest of the pages.
    for(int i=vp2_count;i<PPB;i++){
        metadata->invmap[vic1_offset+i]=0;
        metadata->rmap[vic1_offset+i]=-1; 
    }
    for(int i=vp1_count;i<PPB;i++){
        metadata->invmap[vic2_offset+i]=0;
        metadata->rmap[vic2_offset+i]=-1;
    }

    //update block info(metadata)
    metadata->total_invalid -= metadata->invnum[vic_1->idx];
    metadata->total_invalid -= metadata->invnum[vic_2->idx];
    metadata->invnum[vic_1->idx] = 0;
    metadata->invnum[vic_2->idx] = 0;
    metadata->state[vic_1->idx]++;
    metadata->state[vic_2->idx]++;
    //update access tracking info (swap access info)
    for(int i=0;i<tasknum;i++){
        int temp;
        temp = metadata->access_tracker[i][vic_1->idx];
        metadata->access_tracker[i][vic_1->idx] = metadata->access_tracker[i][vic_2->idx];
        metadata->access_tracker[i][vic_2->idx] = temp;
    }
    //update block info(blocks)
    //!!!!make sure we also edit total_fp!!!!
    vic_1->fpnum = PPB-vp2_count;
    vic_2->fpnum = PPB-vp1_count;
    printf("[WL]new fp : %d, %d\n",vic_1->fpnum,vic_2->fpnum);
    *total_fp += (vic_1->fpnum - prev_fp_vic1) + (vic_2->fpnum - prev_fp_vic2);
    //update freeblock list
    //block currently exist exist either in fullblock list or freeblock list.
    block *btemp;
    block *btemp2;
    printf("[WL]list check, fu : %d, fb : %d\n",full_head->blocknum,fblist_head->blocknum);
    printf("[WL]finished moving %d and %d\n",vic_1->idx,vic_2->idx);
    

    //retrieve the blocks from original places.
    if(is_idx_in_list(full_head,vic1)){
        btemp = ll_remove(full_head,vic1);
    }
    else if(is_idx_in_list(fblist_head,vic1)){
        btemp = ll_remove(fblist_head,vic1);
    }
    if(is_idx_in_list(full_head,vic2)){
        btemp2 = ll_remove(full_head,vic2);
    }
    else if(is_idx_in_list(fblist_head,vic2)){
        btemp2 = ll_remove(fblist_head,vic2);    
    }

    //reallocate them + update free page number
    if(btemp->fpnum != 0){      
        ll_append(fblist_head,btemp);
    }
    else if(btemp->fpnum == 0){
        ll_append(full_head,btemp);
    }
    if(btemp2->fpnum != 0){
        ll_append(fblist_head,btemp2);
    }
    else if(btemp2->fpnum == 0){
        ll_append(full_head,btemp2);
    }

    //update hotcold blocklist (DISABLED NOW)
    /*
    btemp = ll_remove(hotlist,vic1);
    btemp2 = ll_remove(coldlist,vic2);
    if((btemp == NULL) || (btemp2 == NULL)) 
        printf("block not found. check hot-cold list!\n");
    ll_append(coldlist,btemp);
    ll_append(hotlist,btemp2);
    
    
    //print codes for WL unit-test
    printf("metadata checking. {new(old)}\n");
    printf("[invnum] ");
    for(int i=0;i<NOB;i++)
        printf("%d(%d) ",metadata->invnum[i],temp.invnum[i]);
    printf("\n[state] ");
    for(int i=0;i<NOB;i++)
        printf("%d(%d) ",metadata->state[i],temp.state[i]);
    printf("\n");
    

    printf("hotcold checking. state(idx). one element should be swapped.\n");
    cur = hotlist->head;
    while(cur!=NULL){
        printf("%d(%d) ",metadata->state[cur->idx],cur->idx);
        cur = cur->next;
    }
    printf(" vs ");
    cur = coldlist->head;
    while(cur!=NULL){
        printf("%d(%d) ",metadata->state[cur->idx],cur->idx);
        cur = cur->next;
    }
    printf("\n");
    */
}
