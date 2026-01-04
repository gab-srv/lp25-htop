#ifndef PROCESS_READER_H
#define PROCESS_READER_H

#include "process.h"
#include "cpu_history.h"

/* Fonction principale pour collecter les processus */
int get_processes(process_t *list, int max);

#endif // PROCESS_READER_H
