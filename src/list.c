#include "stateaware.h"

bhead* ll_init(){
    //init and return head poiner
    bhead* head = NULL;
    head = (bhead*)malloc(sizeof(bhead));
    head->blocknum = 0;
    head->head = NULL;
    return head;
}

block* ll_append(bhead* head, block* new){
    //append block
    block* cur;
    //printf("appending %d\n",new->idx);
    if(head->head == NULL){
        head->head = new;
        new->prev = NULL;
        new->next = NULL;
    }
    else{
        cur = head->head;
        while(cur->next != NULL)
            cur = cur->next;
        cur->next = new;
        new->prev = cur;
        new->next = NULL;
    }
    head->blocknum++;

}

block* ll_pop(bhead* head){
    //pop block
    block* temp;
    temp = head->head;
    if(temp == NULL){
        printf("head is null\n");
        return NULL;
    }
    else{
        if(temp->next != NULL){
        head->head = temp->next;
        head->head->prev = NULL;           
        } else {
        head->head = NULL;
        }
    }
    head->blocknum--;
    temp->prev = NULL;
    temp->next = NULL;
    return temp;
}

block* ll_findidx(bhead* head, int tar){
    //find certain block
    block* cur = NULL;
    cur = head->head;
    
    while(cur != NULL){
        //printf("cur->idx:%d\n",cur->idx);
        if(cur->idx == tar){
            //printf("found %d\n",cur->idx);
            break;
        }    
        else{
            cur = cur->next;
        }
    }
    if(cur==NULL){
        printf("[findidx]cannot find target block\n");
    }
    return cur;
}

block* ll_remove(bhead* head, int tar){
    //remove certain block
    block* cur = NULL;
    cur = head->head;
    
    while(cur != NULL){
        //printf("[rmv traverse]cur->idx:%d\n",cur->idx);
        if(cur->idx == tar)
            break;
        else{
            cur = cur->next;
        }
    }
    //escape code
    if(cur == NULL){
        //printf("[RMV]there's no such block!\n");
        return NULL;
    }
    //conditions of target block
    if((cur->next != NULL) && (cur->prev != NULL)){//middle
        //printf("case1\n");
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
        //printf("[case1]%d to %d\n",cur->prev->idx,cur->next->idx);
    }
    else if (head->head == cur){//head
        //printf("case2\n");
        if(cur->next != NULL){//removing head & link next as head
            head->head = cur->next;
            head->head->prev = NULL;
            //printf("[case2]head is now %d\n",head->head->idx);
        } else {
            head->head = NULL;
        }
    }
    else if ((cur->next == NULL) && (cur->prev != NULL)){//tail
        //printf("case3\n");
        //printf("[case3]prev block : %d\n",cur->prev->idx);
        cur->prev->next = NULL;
    }
    head->blocknum--;
    //printf("bnum:%d\n",head->blocknum);
    if(head->blocknum <= -1) 
        abort();
    cur->prev = NULL;
    cur->next = NULL;
    return cur;
}

block* ll_find(meta* metadata, bhead* head, int cond){
    //find a block which meets condition(state)
    block* cur;
    block* tar;
    cur = head->head;
    tar = cur;
    while(cur != NULL){
        if(cond == YOUNG){
            if(metadata->state[cur->idx] <= metadata->state[tar->idx]){
                tar = cur;
            }
        } else if (cond > 0){
            if((metadata->state[cur->idx] <= metadata->state[tar->idx])&&
               (metadata->state[cur->idx] >= cond)){
                tar = cur;
            }
        } else if (cond == OLD){
            if(metadata->state[cur->idx] >= metadata->state[tar->idx]){
                tar = cur;
            }
        }
        cur = cur->next;
    }
    return tar;
}

int is_idx_in_list(bhead* head, int tar){
    block* cur;
    cur = head->head;
    int ret = 0;
    while(cur != NULL){
        //printf("%d vs %d\n",cur->idx,tar);
        if(cur->idx == tar){
            ret = 1;
        }
        cur = cur->next;
    }
    return ret;
}