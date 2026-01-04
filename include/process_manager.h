#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H
//inutile avec nouvelle version réseau
#include "process.h"
#include "list_utils.h"
#include "process_reader.h"
#include "cpu_history.h"

/* Fonction principale pour obtenir la liste locale */
process_node* get_local_process_list(int topN);

#endif // PROCESS_MANAGER_H
