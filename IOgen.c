#include "IOgen.h"

void IOgen_task(rttask* task, long runtime, int offset, float _splocal, float _tplocal){
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
    hotspace = (int)((hb - lb)*_splocal);
    w_size = (int)(runtime / (long)task->wp * (long)task->wn);
    r_size = (int)(runtime / (long)task->rp * (long)task->rn);
    for(int i=0;i<w_size;i++){
        hot = (float)(rand()%100/100.0);
        if (hot<_tplocal){
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
void IOgen(int tasknum, rttask* tasks,long runtime, int offset, float _splocal, float _tplocal){
    //generates a set of target address for write or read workload.
    //Address is generated according to the task parameter and target runtime.
    //FIXME::current IOgen uses hardcoded address area
    for(int i=0;i<tasknum;i++){
        IOgen_task(&(tasks[i]),runtime,offset,_splocal,_tplocal);
    }
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

void lat_open(int tasknum, FILE** wlpp, FILE** rlpp){
    for(int i=0;i<tasknum;i++){
        char name[20];
        sprintf(name,"lat_w%d.csv",i);
        wlpp[i] = fopen(name,"w");
    }
    for(int i=0;i<tasknum;i++){
        char name2[20];
        sprintf(name2,"lat_r%d.csv",i);
        rlpp[i] = fopen(name2,"w");
    }
}

void lat_close(int tasknum, FILE** wlpp, FILE** rlpp){
    for(int i=0;i<tasknum;i++){
        fflush(wlpp[i]);
        fflush(rlpp[i]);
        fclose(wlpp[i]);
        fclose(rlpp[i]);
    }
}