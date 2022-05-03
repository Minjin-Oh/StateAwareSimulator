#include "stateaware.h"

int compare(const void *a, const void *b){
    long num1= *(long*)a;
    long num2= *(long*)b;

    if(num1<num2)
        return -1;
    if(num1>num2)
        return 1;
    return 0;
}

int* add_checkpoints(int tasknum, rttask* tasks, long runtime, int* cps_size){
    long size = 0; // find the size of checkpoint list
    printf("runtime : %ld\n",runtime);
    for(int i=0;i<tasknum;i++){
        size += runtime/(long)tasks[i].wp + 1;
        size += runtime/(long)tasks[i].rp + 1;
        size += runtime/(long)tasks[i].gcp + 1;
        printf("periods : %d %d %d\n",tasks[i].wp,tasks[i].rp,tasks[i].gcp);
        printf("added %d %d %d\n",runtime/tasks[i].wp,runtime/tasks[i].rp,runtime/tasks[i].gcp);
    }
    size += runtime/(long)100000 + 1;
    printf("size is %ld\n",size);
    long* cps = (long*)malloc(sizeof(long)*size);
    long cnt = 0;
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<3;j++){
            int mult = 0;
            int period = -1;
            if(j==0) period = tasks[i].wp;
            if(j==1) period = tasks[i].rp;
            if(j==2) period = tasks[i].gcp;
            while((long)mult*(long)period <= runtime){
                cps[cnt] = (long)mult*(long)period;
                cnt++;
                mult++;
            }
        }
    }
    //add another timestamp for RR scheduling.
    int RRmult = 0;
    int RRperiod = 100000;//set default RR period as multiple of 100ms.
    while((long)RRmult*(long)RRperiod <= runtime){
        cps[cnt] = (long)RRmult*(long)RRperiod;
        cnt++;
        RRmult++;
    }
    printf("size %ld, cnt %ld\n",size,cnt);
    qsort(cps,size,sizeof(long),compare);
    *cps_size = (int)size;
    for(int i=0;i<300;i++){
        printf("%ld, ",cps[i]);
    }
    sleep(1);
    return cps;
}


