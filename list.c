#include "stateaware.h"

bhead* ll_init(){
    //init and return head poiner
    bhead* head;
    head = (bhead*)malloc(sizeof(bhead));
    head->blocknum = 0;
    head->head = NULL;
}

void ll_free(bhead* head){
    block* cur = head->head;
    //free linked blocks starting from the head
    while(cur->next != NULL){
        cur = cur->next;
        free(cur->prev);
    }
    //if cur reaches last block, free it.
    free(cur);
    //free head
    free(head);
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
        printf("head is null");
        abort();
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
    return temp;
}

block* ll_remove(bhead* head, int tar){
    //remove certain block
    block* cur = NULL;
    cur = head->head;
    while((cur->idx != tar) && (cur != NULL)){
        cur = cur->next;
    }
    //escape code
    if(cur == NULL){
        printf("there's no such block!\n");
        return NULL;
    }
    //conditions of target block
    if((cur->next != NULL) && (cur->prev != NULL)){
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
    }
    else if ((cur->next != NULL) && (cur->prev == NULL)){
        head->head = cur->next;
        head->head->prev = NULL;
    }
    else if ((cur->next == NULL) && (cur->prev != NULL)){
        cur->prev->next = NULL;
    }
    else{//only one block inside
        head->head = NULL;
    }
    head->blocknum--;
    return cur;
}

block* ll_condremove(meta* metadata, bhead* head, int cond){
    //remove certain block, according to condition of block.
    block* cur;
    block* tar;
    cur = head->head;
    tar = cur;
    while(cur != NULL){
        if(cond == YOUNG){
            if(metadata->state[cur->idx] <= metadata->state[tar->idx]){
                tar = cur;
            }
        }
        cur = cur->next;
    }
    //escape code
    if(tar == NULL){
        printf("there's no such block!\n");
        return NULL;
    }
    //conditions of target block
    if((tar->next != NULL) && (tar->prev != NULL)){
        tar->prev->next = tar->next;
        tar->next->prev = tar->prev;
    }
    else if ((tar->next != NULL) && (tar->prev == NULL)){
        head->head = tar->next;
        head->head->prev = NULL;
    }
    else if ((tar->next == NULL) && (tar->prev != NULL)){
        tar->prev->next = NULL;
    }
    else{//only one block inside
        head->head = NULL;
    }
    head->blocknum--;
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