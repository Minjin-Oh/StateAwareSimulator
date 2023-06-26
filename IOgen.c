#include "IOgen.h"

extern long* lpa_update_timing[NOP];
extern int update_cnt[NOP];

int gen_lpa_rand(int hotspace, int lb, int space, float tplocal){
    int lpa;
    float hot = (float)(rand()%100/100.0);
    if (hot<tplocal){
        lpa = rand()%hotspace + lb;
    }
    else{
        lpa = rand()%(space-hotspace) + hotspace + lb;
    }
    return lpa;
}

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
    int temp_write[100];
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
    int reqsize = 0;
    for(int i=0;i<w_size;i++){
        //generate lpa. make sure that lpa does not overlap in single job
        lpa = gen_lpa_rand(hotspace,lb,space,_tplocal);
        for(int a=0;a<reqsize;a++){
            if(temp_write[a] == lpa){
                while(temp_write[a] == lpa){
                    lpa = gen_lpa_rand(hotspace,lb,space,_tplocal);
                }
            }
        }
        temp_write[reqsize] = lpa;
        reqsize++;
        if(reqsize == task->wn){
            for(int a=0;a<reqsize;a++){
                fprintf(w_fp,"%d,",temp_write[a]);
                temp_write[a] = -1;
            }
            reqsize = 0;
        }
    }
    for(int i=0;i<r_size;i++){
        hot = (float)(rand()%100/100.0);
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

void gen_loc(rttask* task, int rank, float* _sploc, float* _tploc, int* offset){
    //generate locality parameter for task.
    //rank ranges from 0 to 3.
    //if rank is near 0, task has low tploc + high sploc.
    //if rank is near 3, task has high tploc + low sploc.
    int rand_param;
    if(rank == 0){
        rand_param = rand()%15;
        *_tploc = (float)0.85 + (float)rand_param / (float)100;
        *_sploc = 1.0 - *_tploc;
    }
    else if (rank == 1){
        rand_param = rand()%10;
        *_tploc = (float)0.75 + (float)rand_param / (float)100;
        *_sploc = 1.0 - *_tploc;
    }
    else if (rank == 2){
        rand_param = rand()%10;
        *_tploc = (float)0.65 + (float)rand_param / (float)100;
        *_sploc = 1.0 - *_tploc;
    }
    else if (rank == 3){
        rand_param = rand()%15;
        *_tploc = (float)0.50 + (float)rand_param / (float)100;
        *_sploc = 1.0 - *_tploc;
    }
    *offset = (int)((float)(task->addr_ub - task->addr_lb)* (*_sploc)/2.0);
}
void IOgen(int tasknum, rttask* tasks,long runtime, int offset, float _splocal, float _tplocal){
    //generates a set of target address for write or read workload.
    //Address is generated according to the task parameter and target runtime.
    //FIXME::current IOgen uses hardcoded address area
    //FIXME::current IOgen_new uses hardcoded hot space specification value
    float temp_sploc;
    float temp_tploc;
    int temp_offset;
    FILE* locfile;
    //if locality is specified over 0.0 , all tasks uniformly follow it.
    if (_splocal > 0.0 && _tplocal > 0.0){
        locfile = fopen("loc.csv","w");
        for(int i=0;i<tasknum;i++){
            IOgen_task(&(tasks[i]),runtime,offset,_splocal,_tplocal);
            //IOgen_task_new(&(tasks[i]),runtime,-1,PPB*2,offset,_splocal,_tplocal);
            tasks[i].sploc = _splocal;
            tasks[i].tploc = _tplocal;
            fprintf(locfile,"%f, %f\n",_splocal,_tplocal);
        }
        fclose(locfile);
    } 
    //if locality is specified as -1, randomly generate locality in descending order.
    else if (_splocal == -1.0 || _tplocal == -1.0){
        locfile = fopen("loc.csv","w");
        for (int i=0;i<tasknum;i++){
            //randomly generate locality for each task
            gen_loc(&(tasks[i]),i%4,&temp_sploc,&temp_tploc,&temp_offset);
            //assign offset to each task
            printf("[task %d]sploc %f,tploc %f,offset %d\n",i,temp_sploc,temp_tploc,temp_offset);
            IOgen_task(&(tasks[i]),runtime,temp_offset,temp_sploc,temp_tploc);
            tasks[i].sploc = temp_sploc;
            tasks[i].tploc = temp_tploc;
            fprintf(locfile,"%f, %f\n",temp_sploc,temp_tploc);
        }
        fclose(locfile);
    }
    //if locality is specified as -2, assign pre-recorded locality using input loc.csv files.
    else if (_splocal == -2.0 || _tplocal == -2.0){
	locfile = fopen("loc.csv","r");
        for (int i=0;i<tasknum;i++){
	    fscanf(locfile,"%f, %f\n",&temp_sploc,&temp_tploc);
	    temp_offset = (int)((float)(tasks[i].addr_ub - tasks[i].addr_lb)* (temp_sploc)/2.0);
	    printf("[task %d]sploc %f,tploc %f,offset %d\n",i,temp_sploc,temp_tploc,temp_offset);
	    tasks[i].sploc = temp_sploc;
	    tasks[i].tploc = temp_tploc;
	    IOgen_task(&(tasks[i]),runtime,temp_offset,temp_sploc,temp_tploc);
	}
	fclose(locfile);
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
    }*/
    
    
}

void IO_open(int tasknum, FILE** wfpp, FILE** rfpp){
    for(int i=0;i<tasknum;i++){
        char name[10];
        char name2[10];
        sprintf(name,"wr_t%d.csv",i);
        sprintf(name2,"rd_t%d.csv",i);
        wfpp[i] = fopen(name,"r");
        rfpp[i] = fopen(name2,"r");
        printf("opening %s and %s\n",name,name2);
        if(wfpp[i] == NULL){
            printf("file pointer of write workload is missing\n");
            abort();
        }
        if(rfpp[i] == NULL){
            printf("file pointer of read workload is missing\n");
            abort();
        }
        
    }
    sleep(3);
}

void IO_close(int tasknum, FILE** wfpp, FILE** rfpp){
    for(int i=0;i<tasknum;i++){
        fclose(wfpp[i]);
        fclose(rfpp[i]);
        printf("closing wr_t%d and rd_t%d\n",i,i);
    }
}

void reset_IO_update(meta* metadata, int lpa_lb, int lpa_ub, long IO_offset){
    //a function which resets update time of given LPA range
    //called for first job after I/O rewind
    //according to the reset time, re-calculate the next update time of designated task.
    char name[30];
    FILE* timing_fp;
    long scan_ret;
    long timing_ret;
#ifdef TIMING_ON_MEM
    for(int i=lpa_lb; i<lpa_ub; i++){
        if(update_cnt[i] >= 1){
            metadata->next_update[i] = lpa_update_timing[i][0] + IO_offset;
        } 
        else {
            metadata->next_update[i] = WORKLOAD_LENGTH + IO_offset;
        }
        metadata->write_cnt_per_cycle[i] = 0;
    }
#endif
#ifndef TIMING_ON_MEM
    for(int i=lpa_lb; i<lpa_ub;i++){
        sprintf(name,"./timing/%d.csv",i);
        timing_fp = fopen(name,"r");
        if(timing_fp != NULL){
            scan_ret = fscanf(timing_fp,"%ld,",&timing_ret);
            metadata->next_update[i] = timing_ret + IO_offset;
        } 
        else if (timing_fp == NULL){
            metadata->next_update[i] = WORKLOAD_LENGTH + IO_offset;
        }
        if(timing_fp != NULL){
            fclose(timing_fp);
        }
    }
#endif
}

void IO_timing_update(meta* metadata, int lpa, int wcount,long offset){
    char name[30];
    FILE* timing_fp;
    long scan_ret;
    long timing_ret;
#ifdef TIMING_ON_MEM
    if(wcount >= 0 && wcount < update_cnt[lpa]){
        metadata->next_update[lpa] = lpa_update_timing[lpa][wcount]+offset;
    } else if (wcount == update_cnt[lpa]){//no more update
        metadata->next_update[lpa] = offset + WORKLOAD_LENGTH;
    } else if (update_cnt[lpa] == 0){//never updated 
        metadata->next_update[lpa] = offset + WORKLOAD_LENGTH;
    }
#endif
#ifndef TIMING_ON_MEM
    sprintf(name,"./timing/%d.csv",lpa);
    timing_fp = fopen(name,"r");
    if(timing_fp != NULL){
        for(int i=0;i<wcount+1;i++){
            scan_ret = fscanf(timing_fp,"%ld,",&timing_ret);
        }
        if(scan_ret != EOF){
                metadata->next_update[lpa] = timing_ret+offset;
                printf("lpa %d update timing set to %ld\n",lpa,metadata->next_update[lpa]);
        } else {
            metadata->next_update[lpa] = offset + WORKLOAD_LENGTH;
            printf("[NOUPDATE]lpa %d update timing set to %ld\n",lpa,metadata->next_update[lpa]);
        }
    } else if (timing_fp == NULL){
        metadata->next_update[lpa] = offset + WORKLOAD_LENGTH;
        printf("[NOFILE]lpa %d update timing set to %ld\n",lpa,metadata->next_update[lpa]);
    }
    if(timing_fp != NULL){
        fclose(timing_fp);
    }
#endif
    //printf("%d next_update_time : %ld, offset : %ld\n",lpa,metadata->next_update[lpa],offset);
    
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
