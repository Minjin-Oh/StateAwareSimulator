#include "rrsim_q.h"

//make read request
long gen_read_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq){
    long tot_exec = 0;
    int v1_offset = PPB*vic1;
    int v2_offset = PPB*vic2;
    int lpa, v1_cnt = 0, v2_cnt = 0;
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v1_offset+i]==0){
            IO* req = (IO*)malloc(sizeof(IO));
            lpa=temp.rmap[v1_offset+i];
            req->type = RRRE;
            req->deadline = (long)cur_cp + (long)(rrp);
            req->exec = (long)floor((double)r_exec(temp.state[vic1]));
            ll_append_IO(rrq,req);
            v1_cnt++;
            tot_exec += req->exec;
        }
    }
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v2_offset+i]==0){
            IO* req2 = (IO*)malloc(sizeof(IO));
            lpa=temp.rmap[v2_offset+i];
            req2->type = RRRE;
            req2->deadline = (long)cur_cp + (long)(rrp);
            req2->exec = (long)floor((double)r_exec(temp.state[vic2]));
            ll_append_IO(rrq,req2);
            v2_cnt++;
            tot_exec += req2->exec;
        }
    }
    return tot_exec;
}

//make write request
long gen_write_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq){
    long tot_exec = 0;
    int v1_offset = PPB*vic1;
    int v2_offset = PPB*vic2;
    int lpa, v1_cnt = 0, v2_cnt = 0;
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v1_offset+i]==0){
            IO* req = (IO*)malloc(sizeof(IO));
            lpa=temp.rmap[v1_offset+i];
            req->type = RRWR;
            req->rr_valid_count = PPB - temp.invnum[vic1];
            req->vic_idx = vic1;
            req->tar_idx = vic2;
            req->rr_old_lpa = lpa;
            req->rr_vic_ppa = v1_offset+i;
            req->rr_tar_ppa = v2_offset+v1_cnt;
            req->vmap_task = temp.vmap_task[v1_offset+i];
            req->deadline = (long)cur_cp + (long)(rrp);
            req->exec = (long)floor((double)w_exec(temp.state[vic1]));
            ll_append_IO(rrq,req);
            v1_cnt++;
            if(v1_cnt == req->rr_valid_count){
                req->islastreq = 1;
            } 
            else {
                req->islastreq = 0;
            }
            tot_exec += req->exec;
        }
    }
    for(int i=0;i<PPB;i++){
        if(temp.invmap[v2_offset+i]==0){
            IO* req2 = (IO*)malloc(sizeof(IO));
            lpa=temp.rmap[v2_offset+i];
            req2->type = RRWR;
            req2->rr_valid_count = PPB - temp.invnum[vic2];
            req2->vic_idx = vic2;
            req2->tar_idx = vic1;
            req2->rr_old_lpa = lpa;
            req2->rr_vic_ppa = v2_offset+i;
            req2->rr_tar_ppa = v1_offset+v2_cnt;
            req2->vmap_task = temp.vmap_task[v2_offset+i];
            req2->deadline = (long)cur_cp + (long)(rrp);
            req2->exec = (long)floor((double)w_exec(temp.state[vic2]));
            ll_append_IO(rrq,req2);
            v2_cnt++;
            if(v2_cnt == req2->rr_valid_count){
                req2->islastreq = 1;
            }
            else {
                req2->islastreq = 0;
            }
            tot_exec += req2->exec;
        }
    }
    return tot_exec;
}

long gen_erase_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq){
    long tot_exec = 0;

    IO* er = (IO*)malloc(sizeof(IO));
    er->type = RRER;
    er->vic_idx = vic1;
    er->deadline = (long)cur_cp + (long)(rrp);
    er->exec = (long)floor((double)e_exec(temp.state[vic1]));
    tot_exec += er->exec;
    ll_append_IO(rrq,er);
    
    IO* er2 = (IO*)malloc(sizeof(IO));
    er2->type = RRER;
    er->vic_idx = vic2;
    er->deadline = (long)cur_cp + (long)(rrp);
    er->exec = (long)floor((double)e_exec(temp.state[vic2]));
    ll_append_IO(rrq,er2);
    tot_exec += er2->exec;

    return tot_exec;
}
