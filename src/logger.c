#include "stateaware.h"
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

extern double OP;

// Helper function to create a directory if it doesn't exist
static void create_log_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#if defined(_WIN32)
        _mkdir(path);
#else
        mkdir(path, 0755); // Use 0755 for rwx for owner, and rx for others
#endif
    }
}

// Helper function to open a log file inside a specific directory
static FILE* open_log_file(const char* dir, const char* filename, const char* mode) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);
    FILE* fp = fopen(filepath, mode);
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to open file %s\n", filepath);
        perror("fopen");
    }
    return fp;
}

FILE* open_file_bycase(int gcflag, int wflag, int rrflag, const char* log_dir){
    FILE* fp;
    if(gcflag == 1 && wflag == 1 && rrflag == 1){
        fp = fopen("SA_prof_all.csv","w");
    }
    else if (gcflag == 0 && wflag == 1 && rrflag == 1){
        fp = fopen("SA_prof_case1.csv","w");
    }
    else if (gcflag == 1 && wflag == 0 && rrflag == 1){
        fp = fopen("SA_prof_case2.csv","w");
    }
    else if (gcflag == 1 && wflag == 1 && rrflag == 0){
        fp = fopen("SA_prof_case3.csv","w");
    }
    else if (gcflag == 0 && wflag == 0 && rrflag == 1){
        fp = fopen("SA_prof_case4.csv","w");
    }
    else if (gcflag == 0 && wflag == 1 && rrflag == 0){
        fp = fopen("SA_prof_case5.csv","w");
    }
    else if (gcflag == 1 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_case6.csv","w");
    }
    else if (gcflag == 0 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_none.csv","w");
    }
    else if (gcflag == 2 && wflag == 2 && rrflag == 0){
        fp = fopen("SA_prof_greedy.csv","w");
    }
    else if (gcflag == 3 && wflag == 0 && rrflag == 0){
        fp = fopen("SA_prof_gclimit.csv","w");
    }
    else {
        fp = fopen("SA_prof_motiv.csv","w");
    }
    return fp;
}

FILE* open_file_pertask(int gcflag, int wflag, int rrflag, int tasknum){
    //file per each task + 1 file for hot data distribution profiling
    FILE** fps;
    fps = (FILE**)malloc(sizeof(FILE*)*(tasknum+tasknum+2));
    char name[100];
    const char* log_dir = "logs";
    create_log_directory(log_dir);

    if(wflag == 0 && gcflag == 0 && rrflag == -1){ // noWL
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_no_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_no_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_no_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_no_invdist.csv","w");
    } 
    else if(wflag == 11 && gcflag == 0 && rrflag == -1){//dynWL
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_dynWL_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_dynWL_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_dynWL_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_dynWL_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 0 && rrflag ==  0){//staticWL
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_statWL_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_statWL_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_statWL_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_statWL_invdist.csv","w");
    }
    else if(wflag == 11 && gcflag == 0 && rrflag ==  0){//WLcomb
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_WLcomb_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_WLcomb_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_WLcomb_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_WLcomb_invdist.csv","w");
    }
    else if(wflag == 14 && gcflag == 6 && rrflag ==  1){//ours
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_ours_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_ours_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_ours_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_ours_invdist.csv","w");
    }
    else if(wflag == 14 && gcflag == 0 && rrflag == -1){//Wonly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wonly_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wonly_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_wonly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_wonly_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 6 && rrflag == -1){//GConly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gconly_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gconly_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_gconly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_gconly_invdist.csv","w");
    }
    else if(wflag == 0  && gcflag == 0 && rrflag ==  1){//RRonly
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_rronly_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_rronly_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_rronly_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_rronly_invdist.csv","w");
    }
    else if(wflag == 14 && gcflag == 6 && rrflag == -1){//W+GC
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wgc_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wgc_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_wgc_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_wgc_invdist.csv","w");
    }
    else if (rrflag == 1 && gcflag == 6 && wflag == 0){//GC+RR
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gcrr_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_gcrr_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_gcrr_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_gcrr_invdist.csv","w");
    } 
    else if (gcflag == 0 && wflag == 14 && rrflag == 1){//W+RR
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wrr_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_wrr_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_wrr_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_wrr_invdist.csv","w");
    }   
    else {
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_new_t%d.csv",i);
            fps[i] = open_log_file(log_dir, name, "w");
        }
        for(int i=0;i<tasknum;i++){
            sprintf(name,"prof_new_wlog%d.csv",i);
            fps[tasknum+i] = open_log_file(log_dir, name, "w");
        }
        fps[tasknum+tasknum] = open_log_file(log_dir, "prof_new_fbdist.csv","w");
        fps[tasknum+tasknum+1] = open_log_file(log_dir, "prof_new_invdist.csv","w");
    }
    return fps;
}

float print_profile(rttask* tasks, int tasknum, int taskidx, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp,int cur_gc_idx,int cur_gc_state, block* cur_wb, bhead* fblist_head, bhead*write_head, int getfp, int gcvalidcount){
    //init params
    int cur_read_worst[tasknum];
    int state_tot = 0;
    double state_avg = 0.0;
    double state_var = 0.0;
    float total_u = 0.0;
    float total_r = 0.0;
    float total_w = 0.0;
    float total_gc = 0.0;
    float total_u_noblock = 0.0;
    //find worst case util w.r.t system-wise worst block
    float worst_util = find_worst_util(tasks,tasknum,metadata);
    //find worst block for each task
    
    for(int k=0;k<tasknum;k++){
        cur_read_worst[k] = 0;
        for(int i=0;i<NOP;i++){
            if(metadata->vmap_task[i] == k){
                int cur_b = i/PPB;
                if(metadata->state[cur_b] >= cur_read_worst[k]){
                    cur_read_worst[k] = metadata->state[cur_b];
                }
            }
        }
        //printf("\n[%d]cur_read_worst : %d\n",k,cur_read_worst[k]);
    }
    //printf("system worst : %d\n",old);
    //find current util w.r.t actual blocks
    
    for(int j=0;j<tasknum;j++){//0 = write, 2 = GC
        total_u += metadata->runutils[0][j];
        total_w += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_r += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        total_gc += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    for(int i=0;i<NOB;i++){
        state_tot += metadata->state[i];
    }
    state_avg = (double)state_tot / (double)NOB;
    for(int i=0;i<NOB;i++){
        state_var += pow((double)metadata->state[i] - (double)state_avg ,2.0);
    }
    state_var = state_var / NOB;
    state_var = sqrt(state_var);
    total_u_noblock = total_u;
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //print all infos (prof_{no, WLcomb, ours}_t%d.csv)
    fprintf(fp,"%ld,%f,%f,%f,%f,%f, %d,%d, %f,%f,%f, %f, %lf\n",
    cur_cp,
    total_u, total_u_noblock,
    metadata->runutils[0][taskidx],
    metadata->runutils[1][taskidx],
    metadata->runutils[2][taskidx],
    metadata->state[cur_wb->idx], cur_gc_state,
    total_w,total_r,total_gc,
    state_avg, state_var); 
    return total_u;
}

// timestamp, tot_util, tot_util(no block), w_util, r_util, gc_util, w_blk_pe, gc_blk_pe, tot_w_util, tot_r_util, tot_w_util, avg_pe, var_pe

//misc function for motivation data
float print_profile_timestamp(rttask* tasks, int tasknum, meta* metadata, FILE* fp, 
                   int yng, int old,long cur_cp){
    //init params
    int cur_read_worst[tasknum];
    int state_tot = 0;
    double state_avg = 0.0;
    double state_var = 0.0;
    float total_u = 0.0;
    float total_r = 0.0;
    float total_w = 0.0;
    float total_gc = 0.0;
    float total_u_noblock = 0.0;
    //find worst case util w.r.t system-wise worst block
    
    for(int j=0;j<tasknum;j++){//0 = write, 1 = read, 2 = GC
        total_u += metadata->runutils[0][j];
        total_w += metadata->runutils[0][j];
        total_u += metadata->runutils[1][j];
        total_r += metadata->runutils[1][j];
        total_u += metadata->runutils[2][j];
        total_gc += metadata->runutils[2][j];
        //printf("%f, %f, %f, cur : %f\n",metadata->runutils[0][j],metadata->runutils[1][j],metadata->runutils[2][j],total_u);
    }
    total_u += (float)e_exec(old) / (float)_find_min_period(tasks,tasknum);
    //block state profiling
    for(int i=0;i<NOB;i++){
        state_tot += metadata->state[i];
    }
    state_avg = (double)state_tot / (double)NOB;
    for(int i=0;i<NOB;i++){
        state_var += pow((double)metadata->state[i] - (double)state_avg ,2.0);
    }
    state_var = state_var / NOB;
    state_var = sqrt(state_var);
    //print all infos (rrchecker.csv)
    fprintf(fp,"%ld,%f,%d,%d,%lf,%lf\n",
    cur_cp,total_u,old,yng,state_avg,state_var); 
    return total_u;
}

void print_profile_updaterate(meta* metadata, FILE* updaterate_fp){
    for (int i=0;i<NOP;i++){
        //printf("wtf? %ld %ld\n",metadata->avg_update[i],metadata->recent_update[i]);
        fprintf(updaterate_fp,"%ld, %ld\n",metadata->avg_update[i],metadata->recent_update[i]);
    }
}