#include "stateaware.h"

extern double OP;
extern int MINRC;

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

//in init stage, task structure does not have correct data
//use integers instead of structure to calc gc multiplication.
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

int _find_min_period(rttask* task,int tasknum){
    int ret = -1;
    int min_each_task = -1;
    for(int i=0;i<tasknum;i++){
        int temp = _gc_period(&(task[i]),(int)(MINRC));
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
    //calculate worst-case utilization regarding to current block state
    //as a worst case bound, simply assume that each task independantly chooses the block.
    //find oldest, freshest block
    int freshest, oldest, sum;
    float avg, var;
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
        total_u += __calc_gcu(&(task[i]),MINRC,freshest,oldest,oldest);
        //printf("[WC]cur_util:%f\n",total_u);
    }
    //add blocking factor
    total_u += (float)e_exec(oldest) / (float)_find_min_period(task,tasknum);
    //printf("[WC]exec : %f, min_p :%d\n",e_exec(oldest),_find_min_period(task,tasknum,OP));
    //rintf("[WC]worst case util is %f\n",total_u);
    return total_u;
}

float find_cur_util(rttask* tasks, int tasknum, meta* metadata, int old){
    float total_u = 0.0;
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    return total_u;
}

int find_util_safe(rttask* tasks, int tasknum, meta* metadata, int old, int taskidx, int type, float util){
    float total_u = 0.0;
    
    total_u = find_cur_util(tasks,tasknum,metadata,old);
    if(type == WR){
        total_u -= metadata->runutils[0][taskidx];
    } else if (type == RD){
        total_u -= metadata->runutils[1][taskidx];
    } else if (type == GC){
        total_u -= metadata->runutils[2][taskidx];
    }
    total_u += util;
    if (total_u <= 1.0){
        return 0;
    } else if (total_u > 1.0){
        return -1;
    } else {
        printf("util safety check failed\n");
        abort();
    }
}

int util_check_main(){
    //exec function test
    printf("[exec time scaling]\n");
    printf("20 cycle %f %f %f\n",w_exec(20),r_exec(20),e_exec(20));
    printf("10 cycle %f %f %f\n",w_exec(10),r_exec(10),e_exec(10));
    printf("00 cycle %f %f %f\n",w_exec(0),r_exec(0),e_exec(0));
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*3);
    init_task(&(tasks[0]),1,STARTW*50,65,STARTR*10,10,__calc_gcmult(STARTW*50,65,(int)(PPB*OP)),0,PPB*OP);
    printf("[util calc check]\n");
    printf("rd util e:%d p:%d u:%f\n",tasks[0].rn*STARTR, tasks[0].rp,__calc_ru(&(tasks[0]),0));
    printf("wt util e:%d p:%d u:%f\n",tasks[0].wn*STARTW, tasks[0].wp,__calc_wu(&(tasks[0]),0));
    int gc_exec = (PPB-(int)(PPB*OP))*(w_exec(0)+r_exec(0))+e_exec(0);
    int gc_period = _gc_period(&(tasks[0]),(int)(PPB*OP));
    printf("gc util e:%d p:%d u:%f\n",gc_exec,gc_period,__calc_gcu(&(tasks[0]),(int)(PPB*OP),0,20,20));
}


/*while(cur != NULL){
        if(metadata->state[cur->idx] != old && metadata->invnum[cur->idx] >= MINRC){
            cur_state = metadata->state[cur->idx];
            new_rc = metadata->invnum[cur->idx];
            float gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            float gc_period = _gc_period(&(task[taskidx]),(int)(new_rc));
            cur_gc = gc_exec/gc_period;
            if(cur_gc <= cur_minutil){
                expected_idx = cur->idx;
                cur_minutil = cur_gc;
            }
        }
        cur = cur->next;
    }
*/

/*while(cur != NULL){
        if(metadata->invnum[cur->idx] >= MINRC){
            cur_state = metadata->state[cur->idx];
            new_rc = metadata->invnum[cur->idx];
            float profile_util = 0.0;
            float gc_exec = (PPB-new_rc)*(w_exec(yng)+r_exec(cur_state))+e_exec(cur_state);
            float gc_period = _gc_period(&(task[taskidx]),(int)(new_rc));
            cur_gc = gc_exec/gc_period;
            profile_util = cur_gc + util_profile[cur_state+1];
            if(profile_util <= cur_minutil){
                expected_idx = cur->idx;
                cur_minutil = cur_gc + util_profile[cur_state+1];
            }
        }
        cur = cur->next;
    }
*/

/*//print functions for util checking
printf("util check : ");
    for(int i=0;i<MAXPE;i++){
        util_profile[i] = 0.0;
        //add readworst util
        for(int j=0;j<tasknum;j++){
            if(i <= metadata->cur_read_worst[j]){
                util_profile[i] += __calc_ru(&(task[j]),metadata->cur_read_worst[j]);
            } else {
                util_profile[i] += __calc_ru(&(task[j]),i);
            }
            util_profile[i] += __calc_wu(&(task[j]),i);
        }
        printf("%d(%f), ",i,util_profile[i]);
    }
    printf("\n");*/
    //sleep(1);