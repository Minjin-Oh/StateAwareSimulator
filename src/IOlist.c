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
    } else{
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
