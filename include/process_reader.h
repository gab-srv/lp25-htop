#ifndef PROCESS_READER_H
#define PROCESS_READER_H

#include "process.h"
#include "cpu_history.h"

/* Fonction principale pour collecter les processus */
int collect_all_processes(process_t **out_arr);

#endif // PROCESS_READER_H
