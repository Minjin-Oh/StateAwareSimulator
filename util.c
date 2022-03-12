#include "stateaware.h"

float w_exec(int cycle){
    //!!!write exec tends to decrease, so be aware of that!!!
    float wgrad = (float)(ENDW-STARTW)/(float)MAXPE;
    float wconst = STARTW;
    return wgrad*cycle + wconst;
}

float r_exec(int cycle){
    //!!!read exec tends to increase, so be aware of that!!!
    float rgrad = (float)(ENDR-STARTR)/(float)MAXPE;
    float rconst = STARTR;
    return rgrad*cycle + rconst;
}

float e_exec(int cycle){
    //!!!read exec tends to increase, so be aware of that!!!
    float egrad = (float)(ENDE-STARTE)/(float)MAXPE;
    float econst = STARTE;
    return egrad*cycle + econst;
}

int myceil(float a){
    int b = (int)a;
    if(a-b>0) return b+1;
    else return a;
}

int myfloor(float a){
    int b = (int)a;
    return a;
}
int _gc_period(rttask* task,int MINRC){
    int mult;
    int gcp, min_reclaim;
    min_reclaim = MINRC;
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

//in init stage, task structure does not have correct data
//use integers instead of structure to calc gc multiplication.
float __calc_gcmult(int wp, int wn, int MINRC){
    int mult;
    int gcp, min_reclaim;
    min_reclaim = MINRC;
    if (min_reclaim >= wn){
        mult = myfloor((float)min_reclaim/(float)wn);
        //printf("min_rc,wn,mult : %d %d %d\n",min_reclaim,wn,mult);
        return wp * mult;
    }
    else{
        mult = myceil((float)wn/(float)min_reclaim);
        return wp * (float)1/(float)mult;
    }
    printf("gc period calc fail\n");
    sleep(1);
    abort();
}


float __calc_ru(rttask* task, int scale_r){
    return (float)(task->rn*r_exec(scale_r))/(float)task->rp;
}
float __calc_wu(rttask* task, int scale_w){
    return (float)(task->wn*w_exec(scale_w))/(float)task->wp;
}

float __calc_gcu(rttask* task, int min_rc, int scale_w, int scale_r, int scale_e){
    int max_valid = PPB - min_rc;
    float gc_exec, gc_period;
    gc_exec = (max_valid)*(w_exec(scale_w)+r_exec(scale_r))+e_exec(scale_e);
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

int _find_min_period(rttask* task,int tasknum,float _OP){
    int ret = -1;
    int min_each_task = -1;
    for(int i=0;i<tasknum;i++){
        int temp = _gc_period(&(task[i]),(int)(PPB*_OP));
        //min btw r,w,gc
        //printf("[internal]comp btw%d %d %d\n",task[i].wp,task[i].rp,temp);
        min_each_task = __get_min(task[i].wp,task[i].rp,temp);
        //printf("%d\n",min_each_task);
        if(i==0) 
            ret = min_each_task;
        else if(ret >= min_each_task)
            ret = min_each_task;
    }   

    if(ret == -1){
        printf("min period not found\n");
        sleep(1);
        abort();
    }
    return ret;
}
float find_worst_util(rttask* task, int tasknum, meta* metadata){
    //calculate worst-case utilization regarding to current block state
    //as a worst case bound, simply assume that each task independantly chooses the block.

    //find oldest, freshest block
    int freshest, oldest, sum;
    float std, avg, var;
    sum = 0;
    var = 0.0;
    for(int i=0;i<NOB;i++){
        sum += (float)metadata->state[i];
        if(i==0){
            freshest = metadata->state[i];
            oldest = metadata->state[i];
        }
        else{
            if (freshest >= metadata->state[i]){
                freshest = metadata->state[i];
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
        //printf("dev : %f,dev^2 : %f, cur_var : %f\n",a,b,var);
    }
    //printf("oldest : %d, freshest : %d, std : %f\n",oldest,freshest,sqrt(var/(float)NOB));
    //with the oldest/freshest, calculate total utilization
    float total_u = 0.0;
    int min_period;
    for(int i=0;i<tasknum;i++){
        //write util
        total_u += __calc_wu(&(task[i]),freshest);
        //read util
        total_u += __calc_ru(&(task[i]),oldest);
        //GC util
        total_u += __calc_gcu(&(task[i]),(int)(PPB*OP),freshest,oldest,oldest);
        //printf("[WC]cur_util:%f\n",total_u);
    }
    //add blocking factor
    total_u += (float)e_exec(oldest) / (float)_find_min_period(task,tasknum,OP);
    //printf("[WC]exec : %f, min_p :%d\n",e_exec(oldest),_find_min_period(task,tasknum,OP));
    printf("[WC]worst case util is %f\n",total_u);
    return total_u;
}

float find_SAworst_util(rttask* task, int tasknum, meta* metadata){
    //Find a bit more optimized value as a worst utilization.
    //check if a system can bound the worst case utilization,
    //by restricting "cycle count" of target block

    //params
    float SA_total_u = 0.0;
    int freshest, oldest;
    int limit_fresh = 0;
    int limit_old = 0;
    int feasible = NOB;
    int minimum_period = -1;
    
    //find cycle bound.
    for(int i=0;i<NOB;i++){
        if(i==0){
            freshest = metadata->state[i];
            oldest = metadata->state[i];
        }
        else{
            if (freshest >= metadata->state[i]){
                freshest = metadata->state[i];
            }
            if (oldest <= metadata->state[i]){
                oldest = metadata->state[i];
            }
        }
    }
    printf("fresh:%d,old:%d\n",freshest,oldest);
    //find the possible window for task.
    //shrink window = lower write util(write accel), higher gc util (OP decrease)
    limit_fresh = freshest;
    limit_old = oldest;
    float prev_util = 0.0;
    float cur_util = 0.0;
    int prev_min_reclaim = 0;
    int cur_min_reclaim = 0;
    float utils[MAXPE-freshest];
    for(int a=0;a<tasknum;a++){
        for(int i=0;i<MAXPE-freshest;i++){
            feasible = NOB;
            //check if how many block is feasible
            for(int j=0;j<NOB;j++){
                if(metadata->state[j] < limit_fresh) feasible--;
            }

            //find new minimum reclaimable page. 
            //lpa not changed. calc as (1-OP) * originalphysicalspace
            float new_phy = (float)(feasible*PPB);
            float orig_logi = (float)(NOP*(1-OP));
            float newOP = 1 - orig_logi/new_phy;
            if(newOP <= 0.0){
                utils[i] = 100.0;//unshrinkable
                //printf("[%d]%f\n",limit_fresh,utils[i]);
                limit_fresh++;
                continue;
            }
            int min_reclaim = (int)(newOP*PPB);
            if(prev_min_reclaim == 0){
                prev_min_reclaim = min_reclaim;
            }
            printf("[SA]feasible block : %d, newOP : %f, min_rec : %d\n",feasible,newOP,min_reclaim);
            
            //prev_util vs cur util
            float cur_r = __calc_ru(&(task[a]),oldest);
            float cur_w = __calc_wu(&(task[a]),limit_fresh);
            float cur_gc = __calc_gcu(&(task[a]),min_reclaim,limit_fresh,oldest,oldest);
            cur_util = cur_r+cur_w+cur_gc;    
            if(prev_util == 0.0){
                prev_util = cur_util;
            }
            else{/*do not update prev value, if it already exists*/}
            printf("[SA][%d] prev util : %f, new util : %f\n",limit_fresh,prev_util,cur_util);
            int task_minp = __get_min(task[a].wp,task[a].rp,_gc_period(&(task[a]),prev_min_reclaim));
            
            //update minimum period if necessary
            if(minimum_period == -1)
                minimum_period = task_minp;
            else if (minimum_period >= task_minp)
                minimum_period = task_minp;

            //record utilization
            utils[i] = cur_util + (float)e_exec(oldest)/(float)minimum_period;
            
            prev_util = cur_util;
            prev_min_reclaim = min_reclaim;
            limit_fresh++;
            printf("[SA][%d]%f\n",limit_fresh,utils[i]);
            //At this point, update all information for next check next case.

        }//single task ended shrink
    }//every task ended shrink
    for(int i=0;i<MAXPE-freshest;i++){
        if(SA_total_u == 0.0){
            SA_total_u = utils[i];
        }
        else if(SA_total_u >= utils[i]){
            SA_total_u = utils[i];
        }
        else{/*do nothing*/}
    }
    printf("[SA]sa total utilization is %f\n",SA_total_u);
    return SA_total_u;
}

int util_check_main(){
    //exec function test
    printf("[exec time scaling]\n");
    printf("20 cycle %f %f %f\n",w_exec(20),r_exec(20),e_exec(20));
    printf("10 cycle %f %f %f\n",w_exec(10),r_exec(10),e_exec(10));
    printf("00 cycle %f %f %f\n",w_exec(0),r_exec(0),e_exec(0));
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*3);
    init_task(&(tasks[0]),1,STARTW*50,65,STARTR*10,10,__calc_gcmult(STARTW*50,65,(int)(PPB*OP)));
    printf("[util calc check]\n");
    printf("rd util e:%d p:%d u:%f\n",tasks[0].rn*STARTR, tasks[0].rp,__calc_ru(&(tasks[0]),0));
    printf("wt util e:%d p:%d u:%f\n",tasks[0].wn*STARTW, tasks[0].wp,__calc_wu(&(tasks[0]),0));
    int gc_exec = (PPB-(int)(PPB*OP))*(w_exec(0)+r_exec(0))+e_exec(0);
    int gc_period = _gc_period(&(tasks[0]),(int)(PPB*OP));
    printf("gc util e:%d p:%d u:%f\n",gc_exec,gc_period,__calc_gcu(&(tasks[0]),(int)(PPB*OP),0,20,20));
    
}

float calc_std(meta* metadata){
    float std, avg, var;
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