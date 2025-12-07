#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "process.h"

process_node* get_local_process_list(int topN);
void free_process_list(process_node *head);

#endif
