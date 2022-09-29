#include "rrsim_q.h"

//make read request
long gen_read_rr(int vic1, int vic2, int cur_cp, int rrp, meta* metadata, IOhead* rrq){
    long tot_exec = 0;
    int v1_offset = PPB*vic1;
    int v2_offset = PPB*vic2;
    int lpa, v1_cnt = 0, v2_cnt = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[v1_offset+i]==0){
            IO* req = (IO*)malloc(sizeof(IO));
            lpa=metadata->rmap[v1_offset+i];
            req->type = RRRE;
            req->deadline = (long)cur_cp + (long)(rrp);
            req->exec = (long)floor((double)r_exec(metadata->state[vic1]));
            ll_append_IO(rrq,req);
            v1_cnt++;
            tot_exec += req->exec;
        }
    }
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[v2_offset+i]==0){
            IO* req2 = (IO*)malloc(sizeof(IO));
            lpa=metadata->rmap[v2_offset+i];
            req2->type = RRRE;
            req2->deadline = (long)cur_cp + (long)(rrp);
            req2->exec = (long)floor((double)r_exec(metadata->state[vic2]));
            ll_append_IO(rrq,req2);
            v2_cnt++;
            tot_exec += req2->exec;
        }
    }
    return tot_exec;
}

//make write request
long gen_write_rr(int vic1, int vic2, int cur_cp, int rrp, meta* metadata, IOhead* rrq){
    long tot_exec = 0;
    int v1_offset = PPB*vic1;
    int v2_offset = PPB*vic2;
    int lastgen1=0, lastgen2=0;
    int lpa, v1_cnt = 0, v2_cnt = 0;
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[v1_offset+i]==0){
            IO* req = (IO*)malloc(sizeof(IO));
            lpa=metadata->rmap[v1_offset+i];
            req->type = RRWR;
            req->rr_valid_count = PPB - metadata->invnum[vic1];
            req->vic_idx = vic1;
            req->tar_idx = vic2;
            req->rr_old_lpa = lpa;
            req->rr_vic_ppa = v1_offset+i;
            req->rr_tar_ppa = v2_offset+v1_cnt;
            req->vmap_task = metadata->vmap_task[v1_offset+i];
            req->deadline = (long)cur_cp + (long)(rrp);
            req->exec = (long)floor((double)w_exec(metadata->state[vic1]));
            ll_append_IO(rrq,req);
            v1_cnt++;
            if(v1_cnt == req->rr_valid_count){
                req->islastreq = 1;
                lastgen1=1;
            } 
            else {
                req->islastreq = 0;
            }
            tot_exec += req->exec;
            //printf("[RRWR-unit]v1cnt:%d, expected:%d, lpa : %d, mov %d to %d\n",
            //        v1_cnt,req->rr_valid_count,req->rr_old_lpa,req->rr_vic_ppa,req->rr_tar_ppa);
        }
        
    }
    for(int i=0;i<PPB;i++){
        if(metadata->invmap[v2_offset+i]==0){
            IO* req2 = (IO*)malloc(sizeof(IO));
            lpa=metadata->rmap[v2_offset+i];
            req2->type = RRWR;
            req2->rr_valid_count = PPB - metadata->invnum[vic2];
            req2->vic_idx = vic2;
            req2->tar_idx = vic1;
            req2->rr_old_lpa = lpa;
            req2->rr_vic_ppa = v2_offset+i;
            req2->rr_tar_ppa = v1_offset+v2_cnt;
            req2->vmap_task = metadata->vmap_task[v2_offset+i];
            req2->deadline = (long)cur_cp + (long)(rrp);
            req2->exec = (long)floor((double)w_exec(metadata->state[vic2]));
            ll_append_IO(rrq,req2);
            v2_cnt++;
            if(v2_cnt == req2->rr_valid_count){
                req2->islastreq = 1;
                lastgen2=1;
            }
            else {
                req2->islastreq = 0;
            }
            tot_exec += req2->exec;
            //printf("[RRWR-unit]v2cnt:%d, expected:%d, lpa : %d, mov %d to %d\n",
            //       v2_cnt,req2->rr_valid_count,req2->rr_old_lpa,req2->rr_vic_ppa,req2->rr_tar_ppa);
        }
    }
    //printf("[RRWR-summ]%d with %d\n",vic1,vic2);
    //printf("[RRWR-summ]%d, %d vs %d, %d\n",v1_cnt,v2_cnt,PPB-metadata->invnum[vic1],PPB-metadata->invnum[vic2]);
    //printf("[RRWR-summ]invnum : %d, %d\n",metadata->invnum[vic1],metadata->invnum[vic2]);
    //printf("[RRWR-summ]lastreqflag :%d, %d\n",lastgen1,lastgen2);
    if((v1_cnt != PPB-metadata->invnum[vic1]) || (v2_cnt != PPB-metadata->invnum[vic2])){
        abort();
    }
    return tot_exec;
}

long gen_erase_rr(int vic1, int vic2, int cur_cp, int rrp, meta* metadata, IOhead* rrq){
    long tot_exec = 0;

    IO* er = (IO*)malloc(sizeof(IO));
    er->type = RRER;
    er->vic_idx = vic1;
    er->deadline = (long)cur_cp + (long)(rrp);
    er->exec = (long)floor((double)e_exec(metadata->state[vic1]));
    tot_exec += er->exec;
    ll_append_IO(rrq,er);
    
    IO* er2 = (IO*)malloc(sizeof(IO));
    er2->type = RRER;
    er2->vic_idx = vic2;
    er2->deadline = (long)cur_cp + (long)(rrp);
    er2->exec = (long)floor((double)e_exec(metadata->state[vic2]));
    tot_exec += er2->exec;
    ll_append_IO(rrq,er2);

    return tot_exec;
}
