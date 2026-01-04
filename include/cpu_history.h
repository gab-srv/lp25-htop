#ifndef CPU_HISTORY_H
#define CPU_HISTORY_H
//inutile avec nouvelle version réseau
typedef struct {
    int pid;
    unsigned long long ticks;
} prev_ticks_t;

/* Variables externes */
extern unsigned long long g_prev_total;
extern prev_ticks_t g_prev[16384];
extern int g_prev_count;

/* Fonctions */
int find_prev_index(int pid);
void set_prev_ticks(int pid, unsigned long long t);
unsigned long long get_prev_total(void);
void set_prev_total(unsigned long long t);

#endif // CPU_HISTORY_H
