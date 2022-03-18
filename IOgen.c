#include "stateaware.h"

void IOgen(int tasknum, rttask* tasks,int runtime, int offset){
    //generates a set of target address for write or read workload.
    //Address is generated according to the task parameter and target runtime.
    FILE* write_fp = fopen("workload_w.csv","w");
    FILE* read_fp = fopen("workload_r.csv", "w");
    int logispace = (int)(NOP*(1-OP));
    int hotspace = (int)(logispace*SPLOCAL);
    int w_size, r_size;
    w_size = 0;
    r_size = 0;
    for(int i=0;i<tasknum;i++){
        w_size += runtime/tasks[i].wp * tasks[i].wn;
        r_size += runtime/tasks[i].rp * tasks[i].rn;
    }
    int total_w = w_size;
    int total_r = r_size;
    int lpa;
    float hot;
    int gen_check_write[NOB];
    int gen_check_read[NOB];
    for(int i=0;i<NOB;i++){
        gen_check_write[i] = 0;
        gen_check_read[i] = 0;
    }
    for(int i=0;i<total_w;i++){
        hot = (float)(rand()%100/100.0);
        if (hot < TPLOCAL){
            lpa = rand()%hotspace;
        }
        else{
            lpa = rand()%(logispace-hotspace)+hotspace;
        }
        gen_check_write[lpa/NOB]++;
        fprintf(write_fp,"%d,",lpa);
    }
    for(int i=0;i<total_r;i++){
        hot = (float)(rand()%100/100.0);
        if (hot < TPLOCAL){
            lpa = rand()%hotspace+offset;//offset can be given
        }
        else {
            lpa = rand()%(logispace);
            while(lpa <= hotspace+offset && lpa >= offset){
                lpa = rand()%(logispace);
            }
        }
        gen_check_read[lpa/NOB]++;
        fprintf(read_fp,"%d,",lpa);
    }
    fflush(write_fp);
    fflush(read_fp);
    fclose(write_fp);
    fclose(read_fp);
    printf("write workload res:");
    for(int i=0;i<NOB;i++){
        printf("%d ",gen_check_write[i]);
    }
    printf("\n");
    printf("read workload res:");
    for(int i=0;i<NOB;i++){
        printf("%d ",gen_check_read[i]);
    }
    printf("\n");
}

int IOget(FILE* fp){
    int ret;
    fscanf(fp,"%d,",&ret);
    printf("[IOget]%d\n",ret);
    return ret;
}