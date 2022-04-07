#include "stateaware.h"
#include <stdio.h>
#include <stdlib.h>
#include <glpk.h>

int find_writectrl_lp(rttask* tasks, int tasknum, meta* metadata, double margin,int low, int high){
    //1. gets taskset and current state as a variable
    //2. returns a possible limit of write block
    glp_prob *lp;
    int yng, old;
    double row_ub;
    double obj_writesum;
    double row_writecoeff;
    int row_index[1+tasknum];
    int col_index[1+tasknum];
    double value[1+tasknum];
    double res[tasknum];
    lp = glp_create_prob();
    glp_set_prob_name(lp,"writealloc");
    glp_set_obj_dir(lp,GLP_MIN);
    //optimization direction flag = minimize
    
    //initialization
    yng = low;
    old = high;
    row_writecoeff = (double)(ENDW-STARTW)/(double)MAXPE;
    obj_writesum = 0.0;
    row_ub  = margin;
    for(int i=0;i<tasknum;i++){
        row_ub -= (double)tasks[i].wn/(double)tasks[i].wp*(double)STARTW;
        obj_writesum += (double)tasks[i].wn/(double)tasks[i].wp;
        res[i] = 0.0;
        printf("write util : %lf, %lf\n",(double)tasks[i].wn*500.0/(double)tasks[i].wp,
                                         (double)tasks[i].wn*350.0/(double)tasks[i].wp);

    }

    //ROWS. -inf < utilization bound <= margin - (nw1/Tw1 + ... + nwn/Twn)*b
    glp_add_rows(lp,1); //single row for utilization constraint
    glp_set_row_name(lp,1,"utilization bound");
    glp_set_row_bnds(lp,1,GLP_UP,0.0,row_ub);

    //COLUMNS. obj coeff = (nw/Tw)/sum(nw/Tw)
    //column and row starts with index 1, but task starts with index 0
    glp_add_cols(lp,tasknum);
    for(int i=1;i<tasknum+1;i++){
        char name[5];
        sprintf(name,"x%d",i);
        glp_set_col_name(lp,i,name);   
        glp_set_col_bnds(lp,i,GLP_DB,(double)yng,(double)old);
        glp_set_obj_coef(lp,i,((double)tasks[i-1].wn/(double)tasks[i-1].wp)/obj_writesum);
    }

    //add coefficients for constraints. row coeff = (nw/Tw)*a
    for(int i=1;i<tasknum+1;i++){
        row_index[i] = 1, col_index[i] = i, value[i] = (double)tasks[i-1].wn/(double)tasks[i-1].wp*row_writecoeff;
    }

    //load matrix
    glp_load_matrix(lp,tasknum,row_index,col_index,value);
    glp_simplex(lp,NULL);

    printf("lpsolve res: ");
    for(int i=0;i<tasknum;i++){
        res[i] = glp_get_col_prim(lp,i+1);
        printf("%d(%lf) ",(int)res[i],res[i]);
    }
    printf("\n");
    
}