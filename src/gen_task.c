#include "stateaware.h"
#include "init.h"
extern int MINRC;

rttask* generate_taskset(int tasknum, float tot_util, int addr, float* result_util, int cycle){
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
#ifdef UUNIFAST
    double cur_util = (double)tot_util;
    double randfloat = (double)rand()/(double)RAND_MAX;
    for(int i=1;i<tasknum;i++){
        double nextsum = cur_util * pow(randfloat,1.0/(double)(tasknum-i));  // 현재 남아있는 utilization random ratio(pow(...))에 따라서 next total utilization 결정
		utils[i - 1] = (float)(cur_util - nextsum);  // 현재 남아있는 utilization에서 next total utilization을 빼서 i-1번째 task의 utilization 결정
        cur_util = nextsum;
    }
    utils[tasknum-1] = cur_util;  // 마지막 task utilization은 다른 task에 utilization을 할당하고 남은 utilization으로 결정
    //check code
    float test_util_tot=0.0;
    printf("::UUNIFAST GEN::\n");
    for(int i=0;i<tasknum;i++){
        printf("[%d]%f\n",i,utils[i]);
        test_util_tot += utils[i];
    }
    printf("[ALL]%f\n",test_util_tot);
#endif
#ifndef UUNIFAST
    util_ratio_sum = 0;
    for(int i=0;i<tasknum;i++){
        util_ratio[i] = rand()%10 + 1;
        util_ratio_sum += util_ratio[i];
    }//! generated portion

    //utilization ratio
    for(int i=0;i<tasknum;i++){
        utils[i] = ((float)util_ratio[i]/(float)util_ratio_sum) * tot_util;
    }
#endif
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
        //access number of page should be under 30pg --> considering a free page limit
        //randomly generate a flash page under 30pg, 50pg
        wnum = rand()%30 + 1;
        rnum = rand()%50 + 1;
        //generate a taskset parameter
        wp = (int)((float)(wnum*STARTW) / util_task[0]);
        rp = (int)((float)(rnum*STARTR) / util_task[1]);
        gcp = __calc_gcmult(wp,wnum,MINRC);
        init_task(&(tasks[i]),i,wp,wnum,rp,rnum,gcp,addr/tasknum*(i),addr/tasknum*(i+1)-1);
        printf("wp: %d, wn : %d, rp : %d, rn : %d, gcp : %d wu: %f, ru: %f, gcu : %f\n",
            wp,wnum,rp,rnum,gcp,util_task[0],util_task[1],__calc_gcu(&(tasks[i]),MINRC,0,0,0));
        
    }
	float utilsum = 0.0;  // total utilization (blocking factor + write/read utilization + gc utilization)
    float WRsum = 0.0;  // write, read utilization
    float utilsum_noblock = 0.0; // write, read, gc utilization
    float utilsum_nogc = 0.0;  // blocking factor
    for(int i=0;i<tasknum;i++){
        WRsum += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        WRsum += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
    }
    for(int i=0;i<tasknum;i++){
        utilsum += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        utilsum += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
        utilsum += __calc_gcu(&(tasks[i]),MINRC,0,0,0);
    }
    utilsum_noblock = utilsum;
    utilsum += (float)STARTE/(float)_find_min_period(tasks,tasknum);
    utilsum_nogc += (float)STARTE/(float)_find_min_period(tasks,tasknum);
    printf("WRsum : %f, utilsum(noblock): %f, utilsum(nogc): %f totutil : %f\n",WRsum,utilsum_noblock,utilsum_nogc,utilsum);
    *result_util = utilsum;
    return tasks;
}

rttask* generate_taskset_hardcode(int tasknum, int addr, float* result_util){
    //gets already generated taskparam.csv to "reuse" read/write task params.
    //reconfigures gcp, lb and ub according to current overprovisioning rate
    rttask* tasks;
    int wn, wp, rn, rp, gcp, lb, ub;
    FILE* taskfp = fopen("taskparam.csv","r");
    tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    for(int i=0;i<tasknum;i++){
        fscanf(taskfp,"%d,%d,%d,%d,%d,%d,%d\n",&wn,&wp,&rn,&rp,&gcp,&lb,&ub);
        //recalculate gcp using MINRC from emul_main.c
        gcp = __calc_gcmult(wp,wn,MINRC);
        //recalculate bounds of logical address using addr param
        init_task(&(tasks[i]),i,wp,wn,rp,rn,gcp,addr/tasknum*(i),addr/tasknum*(i+1)-1);
    }
    float checker = 0.0;
    for(int i=0;i<tasknum;i++){
        checker += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        checker += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
        checker += __calc_gcu(&(tasks[i]),MINRC,0,0,0);
    }
    checker += (float)STARTE/(float)_find_min_period(tasks,tasknum);
    printf("checker : %f\n", checker);
    *result_util = checker; 
    fclose(taskfp);
    return tasks;
}

rttask* generate_taskset_skew(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle){
    /*assign 90% of utilization to skewnum out of n tasks*/
    /*let skewnum out of n tasks to have utilization over threshold*/

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
        rnum = rand()%50 + 1;
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

rttask* generate_taskset_skew2(int tasknum, float tot_util, int addr, float* result_util, int skewnum, char type, int cycle){
    float utils[tasknum];
    float per_task_portion = 0.05;
    float offset = 0.045;
    float rand_portion = 0.005;
    float r_util[tasknum], w_util[tasknum];
    int wp, rp, gcp, wnum, rnum;
    rttask* tasks = (rttask*)malloc(sizeof(rttask)*tasknum);
    
    //generate util for each task
    for(int i=0;i<tasknum;i++){
        float rand_u = (float)(rand()%(int)(rand_portion*1000)) / 1000.0;
        if(i<skewnum){
            w_util[i] = rand_u + offset;
            r_util[i] = per_task_portion - w_util[i];
        } else {
            r_util[i] = rand_u + offset;
            w_util[i] = per_task_portion - r_util[i];
        }
    }

    //assign 90% of util for write if task is write-intensive
    for(int i=0;i<tasknum;i++){
        wnum = rand()%30 + 1;
        rnum = rand()%50 + 1;
        wp = (int)((float)(wnum*STARTW) / w_util[i]);
        rp = (int)((float)(rnum*STARTR) / r_util[i]);
        gcp = __calc_gcmult(wp,wnum,MINRC);
        init_task(&(tasks[i]),i,wp,wnum,rp,rnum,gcp,addr/tasknum*i,addr/tasknum*(i+1)-1);
        printf("wp: %d, wn : %d, rp : %d, rn : %d, gcp : %d wu: %f, ru: %f, gcu : %f\n",
            wp,wnum,rp,rnum,gcp,w_util[i],r_util[i],__calc_gcu(&(tasks[i]),MINRC,0,0,0));
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

rttask* generate_taskset_fixed(int addr, float* result_util){
    rttask* tasks;
    float w_util[3];
    float r_util[3];
    int wnum[3];
    int rnum[3];
    int wp[3];
    int rp[3];
    int gcp[3];
    w_util[0] = 0.1;
    r_util[0] = 0.15;
    w_util[1] = 0.01;
    r_util[1] = 0.01;
    w_util[2] = 0.01;
    r_util[2] = 0.01;
    tasks = (rttask*)malloc(sizeof(rttask)*3);
    for (int i=0;i<3;i++){
        wnum[i] = rand()%30 + 1;
        rnum[i] = rand()%50 + 1;
        wp[i] = (int)((float)(wnum[i]*STARTW) / w_util[i]);
        rp[i] = (int)((float)(rnum[i]*STARTR) / r_util[i]);
        gcp[i] = __calc_gcmult(wp[i],wnum[i],MINRC);   
    }
    init_task(&(tasks[0]),0,wp[0],wnum[0],rp[0],rnum[0],gcp[0],addr/6 * 0, addr/6 * 1);
    init_task(&(tasks[1]),1,wp[1],wnum[1],rp[1],rnum[1],gcp[1],addr/6 * 1, addr/6 * 2);
    init_task(&(tasks[2]),2,wp[2],wnum[2],rp[2],rnum[2],gcp[2],addr/6 * 2, addr);

    float checker = 0.0;
    for(int i=0;i<3;i++){
        checker += (float)tasks[i].wn*STARTW / (float)tasks[i].wp;
        checker += (float)tasks[i].rn*STARTR / (float)tasks[i].rp;
        checker += __calc_gcu(&(tasks[i]),MINRC,0,0,0);
        
    }
    checker += (float)STARTE/(float)_find_min_period(tasks,3);
    *result_util = checker;
    printf("tot_u : %f\n",checker);
    sleep(1);
    return tasks;

}
void get_task_from_file(rttask* tasks, int tasknum, FILE* taskfile){
    rttask* rand_tasks = tasks;
    int wn, wp, rn, rp, gcp, lb, ub;
    for(int i=0;i<tasknum;i++){
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

void get_loc_from_file(rttask* tasks, int tasknum, FILE* locfile){
    float __sploc, __tploc;
    for(int i=0;i<tasknum;i++){
        fscanf(locfile,"%f, %f\n",&__sploc,&__tploc);
        printf("locality specified, %f, %f\n",__sploc, __tploc);
        tasks[i].sploc = __sploc;
        tasks[i].tploc = __tploc;
        
    }
}
void get_task_from_file_recalc(rttask* tasks, int tasknum, FILE* taskfile, int max_valid_pg){
    //call this function instead of get_task_from_file, when OP changes.
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
        //recalculate GCperiod
        gcp = __calc_gcmult(wp,wn,MINRC);
        rand_tasks[i].gcp = gcp;
        lb = max_valid_pg / tasknum * i;
        ub = max_valid_pg / tasknum * (i+1)-1;
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
