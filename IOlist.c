#include "stateaware.h"

IOhead* ll_init_IO(){
    //init and return head poiner
    IOhead* head;
    head = (IOhead*)malloc(sizeof(IOhead));
    head->reqnum = 0;
    head->head = NULL;
    return head;
}

void ll_free_IO(IOhead* head){
    int freecnt = 0;
    int component = head->reqnum;
    IO* cur = head->head;
    //free linked blocks starting from the head
    IO* temp = head->head;
    while(freecnt != component){
        cur = cur->next;
        freecnt++;
        free(temp);
        temp = cur;
    }   
}

void ll_append_IO(IOhead* head, IO* new){
    //append block
    IO* cur;
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
    head->reqnum++;

}

IO* ll_pop_IO(IOhead* head){
    //pop block
    IO* temp;
    temp = head->head;
    if(temp == NULL){
        printf("[IO]head is null\n");
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
    head->reqnum--;
    temp->prev = NULL;
    temp->next = NULL;
    return temp;
}

/*
IO* ll_findidx(IOhead* head, int tar){
    //find certain block
    IO* cur = NULL;
    cur = head->head;
    
    while(cur != NULL){
        //printf("cur->idx:%d\n",cur->idx);
        if(cur->idx == tar)
            break;
        else{
            cur = cur->next;
        }
    }
    if(cur==NULL){
        printf("[findidx]cannot find target block\n");
    }
    return cur;
}
IO* ll_remove(IOhead* head, int tar){
    //remove certain block
    IO* cur = NULL;
    cur = head->head;
    
    while(cur != NULL){
        //printf("cur->idx:%d\n",cur->idx);
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
    head->reqnum--;
    //printf("bnum:%d\n",head->reqnum);
    if(head->reqnum <= -1) 
        abort();
    cur->prev = NULL;
    cur->next = NULL;
    return cur;
}

IO* ll_find(meta* metadata, IOhead* head, int cond){
    //find a block which meets condition(state)
    IO* cur;
    IO* tar;
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

IO* ll_condremove(meta* metadata, IOhead* head, int cond){
    //remove certain block, according to condition of block.
    //if cond == YOUNG, return youngest block
    //else if cond over 1 is given, return only the block with state >= cond.
    IO* cur;
    IO* tar;
    cur = head->head;
    tar = cur;
    while(cur != NULL){
        printf("[ll]traversing %d(%d)\n",cur->idx, metadata->state[cur->idx]);
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
    head->reqnum--;
    printf("[ll]reqnum:%d, tar : %d\n",head->reqnum,tar->idx);
    tar->prev = NULL;
    tar->next = NULL;
    return tar;
}

int is_idx_in_list(IOhead* head, int tar){
    IO* cur;
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
*/