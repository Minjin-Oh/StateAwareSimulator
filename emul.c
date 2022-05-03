#include "stateaware.h"

void emul_main(rttask* tasks, int tasknum, IO* wqueue, IO* rqueue, IO* gcqueue, IO* rrqueue, long runtime){
    long cur_cp = 0;
    IOhead* wq[tasknum];
    IOhead* rq[tasknum];
    IOhead* gcq[tasknum];
    IOhead* rr;
    IO* cur_IO = NULL;
    long cur_IO_fin = 0;
    while(cur_cp != runtime){
        //cur cp is wheather IO release time or req completion time(=req selec time)
        //if cur_cp is release time, call start job function
        //remember to generate I/O req on IOhead 
        for(int j=0;j<tasknum;j++){
            
            if(cur_cp % (long)tasks[j].wp==0){
                write_job_start();
            }
            if(cur_cp % (long)tasks[j].rp==0){
                read_job_start();
            }
            if(cur_cp % (long)tasks[j].gcp==0){
                gc_job_start();   
            }
        }
        //if cur_cp == IO termination time, finish IO & update metadata.
        if(cur_IO == cur_IO_fin){
            fin_req(cur_IO);
            cur_IO == NULL;
        }
        if(cur_IO == NULL){
            //init cur_dl
            long cur_dl = runtime + runtime/2L; //set over the maximum
            int targ_task = -1;
            int targ_type = -1;
            
            //find a request from I/Oqueue and pop it
            //!!be careful about the case when queue is empty
            for(int j=0;j<tasknum;j++){
                if(wq[j]->head->deadline < cur_dl){
                    targ_task=j;
                    targ_type=0;
                } else if(rq[j]->head->deadline < cur_dl){
                    targ_task=j;
                    targ_type=1;
                } else if(gcq[j]->head->deadline < cur_dl){
                    targ_task=j;
                    targ_type=2;
                }
            }
            //pop IO (sched IO by pointing cur_IO)
            //!be careful about the case when queue is empty
            if(targ_type == 0){
                cur_IO = ll_pop_IO(wq[targ_task]);
                cur_IO_fin = cur_IO->deadline;
            } else if (targ_type == 1){
                cur_IO = ll_pop_IO(rq[targ_task]);
                cur_IO_fin = cur_IO->deadline;
            } else if (targ_type == 2){
                cur_IO = ll_pop_IO(gcq[targ_task]);
                cur_IO_fin = cur_IO->deadline;
            } else if (targ_task == -1 && targ_type == -1) {
                cur_IO = ll_pop_IO(rr);
                cur_IO_fin = cur_IO->deadline;
            }
        }//find, pop & sched IO

        //go to next checkpoint. it is either request end time or job release time
        
    }
}