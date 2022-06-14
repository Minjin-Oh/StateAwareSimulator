#include "stateaware.h"

void set_scheme_flags(char* argv[], 
                      int *gcflag, int *wflag, int *rrflag, int *rrcond){
    if(strcmp(argv[1],"DOGC")==0){
        *gcflag = 1;
    }
    if(strcmp(argv[2],"DOW")==0){
        *wflag = 1;
    } else if (strcmp(argv[2],"GREEDYW")==0){
        *wflag = 2;
    }
    if(strcmp(argv[3],"SKIPRR")==0){// do util-based relocation
        *rrflag = -1;
    } else if (strcmp(argv[3],"DORR")==0){//a case when read occurs in best
        *rrflag = 1;
        *rrcond = 0;
    } else if (strcmp(argv[3],"RR005")==0){
        *rrflag = 1;
        *rrcond = 1;
    } else if (strcmp(argv[3],"RR01")==0){
        *rrflag = 1;
        *rrcond = 2;
    } else if (strcmp(argv[3],"FRR")==0){
        *rrflag = 1;
        *rrcond = 3;
    } else if (strcmp(argv[3], "FRR01")==0){
        *rrflag = 1;
        *rrcond = 4;
    } else if (strcmp(argv[3], "FRR02")==0){
        *rrflag = 1;
        *rrcond = 5;
    } else if (strcmp(argv[3], "FRR03")==0){
        *rrflag = 1;
        *rrcond = 6;
    } else if (strcmp(argv[3],"BASE")==0){
        *rrflag = 0;
        *rrcond = 0;   
    } else if (strcmp(argv[3],"BASE005")==0){
        *rrflag = 0;
        *rrcond = 1;
    } else if (strcmp(argv[3],"BASE01")==0){
        *rrflag = 0;
        *rrcond = 2;
    } else if (strcmp(argv[3],"FBASE")==0){
        *rrflag = 0;
        *rrcond = 3;
    } else if (strcmp(argv[3], "FBASE01")==0){
        *rrflag = 0;
        *rrcond = 4;
    } else if (strcmp(argv[3], "FBASE02")==0){
        *rrflag = 0;
        *rrcond = 5;
    } else if (strcmp(argv[3], "FBASE03")==0){
        *rrflag = 0;
        *rrcond = 6;
    } 
    else if (strcmp(argv[3],"BESTR")==0){//always SKIP RR
        *rrflag = 2;
    }
}

void set_exec_flags(char* argv[], int *tasknum, float *totutil, 
                    int *genflag, int* taskflag,
                    int *skewness, float* sploc, float* tploc, int* skewnum,
                    int *OPflag, double *OP, int *MINRC){
    /* 
    interprets 4th ~ 11th argv
    4th : if WORKGEN, we generate workload pattern
          if TASKGEN, we generate randomnized taskset
    5th : gets a number of task in taskset
    6th : gets a value of total utilization of taskset (for TASKGEN)
    7th : gets a skewness factor. 0 = read-skew, 1 = write-skew, -1 = no skew (for TASKGEN)
    8th : gets a spatial locality of workload (for WORKGEN)
    9th : gets a temporal locality of workload (for WORKGEN)
    10th: gets a number of skewed taskset (use this only when 7th is not -1)
    11th: gets a OP change factor. 0 = use default OP (0.32), 1 = use custom OP
    12th: gets a OP value (0.0 ~ 1.0)
    13th: gets a MINRC value (0 ~ PPB)
    */
    if(strcmp(argv[4],"WORKGEN")==0){
        *genflag = 1;
    }
    if(strcmp(argv[4],"TASKGEN")==0){
        *taskflag = 1;
    }

    *tasknum = atoi(argv[5]);
    *totutil = atof(argv[6]);
    *skewness = atoi(argv[7]);
    *sploc = atof(argv[8]);
    *tploc = atof(argv[9]);
    *OPflag = 0;
    if(*skewness >= 0){
        *skewnum = atoi(argv[10]);
    }
    if(argv[11]!=NULL){
        *OPflag = atoi(argv[11]);
    }
    if(*OPflag == 1){
        *OP = atof(argv[12]);
        *MINRC = atoi(argv[13]);
    } else {
        *OP = 0.32;
        *MINRC = 35;
    }
}
