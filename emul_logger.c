#include "emul_logger.h"
#include "IOgen.h"

int check_latency(FILE** lat_log_w, FILE** lat_log_r, FILE** lat_log_gc, IO* cur_IO, long cur_cp){
    //if start_or_end == 0, it means an I/O start flag
    //if start_or_end == 1, it means an I/O end flag
    
    //do not check latency of IO if cur_IO is empty || not  last req.
    if(cur_IO == NULL){
        return 1;
    } 
    else if(cur_IO->type == RD){
        fprintf(lat_log_r[cur_IO->taskidx],"%ld, %ld\n",cur_cp, cur_cp - cur_IO->IO_start_time);
    } 
    else if (cur_IO->type == WR){
        fprintf(lat_log_w[cur_IO->taskidx],"%ld, %ld\n",cur_cp, cur_cp - cur_IO->IO_start_time);
    }
    else if (cur_IO->type == GCER){
        fprintf(lat_log_gc[cur_IO->taskidx], "%ld, %ld\n",cur_cp, cur_cp - cur_IO->IO_start_time);
    }
    return 0;
}

int check_dl_violation(rttask* tasks, IO* cur_IO, long cur_cp){
    if(cur_IO == NULL){
        return 0;
    }
    else if(cur_IO->type == RD){
        if(cur_cp - cur_IO->IO_start_time > tasks[cur_IO->taskidx].rp){
            printf("[%ld]task %d read dl violation. dl : %ld, response : %ld\n",cur_cp, cur_IO->taskidx, tasks[cur_IO->taskidx].rp,cur_cp - cur_IO->IO_start_time);
            return 1;
        }
    }
    else if(cur_IO->type == WR){
        if(cur_cp - cur_IO->IO_start_time > tasks[cur_IO->taskidx].wp){
            printf("[%ld]task %d write dl violation. dl : %ld, response : %ld\n",cur_cp, cur_IO->taskidx, tasks[cur_IO->taskidx].wp,cur_cp - cur_IO->IO_start_time);
            return 1;
        }
    }
    else{
        return 0;
    }
}
