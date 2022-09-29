#include "locality.h"

void calc_locality(meta* metadata, int target, double OP, double* r_tp_loc, double* w_tp_loc){
    int max_valid_pg = (int)((1.0-OP)*(float)(PPB*NOB));
    int wsum = 0, rsum = 0;
    char w_hotness = 0, r_hotness = 0;
    double wavg = 0.0, ravg = 0.0;
    int whot_count = 0, rhot_count = 0;
    double wloc = 0.0, rloc = 0.0;

    for(int i=0;i<max_valid_pg;i++){
        wsum += metadata->write_cnt[i];
        rsum += metadata->read_cnt[i];
    }
    
    //calculate average access count 
    wavg = (double)wsum / (double)max_valid_pg;
    ravg = (double)rsum / (double)max_valid_pg;
    for(int i=0;i<max_valid_pg;i++){
        if((double)metadata->write_cnt[i] > wavg){
            whot_count += metadata->write_cnt[i];
        }
        if((double)metadata->read_cnt[i] > ravg){
            rhot_count += metadata->read_cnt[i];
        }
    }
    wloc = whot_count / wsum;
    rloc = rhot_count / rsum;

    //divide the page into write-cold, write-hot, read-cold, read-hot page.
    
    if(metadata->write_cnt[target] > wavg){
        *w_tp_loc = wloc;
    } else {
        *w_tp_loc = 1.0 - wloc;
    }

    if(metadata->read_cnt[target] > ravg){
        *r_tp_loc = rloc;
    } else {
        *r_tp_loc = 1.0 - rloc;
    }
}