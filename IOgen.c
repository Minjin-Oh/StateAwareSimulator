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
        if (hot < _tplocal){
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

void IOgen_task_new(rttask* task, long runtime, int w_area, int r_area, int offset, float _splocal, float _tplocal){
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
            if (w_area <= 0){
                lpa = rand()%hotspace + lb;
            } else {
                lpa = rand()%w_area + lb;
            }
        }
        else{
            if (w_area <= 0){
                lpa = rand()%(space-hotspace) + hotspace + lb;
            } else {
                lpa = rand()%(space-w_area) + w_area + lb;
            }
            
        }
        fprintf(w_fp,"%d,",lpa);
    }
    for(int i=0;i<r_size;i++){
        if (hot<_tplocal){
            if(r_area <= 0){
                lpa = rand()%hotspace+offset+lb;//offset can be given
            } else {
                lpa = rand()%r_area + offset + lb;
            }
        }
        else {
            if(r_area <= 0){
                lpa = rand()%(space-hotspace) + lb;
                if (lpa < offset+lb){
                    /*do nothing*/
                }
                else {
                    lpa += hotspace;
                }
            } else {
                lpa = rand()%(space - r_area) + lb;
                if(lpa < offset+lb){
                    /*do nothing*/
                } else {
                    lpa += r_area;
                }
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

void analyze_IO(FILE* fp){
    int ret;
    int check;
    int blk[NOB];
    for(int i=0;i<NOB;i++){
        blk[i] = 0;
    }
    while(1){
        check = fscanf(fp,"%d,",&ret);
        blk[ret/PPB] += 1;
        if(check == EOF){
            break;
        }
    }
    printf("[IO analyze]\n");
    for(int i=0;i<NOB;i++){
        printf("%d(%d) ",i,blk[i]);
    }
    printf("\n");
}

void IOgen(int tasknum, rttask* tasks,long runtime, int offset, float _splocal, float _tplocal){
    //generates a set of target address for write or read workload.
    //Address is generated according to the task parameter and target runtime.
    //FIXME::current IOgen uses hardcoded address area
    //FIXME::current IOgen_new uses hardcoded hot space specification value
    for(int i=0;i<tasknum;i++){
        IOgen_task(&(tasks[i]),runtime,offset,_splocal,_tplocal);
        //IOgen_task_new(&(tasks[i]),runtime,-1,PPB*2,offset,_splocal,_tplocal);
    }
    /*
    //IO analyze code::activate if want to print the raw profile of I/O concentration.
    char name[10];
    char name2[10];
    FILE* fp_w;
    FILE* fp_r;
    for(int i=0;i<tasknum;i++){
        sprintf(name,"wr_t%d.csv",i);
        sprintf(name2,"rd_t%d.csv",i);
        fp_w = fopen(name,"r");
        fp_r = fopen(name2,"r");
        analyze_IO(fp_w);
        analyze_IO(fp_r);
        fclose(fp_w);
        fclose(fp_r);
    }
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

void lat_open(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp){
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
    for(int i=0;i<tasknum;i++){
        char name3[20];
        sprintf(name3,"lat_gc%d.csv",i);
        gclpp[i] = fopen(name3,"w");
    }
}

void lat_close(int tasknum, FILE** wlpp, FILE** rlpp, FILE** gclpp){
    for(int i=0;i<tasknum;i++){
        fclose(wlpp[i]);
        fclose(rlpp[i]);
        fclose(gclpp[i]);
    }
}