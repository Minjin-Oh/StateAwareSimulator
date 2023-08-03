
//substitute with a value of simulator params
int W_EXEC = 500;       //time to write
int R_EXEC = 50;        //time to read
int MINRC = 32;         //minimum reclaimable page

int __calc_gcperiod(int wp, int wn, int _minrc){
    int mult; //a multiplier for GC period (period of GC = mult * write_period)
    
    if (_minrc >= wn){//GC period is longer than write period
        mult = (int)floor((double)_minrc/(double)wn);
        printf("min_rc,wn,mult : %d %d %d\n",_minrc,wn,mult);
        return wp * mult;
    }
    else{//GC period is shorter than write period
        mult = (int)ceil((double)wn/(double)_minrc);
        printf("min_rc,wn,mult : %d %d 1/%d\n",_minrc,wn,mult);
        return (int)(wp * (float)1/(float)mult);
    }
}

void generate_taskset(int tasknum, float tot_util, float* result_util, int cycle){
    
    //params
    float utils[tasknum];                                  //utilization per task
    float util_task[2];                                    //temp array
    int util_ratio[tasknum];                               //random ratio of each task
    int util_ratio_sum;                                    //sum of ratio values
    int wratio, rratio;                                    //ratio of write and read
    int wnum[tasknum], rnum[tasknum];                      //write page num , read page num
    int wp[tasknum], rp[tasknum], gcp[tasknum];            //period varaible
    char pass = 0;                                         //sched check

    //generate a ratio for each task
    util_ratio_sum = 0;
    for(int i=0;i<tasknum;i++){
        util_ratio[i] = rand()%10 + 1;
        util_ratio_sum += util_ratio[i];
    }

    //calculate utilization for each task
    for(int i=0;i<tasknum;i++){
        utils[i] = ((float)util_ratio[i]/(float)util_ratio_sum) * tot_util;
    }

    //assign wn, wp, rn, rp according to utilization & make taskset
    for(int i=0;i<tasknum;i++){
        
        //portion of write, read(1:10 ~ 10:1)
        wratio = rand()%9 + 1;
        rratio = 10 - wratio;
        
        //calculate util for write/read
        util_task[0] = utils[i] * (float)wratio / 10.0;
        util_task[1] = utils[i] * (float)rratio / 10.0;
        
        //randomly generate a flash page under 30pg, 100pg
        wnum[i] = rand()%30 + 1;
        rnum[i] = rand()%100 + 1;
        
        //generate a taskset parameter
        wp[i] = (int)((float)(wnum[i]*W_EXEC) / util_task[0]);
        rp[i] = (int)((float)(rnum[i]*R_EXEC) / util_task[1]);
        gcp[i] = __calc_gcperiod(wp,wnum,MINRC); //GC 주기 계산하는 함수(위 예시 참고해서 본인 구현에 맞게 수정)

        //print new task
        printf("wp: %d, wn : %d, rp : %d, rn : %d, gcp : %d wu: %f, ru: %f\n",
            wp[i],wnum[i],rp[i],rnum[i],gcp[i],util_task[0],util_task[1]);
    }
}