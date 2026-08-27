#include "stateaware.h"

// Handled scheme flags (only those the current shell scripts actually pass):
//   gcflag: NO(0), UTILGC(6), RTGC(8)
//   wflag : NO(0), MOTIVALLY(11), INVW(14)
//   rrflag: SKIPRR(-1), BASE005(rrflag=0, rrcond=1), RR005(rrflag=1, rrcond=1),
//           NRTWL(rrflag=3, rrcond=0)  -- RTGC non-real-time wear leveler
void set_scheme_flags(char* argv[],
                      int *gcflag, int *wflag, int *rrflag, int *rrcond){
    // gcflag
    if(strcmp(argv[1],"UTILGC")==0){
        *gcflag = 6;
    } else if(strcmp(argv[1],"RTGC")==0){
        *gcflag = 8;
    } else {
        *gcflag = 0;
    }

    // wflag
    if(strcmp(argv[2],"MOTIVALLY")==0){
        *wflag = 11;
    } else if(strcmp(argv[2],"INVW")==0){
        *wflag = 14;
    } else {
        *wflag = 0;
    }

    // rrflag / rrcond
    if(strcmp(argv[3],"SKIPRR")==0){
        *rrflag = -1;
    } else if(strcmp(argv[3],"RR005")==0){
        *rrflag = 1;
        *rrcond = 1;
    } else if(strcmp(argv[3],"BASE005")==0){
        *rrflag = 0;
        *rrcond = 1;
    } else if(strcmp(argv[3],"NRTWL")==0){
        // RTGC non-real-time wear leveler (Chang et al. 2004, §3.4.2).
        // rrcond=0 keeps the LaWL do_rr/rr_finished/RR_job_start_q pipeline dormant;
        // rrflag==3 is handled explicitly by the RTGC-only WL path in emul_main.c.
        *rrflag = 3;
        *rrcond = 0;
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
        if (argv[10] == NULL){
            fprintf(stderr, "error: skewness=%d requires argv[10] (skewnum)\n", *skewness);
            exit(EXIT_FAILURE);
        }
        *skewnum = atoi(argv[10]);
    }
    if (argv[11] != NULL){
        *cyc = atoi(argv[11]);
    }
    else{
        *cyc = 0;
    }
    //!!!overprovisioning rate and minimum reclaimable page is hardcoded in set_exec_flags!!!
    *OP = 0.32;
    *MINRC = 35;
}
