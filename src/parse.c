#include "stateaware.h"

void set_scheme_flags(char* argv[], 
                      int *gcflag, int *wflag, int *rrflag, int *rrcond){
    //gcflag
    if(strcmp(argv[1],"DOGC")==0){
        *gcflag = 1;
    } else if (strcmp(argv[1],"GREEDYGC")==0){
        *gcflag = 2;
    } else if (strcmp(argv[1],"LIMITGC")==0){
        *gcflag = 3;
    } else if (strcmp(argv[1],"YOUNGGC")==0){
        *gcflag = 4;
    } else if (strcmp(argv[1],"WEIGHTGC")==0){
        *gcflag = 5;
    } else if (strcmp(argv[1],"UTILGC")==0){
        *gcflag = 6;
    } else {
        *gcflag = 0;
    }

    //wflag
    if(strcmp(argv[2],"DOW")==0){
        *wflag = 1;
    } else if (strcmp(argv[2],"GREEDYW")==0){
        *wflag = 2;
    } else if (strcmp(argv[2],"LIMITW")==0){
        *wflag = 3;
    } else if (strcmp(argv[2],"WEIGHTW")==0){
        *wflag = 4;
    } else if (strcmp(argv[2],"FIXW")==0){
        *wflag = 5;
    } else if (strcmp(argv[2],"HOTW")==0){
        *wflag = 6;
    } else if (strcmp(argv[2],"OLDW")==0){
        *wflag = 7;
    } else if (strcmp(argv[2],"MOTIVWY")==0){
        *wflag = 8;
    } else if (strcmp(argv[2],"MOTIVWO")==0){
        *wflag = 9;
    } else if (strcmp(argv[2],"MOTIVALLO")==0){
        *wflag = 10;
    } else if (strcmp(argv[2],"MOTIVALLY")==0){
        *wflag = 11;
    } else if (strcmp(argv[2], "GRADW")==0){
        *wflag = 12;
    } else if (strcmp(argv[2], "GRADW_MOD")==0){
        *wflag = 13;
    } else if (strcmp(argv[2], "INVW")==0){
        *wflag = 14;
    }
    else {
        *wflag = 0;
    }

    //rrflag,rrcond
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
                    int *genflag, int* taskflag, int* profflag,
                    int *skewness, float* sploc, float* tploc, int* skewnum,
                    int *OPflag, int *cyc, double *OP, int *MINRC){
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
    11th: gets a initial cycle count
    below param is DEPRICATED
    12th: gets a OP value (0.0 ~ 1.0)
    13th: gets a MINRC value (0 ~ PPB)
    */
    if(strcmp(argv[4],"WORKGEN")==0){
        *genflag = 1;
    }
    if(strcmp(argv[4],"TASKGEN")==0){
        *taskflag = 1;
    }
    if(strcmp(argv[4],"PROFGEN")==0){
        *profflag = 1;
    }
    if(strcmp(argv[4],"SCATTERGEN")==0){
        *profflag = 2;
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
    if (argv[11] != NULL){
        *cyc = atoi(argv[11]);
    }
    else{
        *cyc = 0;
    }
    //!!!overprovisioning rate and minimum reclaimable page is hardcoded in set_exec_flags!!!
    *OP = 0.07;
    *MINRC = 7;
}
