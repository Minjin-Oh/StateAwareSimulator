#include "stateaware.h"

int compare(const void *a, const void *b){
    int num1= *(int*)a;
    int num2= *(int*)b;

    if(num1<num2)
        return -1;
    if(num1>num2)
        return 1;
    return 0;
}

int* add_checkpoints(int tasknum, rttask* tasks, int runtime, int* cps_size){
    int size = 0; // find the size of checkpoint list
    for(int i=0;i<tasknum;i++){
        size += runtime/tasks[i].wp;
        size += runtime/tasks[i].rp;
        size += runtime/tasks[i].gcp;
        printf("added %d %d %d\n",runtime/tasks[i].wp,runtime/tasks[i].rp,runtime/tasks[i].gcp);
    }
    int* cps = (int*)malloc(sizeof(int)*size);
    int cnt = 0;
    for(int i=0;i<tasknum;i++){
        for(int j=0;j<3;j++){
            int mult = 1;
            int period = -1;
            if(j==0) period = tasks[i].wp;
            if(j==1) period = tasks[i].rp;
            if(j==2) period = tasks[i].gcp;
            while(mult*period <= runtime){
                cps[cnt] = mult*period;
                cnt++;
                mult++;
            }
        }
    }
    printf("size %d, cnt %d\n",size,cnt);
    qsort(cps,size,sizeof(int),compare);
    *cps_size = size;
    return cps;
}


