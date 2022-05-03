#include "stateaware.h"

rttask* generate_taskset(int tasknum, float tot_util,int addr, float* result_util, int cycle){
    //params
    //what about address??
    float utils[tasknum];
    float util_task[2];
    int util_ratio[tasknum];
    int util_ratio_sum;
    int wratio, rratio;
    int wnum, rnum;
    int wp, rp, gcp;
    char pass = 0;
    rttask* tasks;
    //generate a portion for each task
    util_ratio_sum = 0;
    for(int i=0;i<tasknum;i++){
        util_ratio[i] = rand()%10 + 1;
        util_ratio_sum += util_ratio[i];
    }//! generated portion

    //utilization ratio
    for(int i=0;i<tasknum;i++){
        utils[i] = ((float)util_ratio[i]/(float)util_ratio_sum) * tot_util;
    }

    //assign wn, wp, rn, rp according to utilization & make taskset
    tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    for(int i=0;i<tasknum;i++){
        //portion inside taskset
        wratio = rand()%9 + 1;
        rratio = 10 - wratio;
        //generate a taskset
        util_task[0] = utils[i] * (float)wratio / 10.0;
        util_task[1] = utils[i] * (float)rratio / 10.0;
        
        //!!restrictions!!
        //period should be over 100ms --> considering a blocking factor
        //access number of page should be under 200pg --> considering a free page limit
        //randomly generate a flash page under 200pg
        wnum = rand()%30 + 1;
        rnum = rand()%100 + 1;
        //generate a taskset parameter
        wp = (int)((float)(wnum*STARTW) / util_task[0]);
        rp = (int)((float)(rnum*STARTR) / util_task[1]);
           
        gcp = __calc_gcmult(wp,wnum,MINRC);
        init_task(&(tasks[i]),i,wp,wnum,rp,rnum,gcp,addr/tasknum*(i),addr/tasknum*(i+1)-1);
        printf("wp: %d, wn : %d, rp : %d, rn : %d, gcp : %d wu: %f, ru: %f, gcu : %f\n",
            wp,wnum,rp,rnum,gcp,util_task[0],util_task[1],__calc_gcu(&(tasks[i]),MINRC,0,0,0));
        
    }
    float checker = 0.0;
    float WRsum = 0.0;
    for(int i=0;i<tasknum;i++){
        WRsum += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        WRsum += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
    }
    for(int i=0;i<tasknum;i++){
        checker += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        checker += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
        checker += __calc_gcu(&(tasks[i]),MINRC,0,0,0);
    }
    checker += (float)STARTE/(float)_find_min_period(tasks,tasknum);
    printf("WRsum : %f, checker : %f\n",WRsum, checker);
    *result_util = checker;
    return tasks;
}

rttask* generate_taskset_skew(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle){
    /*assign 90% of utilization to certain amount of tasks*/
    float utils[tasknum];
    float util_task[2];
    int util_ratio[tasknum];
    int util_ratio_sum = 0;
    int wratio, rratio;
    int wnum, rnum;
    int wp, rp, gcp;
    char pass = 0;
    rttask* tasks;
    float skew = tot_util * 0.9 / skewnum;
    for(int i=0;i<tasknum;i++){
        if (i < skewnum){
            util_ratio[i] = -1;
        }
        else {
            util_ratio[i] = rand()%10 + 1;
            util_ratio_sum += util_ratio[i];
        }
    }
    for(int i=0;i<tasknum;i++){
        if( i < skewnum){
            utils[i] = skew;
        }
        else{
            printf("ratio:%f\n",(float)util_ratio[i]/(float)util_ratio_sum);
            utils[i] = ((float)util_ratio[i]/(float)util_ratio_sum) * (tot_util * 0.1);

        }
    }
    for(int i=0;i<tasknum;i++){
        printf("%f\n",utils[i]);
    }
    tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    for(int i=0;i<tasknum;i++){
        if(i < skewnum){//skewed task
            if(type == 0){
                wratio = 9;
                rratio = 1;
            } else if (type == 1){
                wratio = 1;
                rratio = 9;
            }
            util_task[0] = utils[i] * (float)wratio / 10.0;
            util_task[1] = utils[i] * (float)rratio / 10.0;
        } else { // not skewed task
            wratio = rand()%9 + 1;
            rratio = 10 - wratio;
            util_task[0] = utils[i] * (float)wratio / 10.0;
            util_task[1] = utils[i] * (float)rratio / 10.0;
        }

        wnum = rand()%30 + 1;
        rnum = rand()%100 + 1;
        //generate a taskset parameter
        wp = (int)((float)(wnum*STARTW) / util_task[0]);
        rp = (int)((float)(rnum*STARTR) / util_task[1]);
           
        gcp = __calc_gcmult(wp,wnum,MINRC);
        init_task(&(tasks[i]),i,wp,wnum,rp,rnum,gcp,addr/tasknum*i,addr/tasknum*(i+1)-1);
        printf("wp: %d, wn : %d, rp : %d, rn : %d, gcp : %d wu: %f, ru: %f, gcu : %f\n",
            wp,wnum,rp,rnum,gcp,util_task[0],util_task[1],__calc_gcu(&(tasks[i]),MINRC,0,0,0));
    }
    float checker = 0.0;
    for(int i=0;i<tasknum;i++){
        checker += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        checker += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
        checker += __calc_gcu(&(tasks[i]),MINRC,0,0,0);
    }
    checker += (float)STARTE/(float)_find_min_period(tasks,tasknum);
    printf("checker : %f\n",checker);
    *result_util = checker;
    sleep(1);
    return tasks;
}

void get_task_from_file(rttask* tasks, int tasknum, FILE* taskfile){
    rttask* rand_tasks = tasks;
    for(int i=0;i<tasknum;i++){
        int wn, wp, rn, rp, gcp, lb, ub;
        fscanf(taskfile,"%d,%d,%d,%d,%d,%d,%d\n",&wn,&wp,&rn,&rp,&gcp,&lb,&ub);
        printf("reading %d,%d,%d,%d,%d,%d,%d\n",wn,wp,rn,rp,gcp,lb,ub);
        rand_tasks[i].idx = i;
        rand_tasks[i].wn = wn;
        rand_tasks[i].wp = wp;
        rand_tasks[i].rn = rn;
        rand_tasks[i].rp = rp;
        rand_tasks[i].gcp = gcp;
        rand_tasks[i].addr_lb = lb;
        rand_tasks[i].addr_ub = ub;
    }
}

float calcutil_per_pecycle(rttask* task,int tasknum, int cycle){
    float res = 0.0;
    for(int i=0;i<tasknum;i++){
        res += __calc_wu(&(task[i]),cycle);
        res += __calc_ru(&(task[i]),cycle);
        res += __calc_gcu(&(task[i]),MINRC,cycle,cycle,cycle);
    }
    res += e_exec(cycle)/(float)_find_min_period(task,tasknum);
    return res;
}
void randtask_statechecker(int tasknum,int addr){
    float startutil = 0.05;
    int ratios[10][11];
    //init result array
    for(int i=0;i<10;i++){
        for(int j=0;j<11;j++){
            ratios[i][j] = 0;
        }
    }
    for(int i=0;i<10;i++){//for util ranging from 0.05 ~ 0.5
        for(int j=0;j<1000;j++){//generate 1000 tasks and check success or not
            float res;
            float cur_util = startutil*(i+1);
            //generate taskset && save initial util.
            rttask* zstate_task = generate_taskset(tasknum,cur_util,addr,&res,0);
            for(int k=0;k<11;k++){
                res = calcutil_per_pecycle(zstate_task,tasknum,10*k);
                if(res <= 1.0){
                    ratios[i][k]++;
                }
            }   
        }
    }
    
    for(int i=0;i<11;i++){
        printf("ratios[CYC:%d] :",10*i);
        for(int j=0;j<10;j++){
            printf("%f, ",(float)ratios[j][i]/10.0);
        }
        printf("\n");
    }
    FILE* fp = fopen("utilpercycle.csv","w");
    for(int i=0;i<11;i++){
        for(int j=0;j<10;j++){
            fprintf(fp,"%f,",(float)ratios[j][i]/10.0);
        }
        fprintf(fp,"\n");
    }
    fflush(fp);
    fclose(fp);
}