#include "stateaware.h"

bhead* ll_init(){
    //init and return head poiner
    bhead* head;
    head = (bhead*)malloc(sizeof(bhead));
    head->blocknum = 0;
    head->head = NULL;
    return head;
}

void ll_free(bhead* head){
    int freecnt = 0;
    int component = head->blocknum;
    block* cur = head->head;
    //free linked blocks starting from the head
    block* temp = head->head;
    while(freecnt != component){
        cur = cur->next;
        freecnt++;
        printf("temp info : %d, %dvs%d\n",temp->idx,freecnt,component);
        free(temp);
        temp = cur;
    }   
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

block* ll_condremove(meta* metadata, bhead* head, int cond){
    //remove certain block, according to condition of block.
    //if cond == YOUNG, return youngest block
    //else if cond over 1 is given, return only the block with state >= cond.
    block* cur;
    block* tar;
    cur = head->head;
    tar = cur;
    while(cur != NULL){
        //printf("[ll]traversing %d(%d)\n",cur->idx, metadata->state[cur->idx]);
        if(cond == YOUNG){
            if(metadata->state[cur->idx] <= metadata->state[tar->idx]){
                tar = cur;
            }
        }
        cur = cur->next;
    }

    //edge cases
    //if condition is not YOUNG, check if head is suitable
    if(tar == NULL){
        printf("there's no such block!\n");
        return NULL;
    }

    //link other blocks if necessary
    if((tar->next != NULL) && (tar->prev != NULL)){//middle
        tar->prev->next = tar->next;
        tar->next->prev = tar->prev;
    }
    else if (head->head == tar){//head
        if(tar->next != NULL){//removing head & link next as head
            head->head = tar->next;
            head->head->prev = NULL;
        } else {
            head->head = NULL;
        }
    }
    else if ((tar->next == NULL) && (tar->prev != NULL)){//tail
        tar->prev->next = NULL;
    }
    head->blocknum--;
    //printf("[ll]blocknum:%d, tar : %d\n",head->blocknum,tar->idx);
    tar->prev = NULL;
    tar->next = NULL;
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