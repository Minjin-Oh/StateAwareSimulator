#include "stateaware.h"
void IOgen_task(rttask* task, int runtime, int offset){
    //generate workload with given restrictions.
    char name[30];
    char name2[30];
    int space, hotspace, w_size, r_size;
    int gen_check_write, gen_check_read;
    int lpa;
    float hot;
    int lb = task->addr_lb;
    int hb = task->addr_ub;

    //open csv file
    sprintf(name,"wr_t%d.csv",task->idx);
    sprintf(name2,"rd_t%d.csv",task->idx);
    FILE* w_fp = fopen(name,"w");
    FILE* r_fp = fopen(name2,"w");
    
    //generate logical addresses
    space = hb - lb;
    hotspace = (int)((hb - lb)*SPLOCAL);
    w_size = runtime / task->wp * task->wn;
    r_size = runtime / task->rp * task->rn;
    for(int i=0;i<w_size;i++){
        hot = (float)(rand()%100/100.0);
        if (hot<TPLOCAL){
            lpa = rand()%hotspace + lb;
        }
        else{
            lpa = rand()%(space-hotspace) + hotspace + lb;
        }
        fprintf(w_fp,"%d,",lpa);
    }
    for(int i=0;i<r_size;i++){
        if (hot < TPLOCAL){
            lpa = rand()%hotspace+offset+lb;//offset can be given
        }
        else {
            lpa = (rand()%space) + lb;
            while(lpa <= hotspace+offset+lb && lpa >= offset+lb){
                lpa = (rand()%space) + lb;
            }
        }
        fprintf(r_fp,"%d,",lpa);
    }

    //flush and close file
    fflush(w_fp);
    fflush(r_fp);
    fclose(w_fp);
    fclose(r_fp);
}

int IOget(FILE* fp){
    int ret;
    fscanf(fp,"%d,",&ret);
    return ret;
}
void IOgen(int tasknum, rttask* tasks,int runtime, int offset){
    //generates a set of target address for write or read workload.
    //Address is generated according to the task parameter and target runtime.
    //FIXME::current IOgen uses hardcoded address area
    for(int i=0;i<tasknum;i++){
        IOgen_task(&(tasks[i]),runtime,offset);
    }
    
    /* //old workload generating logic
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
    */
}

void IO_open(int tasknum, FILE** wfpp, FILE** rfpp){
    for(int i=0;i<tasknum;i++){
        char name[10];
        char name2[10];
        sprintf(name,"wr_t%d.csv",i);
        sprintf(name2,"rd_t%d.csv",i);
        wfpp[i] = fopen(name,"r");
        rfpp[i] = fopen(name2,"r");
    }
}