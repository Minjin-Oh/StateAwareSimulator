// dead_stubs.c
//
// Weak / no-op implementations for functions that are declared in the public
// headers and referenced by rarely-invoked (or fully dead) code paths but
// whose real implementations were never checked in. Purpose is purely to let
// the linker succeed for the actively-tested flag combinations
// (e.g. UTILGC INVW RR005 which never enters these paths at runtime).
//
// If a stub here starts being executed at runtime, that means a new mode is
// being tried and the corresponding real implementation must be written --
// the stubs abort loudly in that case so silent misbehavior can't slip past.

#include "stateaware.h"

// --- referenced by find_gcctrl / find_gc_test (dead for gcflag == 6) ---------
void update_read_worst(meta* metadata, int tasknum){
    (void)metadata; (void)tasknum;
    // find_gcctrl only. UTILGC uses find_gc_utilsort which does not call this.
}

float calc_readlatency(rttask* tasks, meta* metadata, int taskidx){
    (void)tasks; (void)metadata; (void)taskidx;
    return 0.0f;
}

float calib_readlatency(meta* metadata, int taskidx, float cur_exp_lat, int old_ppa, int new_ppa){
    (void)metadata; (void)taskidx; (void)old_ppa; (void)new_ppa;
    return cur_exp_lat; // identity: no calibration
}

// --- referenced by findW dead paths -----------------------------------------
float calc_weightedread(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas){
    (void)tasks; (void)metadata; (void)tar; (void)taskidx; (void)lpas;
    return 0.0f;
}

float calc_weightedgc(rttask* tasks, meta* metadata, block* tar, int taskidx, int* lpas, int w_start_idx, float OP){
    (void)tasks; (void)metadata; (void)tar; (void)taskidx; (void)lpas; (void)w_start_idx; (void)OP;
    return 0.0f;
}

// --- referenced by assign_write_greedy / assign_write_ctrl (dead for INVW) --
// Real behavior: find a block in `head` matching cond (YOUNG/OLD) and remove it.
// Implemented properly (not just stubbed) because it is a small, exact combo of
// existing linked-list primitives -- lets greedy/ctrl write policies still work
// if the user selects them later.
block* ll_condremove(meta* metadata, bhead* head, int cond){
    int idx = find_block_in_list(metadata, head, cond);
    if(idx < 0) return NULL;
    return ll_remove(head, idx);
}

// --- referenced by task generator switch (dead unless skewness in {>=0, -5}) -
rttask* generate_taskset_skew2(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle){
    (void)tasknum; (void)tot_util; (void)addr; (void)result_util; (void)skewnum; (void)type; (void)cycle;
    fprintf(stderr,"[dead_stubs] generate_taskset_skew2 was never implemented; "
                   "skewness>=0 is not supported in this build.\n");
    abort();
}

rttask* generate_taskset_maxrate(int tasknum, float tot_util, int addr, float* result_util){
    (void)tasknum; (void)tot_util; (void)addr; (void)result_util;
    fprintf(stderr,"[dead_stubs] generate_taskset_maxrate was never implemented; "
                   "skewness == -5 is not supported in this build.\n");
    abort();
}

// --- referenced by BWR_job_start_q (dead: BWR release logic is commented out) -
long gen_bwr_rr(int vic, int tar, long cur_cp, long bwrp, meta* metadata, IOhead* bwrq){
    (void)vic; (void)tar; (void)cur_cp; (void)bwrp; (void)metadata; (void)bwrq;
    return 0L;
}

