#include <stdio.h>
#include <stdlib.h>
#include "../../include/process.h"

void free_process_list(process_node *head){
    while (head){
        process_node *tmp = head;
        head = head->next;
        free(tmp);
    }
}

void add_process(process_node **head, const process_t *p){
    process_node *n = malloc(sizeof(*n));
    if (!n) return;
    n->process = *p;
    n->next = NULL;

    if (!*head){
        *head = n;
        return;
    }

    process_node *c = *head;
    while (c->next) c = c->next;
    c->next = n;
}
