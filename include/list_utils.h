#ifndef LIST_UTILS_H
#define LIST_UTILS_H

#include "process.h"

/* Libère toute une liste de processus */
void free_process_list(process_node *head);

/* Ajoute un processus à la fin de la liste */
void add_process(process_node **head, const process_t *p);

#endif // LIST_UTILS_H
