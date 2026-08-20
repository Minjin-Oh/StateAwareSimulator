#include "stateaware.h"

extern double OP;
extern int MINRC;

/* ============================================================================
 * Latency model layer separation
 * ============================================================================
 *
 * The simulator distinguishes two latency models, mirroring the physical
 * device and the controller's perception of it:
 *
 *   PhysicalLatencyModel   — { w_exec_phys, r_exec_phys, e_exec_phys }
 *     Ground truth. PEC-dependent per Fig. 2. Consumed by the simulation
 *     engine to set req->exec, to accumulate metadata->runutils[] (which
 *     records real admitted cost), and to drive the state-aware utilization
 *     overflow check in print_profile_timestamp. Never call from
 *     controller decision code — doing so leaks physics into the model
 *     the controller is supposed to *not* know.
 *
 *   AssumedLatencyModel    — { w_exec_assumed, r_exec_assumed, e_exec_assumed }
 *     What each technique's admission / GC / relocation decision path sees.
 *     Dispatches on latency_mode:
 *       LATENCY_MODE_STATE      : identical to physical (LaWL, our proposal)
 *       LATENCY_MODE_FIXED_BOL  : t_op(cycle) collapsed to t_op(0)   (fresh-block)
 *       LATENCY_MODE_FIXED_EOL  : t_op(cycle) collapsed to t_op(MAXPE) (worn-block)
 *     Never call from sim engine paths that model actual completion time.
 *
 * Ablation intent: run the same taskset and physics through the three modes,
 * varying only what the controller sees. Divergence in resulting lifetime /
 * deadline misses / wear pattern quantifies the value of state-aware
 * schedulability testing. "Analysis is fixed, physics drifts" is enforced
 * at the interface level rather than by convention.
 */

#define LATENCY_MODE_STATE      0  /* PECAware — assumed == physical */
#define LATENCY_MODE_FIXED_BOL  1  /* fresh-block assumption (STARTW/STARTR/STARTE) */
#define LATENCY_MODE_FIXED_EOL  2  /* worn-block assumption  (ENDW /ENDR /ENDE ) */
#define LATENCY_MODE_LAWL_OPT   3  /* paper Table II @ PEC=0     (LaWL-Opt) */
#define LATENCY_MODE_LAWL_AVG   4  /* paper Table II @ PEC=1000  (LaWL-Avg, average lens) */
#define LATENCY_MODE_LAWL_PES   5  /* paper Table II @ PEC=2000  (LaWL-Pes) */

/* Paper Table II reference constants for LaWL-Opt/Avg/Pes. Deliberately kept
 * separate from STARTW/ENDW (types.h) — those define the ground-truth PEC
 * curve consumed by the execution engine and MUST NOT change across
 * variants (Sec. 2.4 "실행 latency 불변"). Only the decision layer swaps. */
#define LAWL_OPT_R  280.0f
#define LAWL_OPT_W  725.0f
#define LAWL_OPT_E  3500.0f
#define LAWL_AVG_R  460.0f
#define LAWL_AVG_W  702.0f
#define LAWL_AVG_E  7000.0f
#define LAWL_PES_R  640.0f
#define LAWL_PES_W  680.0f
#define LAWL_PES_E  14000.0f

/* Set once in emul_main from argv[12]. Read by the *_assumed family only. */
int latency_mode = LATENCY_MODE_STATE;

/* [PES FALLBACK] Counters incremented by findW.c / findGC.c when criterion
 * rejects every candidate and greedy fallback is used. Reset implicitly to
 * 0 at process start (globals); each sim invocation is one run. */
long g_write_fallback_events = 0;
long g_gc_fallback_events    = 0;
#ifdef EXECSTEP
    extern prof_exec exec_steps;
    float w_exec_phys(int cycle){
        for(int i=0;i<exec_steps.pe_steps[0];i++){
            if(cycle < exec_steps.pe_thres[0][i]){
                return (float)exec_steps.pe_values[0][i];
            }
        }
        return (float)exec_steps.pe_values[0][exec_steps.pe_steps[0]];
    }
    float r_exec_phys(int cycle){
        for(int i=0;i<exec_steps.pe_steps[1];i++){
            if(cycle < exec_steps.pe_thres[1][i]){
                return (float)exec_steps.pe_values[1][i];
            }
        }
        return (float)exec_steps.pe_values[1][exec_steps.pe_steps[1]];
    }
    float e_exec_phys(int cycle){
        for(int i=0;i<exec_steps.pe_steps[2];i++){
            if(cycle < exec_steps.pe_thres[2][i]){
                return (float)exec_steps.pe_values[2][i];
            }
        }
        return (float)exec_steps.pe_values[2][exec_steps.pe_steps[2]];
    }
   
#endif
#ifndef EXECSTEP
    float w_exec_phys(int cycle){
        // !!!write exec tends to decrease, so be aware of that!!!
        float wgrad = (float)(ENDW-STARTW)/(float)MAXPE;
        float wconst = STARTW;
        return wgrad*cycle + wconst;
    }

    float r_exec_phys(int cycle){
        // !!!read exec tends to increase, so be aware of that!!!
        float rgrad = (float)(ENDR-STARTR)/(float)MAXPE;
        float rconst = STARTR;
        return rgrad*cycle + rconst;
    }

    float e_exec_phys(int cycle){
        // !!!read exec tends to increase, so be aware of that!!!
        float egrad = (float)(ENDE-STARTE)/(float)MAXPE;
        float econst = STARTE;
        return egrad*cycle + econst;
    }
#endif

/* AssumedLatencyModel: controller-facing exec times.
 * All allocation / GC / relocation admission decisions in findGC.c, findW.c,
 * findRR.c, and assignW.c route through these. Physical fallthrough happens
 * when latency_mode == LATENCY_MODE_STATE, keeping LaWL bit-identical to
 * the ground-truth path. FIXED_BOL/EOL collapse the PEC-dependent curve to
 * a single lens so an ablation can quantify the cost of *not* knowing the
 * actual latency at decision time. */
float w_exec_assumed(int cycle){
    switch(latency_mode){
        case LATENCY_MODE_FIXED_BOL: return (float)STARTW;
        case LATENCY_MODE_FIXED_EOL: return (float)ENDW;
        case LATENCY_MODE_LAWL_OPT:  return LAWL_OPT_W;
        case LATENCY_MODE_LAWL_AVG:  return LAWL_AVG_W;
        case LATENCY_MODE_LAWL_PES:  return LAWL_PES_W;
        default:                     return w_exec_phys(cycle);
    }
}
float r_exec_assumed(int cycle){
    switch(latency_mode){
        case LATENCY_MODE_FIXED_BOL: return (float)STARTR;
        case LATENCY_MODE_FIXED_EOL: return (float)ENDR;
        case LATENCY_MODE_LAWL_OPT:  return LAWL_OPT_R;
        case LATENCY_MODE_LAWL_AVG:  return LAWL_AVG_R;
        case LATENCY_MODE_LAWL_PES:  return LAWL_PES_R;
        default:                     return r_exec_phys(cycle);
    }
}
float e_exec_assumed(int cycle){
    switch(latency_mode){
        case LATENCY_MODE_FIXED_BOL: return (float)STARTE;
        case LATENCY_MODE_FIXED_EOL: return (float)ENDE;
        case LATENCY_MODE_LAWL_OPT:  return LAWL_OPT_E;
        case LATENCY_MODE_LAWL_AVG:  return LAWL_AVG_E;
        case LATENCY_MODE_LAWL_PES:  return LAWL_PES_E;
        default:                     return e_exec_phys(cycle);
    }
}

int myceil(float a){
    int b = (int)a;
    if(a==b) return b;
    else return b+1;
}

int myfloor(float a){
    int b = (int)a;
    return b;
}

int _gc_period(rttask* task,int _minrc){
    int mult;
    int gcp, min_reclaim;
    min_reclaim = _minrc;
    if (min_reclaim >= task->wn){
        mult = myfloor((float)min_reclaim/(float)task->wn);
        return task->wp * mult;
    }
    else{
        mult = myceil((float)task->wn/(float)min_reclaim);
        return task->wp * (float)1/(float)mult;
    }
    printf("gc period calc fail\n");
    sleep(1);
    abort();
}

// in init stage, task structure does not have correct data
// use integers instead of structure to calc gc multiplication.
int __calc_gcmult(int wp, int wn, int _minrc){
    int mult;
    int min_reclaim;
    min_reclaim = _minrc;
    if (min_reclaim >= wn){
        mult = myfloor((float)min_reclaim/(float)wn);
        printf("min_rc,wn,mult : %d %d %d\n",min_reclaim,wn,mult);
        return wp * mult;
    }
    else{
        mult = myceil((float)wn/(float)min_reclaim);
        printf("min_rc,wn,mult : %d %d 1/%d\n",min_reclaim,wn,mult);
        return (int)(wp * (float)1/(float)mult);
    }
    printf("gc period calc fail\n");
    sleep(1);
    abort();
}


float __calc_ru(rttask* task, int scale_r){
    return (float)(task->rn*r_exec_phys(scale_r))/(float)task->rp;
}
float __calc_wu(rttask* task, int scale_w){
    return (float)(task->wn*w_exec_phys(scale_w))/(float)task->wp;
}

float __calc_gcu(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e){
    int max_valid = PPB - min_rc;
    float gc_exec, gc_period;
    gc_exec = (max_valid)*(w_exec_phys(scale_w)+r_exec_phys(scale_r))+e_exec_phys(scale_e);
    gc_period = _gc_period(task,min_rc);
    return (float)gc_exec / (float)gc_period;
}

// [FIXED-LATENCY] decision-time utility helpers. Identical formulas to the
// SA versions above but consult *_exec_dec so they honor latency_mode.
float __calc_ru_assumed(rttask* task, int scale_r){
    return (float)(task->rn*r_exec_assumed(scale_r))/(float)task->rp;
}
float __calc_wu_assumed(rttask* task, int scale_w){
    return (float)(task->wn*w_exec_assumed(scale_w))/(float)task->wp;
}
float __calc_gcu_assumed(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e){
    int max_valid = PPB - min_rc;
    float gc_exec, gc_period;
    gc_exec = (max_valid)*(w_exec_assumed(scale_w)+r_exec_assumed(scale_r))+e_exec_assumed(scale_e);
    gc_period = _gc_period(task,min_rc);
    return (float)gc_exec / (float)gc_period;
}

int __get_min(int a, int b, int c){
    if (a<b){
        if(a<c){return a;}
        else{return c;}
    }
    else{
        if(b<c){return b;}
        else{return c;}
    }
}

double find_max_double(double a, double b, double c){
    if (a<b){
        if (b<c){return c;}
        else {return b;}
    }else{
        if (a<c){return c;}
        else{return a;}
    }
}

/* tasks[] and MINRC are constants for the duration of a run, so this reduces
 * to a scalar the first time it's asked and then just returns the memo. Hot
 * because find_util_safe_dec → find_cur_util_dec → _find_min_period fires
 * inside per-write / per-GC feasibility loops (~14M writes × several probes). */
static int __min_period_cache      = -1;
static rttask* __min_period_task_p = NULL;
static int __min_period_tasknum    = -1;

int _find_min_period(rttask* task,int tasknum){
    if(task == __min_period_task_p && tasknum == __min_period_tasknum
       && __min_period_cache > 0){
        return __min_period_cache;
    }
    int ret = -1;
    int min_each_task = -1;
    for(int i=0;i<tasknum;i++){
        int temp = _gc_period(&(task[i]),(int)(MINRC));
        min_each_task = __get_min(task[i].wp,task[i].rp,temp);
        if(i==0){
            ret = min_each_task;
        }
        else if(ret >= min_each_task){
            ret = min_each_task;
        }
    }

    if(ret == -1){
        printf("min period not found\n");
        sleep(1);
        abort();
    }
    __min_period_cache      = ret;
    __min_period_task_p     = task;
    __min_period_tasknum    = tasknum;
    return ret;
}

float calc_std(meta* metadata){
    float avg, var;
    int sum = 0;
    var = 0.0;
    for(int i=0;i<NOB;i++){
        sum += (float)metadata->state[i];
    }
    avg = (float)sum/(float)NOB;
    for(int i=0;i<NOB;i++){
        float a = (float)metadata->state[i] - avg;
        float b = a*a;
        var += b;
    }
    return sqrt(var/(float)NOB);
}

float find_worst_util(rttask* task, int tasknum, meta* metadata){
    // calculate worst-case utilization regarding to current block state
    // as a worst case bound, simply assume that each task independantly chooses the block.
    // find oldest, freshest (-> youngest로 변경) block
    int youngest, oldest, sum;
    float avg, var;
    sum = 0;
    var = 0.0;
    for(int i=0;i<NOB;i++){
        sum += (float)metadata->state[i];
        if(i==0){
            youngest = metadata->state[i];
            oldest = metadata->state[i];
        }
        else{
            if (youngest >= metadata->state[i]){
                youngest = metadata->state[i];
            }
            if (oldest <= metadata->state[i]){
                oldest = metadata->state[i];
            }
        }
    }
    avg = (float)sum/(float)NOB;
    for(int i=0;i<NOB;i++){
        float a = (float)metadata->state[i] - avg;
        float b = a*a;
        var += b;
        // printf("dev : %f,dev^2 : %f, cur_var : %f\n",a,b,var);
    }
    // printf("oldest : %d, youngest : %d, std : %f\n",oldest,youngest,sqrt(var/(float)NOB));
    // with the oldest/youngest, calculate total utilization
    float total_u = 0.0;
    int min_period;
    for(int i=0;i<tasknum;i++){
        // write util
        total_u += __calc_wu(&(task[i]),youngest);
        // read util
        total_u += __calc_ru(&(task[i]),oldest);
        // GC util
        total_u += __calc_gcu(&(task[i]),MINRC,youngest,oldest,oldest);
        // printf("[WC]cur_util:%f\n",total_u);
    }
    // add blocking factor
    total_u += (float)e_exec_phys(oldest) / (float)_find_min_period(task,tasknum);
    // printf("[WC]exec : %f, min_p :%d\n",e_exec_phys(oldest),_find_min_period(task,tasknum,OP));
    // printf("[WC]worst case util is %f\n",total_u);
    return total_u;
}

float find_cur_util(rttask* tasks, int tasknum, meta* metadata, int old){
    float total_u = 0.0;
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        // printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    total_u += (float)e_exec_phys(old) / (float)_find_min_period(tasks,tasknum);
    return total_u;
}

int find_util_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util){
    // check if current I/O job does not violate util test along with recently released other jobs.

    float total_u = 0.0;

    total_u = find_cur_util(tasks,tasknum,metadata,old);
    if(type == WR){
        total_u -= metadata->runutils[0][taskidx];
    } else if (type == RD){
        total_u -= metadata->runutils[1][taskidx];
    } else if (type == GC){
        total_u -= metadata->runutils[2][taskidx];
    }
    // printf("tot_u without cur task : %f ",total_u);
    total_u += util;
    // printf("tot_u : %f\n",total_u);
    if (total_u <= 1.0){
        return 0;
    } else if (total_u > 1.0){
        return -1;
    } else {
        printf("util safety check failed\n");
        abort();
    }
}

// [FIXED-LATENCY BEGIN] --------------------------------------------------------
// Decision-time twins of find_worst_util / find_cur_util / find_util_safe.
// The only difference is that per-state utility contributions and the blocking
// term e_exec go through *_exec_dec so LaWL sees fixed latency under FIXED_S /
// FIXED_E while ground-truth callers in logger.c stay unaffected.
//
// Note: metadata->runutils[] are still updated by the runtime with state-aware
// values (that's ground truth of what was admitted). find_util_safe_dec uses
// those as-is, and only the *marginal* term for the current decision + the
// e_exec blocking term switch to fixed. This is intentional: the criterion
// applied to *new* admission decisions is fixed; historical admitted work is
// not retroactively re-scored.
float find_worst_util_assumed(rttask* task, int tasknum, meta* metadata){
    int youngest, oldest;
    youngest = metadata->state[0];
    oldest = metadata->state[0];
    for(int i=1;i<NOB;i++){
        if (youngest >= metadata->state[i]) youngest = metadata->state[i];
        if (oldest   <= metadata->state[i]) oldest   = metadata->state[i];
    }
    float total_u = 0.0;
    for(int i=0;i<tasknum;i++){
        total_u += __calc_wu_assumed(&(task[i]),youngest);
        total_u += __calc_ru_assumed(&(task[i]),oldest);
        total_u += __calc_gcu_assumed(&(task[i]),MINRC,youngest,oldest,oldest);
    }
    total_u += (float)e_exec_assumed(oldest) / (float)_find_min_period(task,tasknum);
    return total_u;
}

float find_cur_util_assumed(rttask* tasks, int tasknum, meta* metadata, int old){
    float total_u = 0.0;
    for(int j=0;j<tasknum;j++){
        total_u += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
    }
    total_u += (float)e_exec_assumed(old) / (float)_find_min_period(tasks,tasknum);
    return total_u;
}

int find_util_safe_assumed(rttask* tasks, int tasknum, meta* metadata, int old,
                       int taskidx, int type, float util){
    float total_u = find_cur_util_assumed(tasks,tasknum,metadata,old);
    if(type == WR)       total_u -= metadata->runutils[0][taskidx];
    else if(type == RD)  total_u -= metadata->runutils[1][taskidx];
    else if(type == GC)  total_u -= metadata->runutils[2][taskidx];
    total_u += util;
    if (total_u <= 1.0)      return 0;
    else if (total_u > 1.0)  return -1;
    printf("util safety check failed\n");
    abort();
}
// [FIXED-LATENCY END] ----------------------------------------------------------

int util_check_main(){
    // exec function test
    printf("[exec time scaling]\n");
    printf("20 cycle %f %f %f\n",w_exec_phys(20),r_exec_phys(20),e_exec_phys(20));
    printf("10 cycle %f %f %f\n",w_exec_phys(10),r_exec_phys(10),e_exec_phys(10));
    printf("00 cycle %f %f %f\n",w_exec_phys(0),r_exec_phys(0),e_exec_phys(0));
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*3);
    init_task(&(tasks[0]),1,STARTW*50,65,STARTR*10,10,__calc_gcmult(STARTW*50,65,(int)(PPB*OP)),0,PPB*OP);
    printf("[util calc check]\n");
    printf("rd util e:%d p:%d u:%f\n",tasks[0].rn*STARTR, tasks[0].rp,__calc_ru(&(tasks[0]),0));
    printf("wt util e:%d p:%d u:%f\n",tasks[0].wn*STARTW, tasks[0].wp,__calc_wu(&(tasks[0]),0));
    int gc_exec = (PPB-(int)(PPB*OP))*(w_exec_phys(0)+r_exec_phys(0))+e_exec_phys(0);
    int gc_period = _gc_period(&(tasks[0]),(int)(PPB*OP));
    printf("gc util e:%d p:%d u:%f\n",gc_exec,gc_period,__calc_gcu(&(tasks[0]),(int)(PPB*OP),0,20,20));
}

long get_gc_locktime(meta* metadata, int blockidx){
    // set locktime as a time until 75% of block is invalidated
    int lpa;
    long ret = 0L;
    long temp[PPB];
    int offset = blockidx*PPB;
    for(int i=offset;i<offset+PPB;i++){
        lpa = metadata->rmap[i];
        temp[i-offset] = metadata->next_update[lpa];
        printf("lpa %d, next update %ld\n",lpa,metadata->next_update[lpa]);
    }
    qsort(temp,PPB,sizeof(long),compare);
    ret = temp[PPB/4 * 3];
    return ret;
}

void print_blocklist_info(bhead* head, meta* metadata){
    block* cur = head->head;
    int cyc_yng = MAXPE;
    int cyc_old = 0;
    int invnum_min = PPB;
    int invnum_max = 0;
    int invnum_avg = 0;
    while(cur != NULL){
        if(metadata->invnum[cur->idx] <= invnum_min){
            invnum_min = metadata->invnum[cur->idx];
        }
        if(metadata->invnum[cur->idx] >= invnum_max){
            invnum_max = metadata->invnum[cur->idx];
        }
        if(metadata->state[cur->idx] <= cyc_yng){
            cyc_yng = metadata->state[cur->idx];
        }
        if(metadata->state[cur->idx] >= cyc_old){
            cyc_old = metadata->state[cur->idx];
        }
        invnum_avg += metadata->invnum[cur->idx];
        cur = cur->next;
    }
    if(head->blocknum != 0){
        // printf("[listinfo]yng:%d,old:%d,invmax:%d,invmin:%d, invavg:%d\n",cyc_yng,cyc_old,invnum_max,invnum_min,invnum_avg/head->blocknum);
    }
}

void print_fullblock_info(meta* metadata, bhead* full_head, long cur_cp, FILE* fp){
    // a function which  prints a full block whenever invnum reaches PPB.
    // called only when finish_WR makes completely full invalid block.
    block* cur = full_head->head;
    
    fprintf(fp, "%ld, ",cur_cp);
    while(cur != NULL){
        //check if invalid == 128
        if(metadata->invnum[cur->idx] == 128){
            fprintf(fp, "%d, ",cur->idx);
        }
        cur = cur->next;
    }
    fprintf(fp, "\n");
}

void print_maxinvalidation_block(meta* metadata, int blockidx){
    int startidx = blockidx*PPB;
    int lpa;
    long longest_next_update = 0;
    for(int i=0;i<PPB;i++){
        lpa = metadata->rmap[startidx+i];
        if(lpa != -1){
            // update longest next update
            if(longest_next_update < metadata->next_update[lpa]){
                longest_next_update = metadata->next_update[lpa];    
            }
            // printf("[%d]%ld\n",lpa,metadata->next_update[lpa]);
        }
        
    }
    printf("bidx : %d, full invalid at %ld\n",blockidx,longest_next_update);
}

int find_block_in_list(meta* metadata, bhead* head, int cond){
    // a function wihch finds a block in given blocklist, considering its state.
    // cond == YOUNG ; find youngest block in blocklist
    // cond == OLD ; find oldest block in blocklist
    int ret = -1;
    block* b;
    int compare;
    if(cond == YOUNG){
        compare = MAXPE;
        b = head->head;
        while(b != NULL){
            if(compare > metadata->state[b->idx]){
                ret = b->idx;
                compare = metadata->state[b->idx];
            }
            b = b->next;
        }
    }
    else if(cond == OLD){
        compare = 0;
        b = head->head;
        while(b != NULL){
            if(compare < metadata->state[b->idx]){
                ret = b->idx;
                compare = metadata->state[b->idx];
            }
            b = b->next;
        }
    }
    return ret;
}