#include "../../include/process.h"
#include <unistd.h>

typedef struct { int pid; unsigned long long ticks; } prev_ticks_t;

static unsigned long long g_prev_total = 0;

static prev_ticks_t g_prev[16384];
static int g_prev_count = 0;

int find_prev_index(int pid){
    for (int i=0;i<g_prev_count;i++)
        if (g_prev[i].pid == pid)
            return i;
    return -1;
}

void set_prev_ticks(int pid, unsigned long long t){
    int i = find_prev_index(pid);
    if (i >= 0){
        g_prev[i].ticks = t;
        return;
    }
    g_prev[g_prev_count].pid = pid;
    g_prev[g_prev_count].ticks = t;
    g_prev_count++;
}

unsigned long long get_prev_total(){ return g_prev_total; }
void set_prev_total(unsigned long long t){ g_prev_total = t; }
