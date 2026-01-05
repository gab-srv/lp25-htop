#ifndef PROCESS_H
#define PROCESS_H

typedef struct {
    int    pid;
    char   user[32];
    char   name[256];
    char   state[8];
    int    memory_kb;
    double mem_percent;
    double cpu_percent;
    double cpu_time_s;
    double elapsed_s;
} process_t;

#endif
