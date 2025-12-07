#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "../../include/process_reader.h"
#include "../../include/process.h"
#include "../../include/cpu_history.h"
#include "../../include/list_utils.h"

#define MAX_LINE_LENGTH 1024
#define MAX_SCAN_PIDS   4096

/* ---------- Utils ---------- */

static int is_numeric(const char *s){
    if (!*s) return 0;
    while (*s){
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static long HZ(void){
    static long hz = 0;
    if (!hz) hz = sysconf(_SC_CLK_TCK);
    return hz;
}

static double lire_uptime_seconds(void){
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return 0.0;
    double up = 0.0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return up;
}

static long lire_memtotal_kb(void){
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char key[64];
    long val = 0;
    while (fscanf(f, "%63s %ld %*s", key, &val) == 2){
        if (strcmp(key, "MemTotal:") == 0){
            fclose(f);
            return val;
        }
        int c; while ((c = fgetc(f)) != '\n' && c != EOF);
    }
    fclose(f);
    return 0;
}

static unsigned long long lire_total_jiffies(void){
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;

    char line[MAX_LINE_LENGTH];
    unsigned long long v[10] = {0}, total = 0;

    if (fgets(line, sizeof(line), f) && strncmp(line, "cpu ", 4) == 0){
        int n = sscanf(line+4, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &v[0], &v[1], &v[2], &v[3], &v[4],
                       &v[5], &v[6], &v[7], &v[8], &v[9]);
        for (int i=0; i<n; i++) total += v[i];
    }

    fclose(f);
    return total;
}

static void uid_to_name(uid_t uid, char out[32]){
    struct passwd pwd, *res = NULL;
    char buf[4096];

    if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &res) == 0 && res){
        strncpy(out, pwd.pw_name, 31);
        out[31] = '\0';
    }
    else snprintf(out, 32, "%u", (unsigned)uid);
}

/* ---------- Lecture /proc/[pid]/stat ---------- */

static int lire_proc_times_ticks(int pid,
                                 unsigned long long *ticks_io,
                                 unsigned long long *start_io)
{
    char path[128], line[MAX_LINE_LENGTH];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(line, sizeof(line), f)){
        fclose(f);
        return -1;
    }
    fclose(f);

    char *rparen = strrchr(line, ')');
    if (!rparen) return -1;

    char *p = rparen + 2;

    unsigned long long ut = 0, st = 0, start = 0;
    char *save = NULL;
    int field = 3;

    for (char *tok = strtok_r(p, " ", &save);
         tok;
         tok = strtok_r(NULL, " ", &save), field++)
    {
        if (field == 14) ut = strtoull(tok, NULL, 10);
        else if (field == 15) st = strtoull(tok, NULL, 10);
        else if (field == 22){
            start = strtoull(tok, NULL, 10);
            break;
        }
    }

    *ticks_io = ut + st;
    *start_io = start;
    return 0;
}

/* ---------- Lecture /proc/[pid]/status + comm ---------- */

static int lire_comm_state_mem_user(int pid, process_t *pr){
    memset(pr, 0, sizeof(*pr));

    pr->pid = pid;
    strcpy(pr->state, "N/A");
    strcpy(pr->name,  "N/A");
    strcpy(pr->user,  "?");

    char path[128], line[MAX_LINE_LENGTH];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    uid_t uid = 0;

    while (fgets(line, sizeof(line), f)){
        if (sscanf(line, "State:%*s %7s", pr->state) == 1) continue;

        if (strncmp(line, "VmRSS:", 6) == 0){
            int kb = 0;
            if (sscanf(line, "VmRSS:%*[^0-9]%d", &kb) == 1)
                pr->memory_kb = kb;
        }
        else if (strncmp(line, "Uid:", 4) == 0){
            sscanf(line, "Uid:%u", &uid);
        }
    }
    fclose(f);

    uid_to_name(uid, pr->user);

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (f){
        if (fgets(pr->name, sizeof(pr->name), f)){
            size_t n = strlen(pr->name);
            if (n && pr->name[n-1] == '\n')
                pr->name[n-1] = '\0';
        }
        fclose(f);
    }

    return 0;
}

/* ---------- Collecte de TOUS les processus + tri ---------- */

static int cmp_cpu_desc(const void *a, const void *b){
    const process_t *x = (const process_t*)a;
    const process_t *y = (const process_t*)b;

    if (y->cpu_percent > x->cpu_percent) return 1;
    if (y->cpu_percent < x->cpu_percent) return -1;
    if (y->mem_percent > x->mem_percent) return 1;
    if (y->mem_percent < x->mem_percent) return -1;

    return x->pid - y->pid;
}

int collect_all_processes(process_t **out_arr){
    int *pids = malloc(sizeof(int) * MAX_SCAN_PIDS);
    if (!pids) return 0;

    DIR *d = opendir("/proc");
    if (!d){
        free(pids);
        return 0;
    }

    struct dirent *e;
    int n_pids = 0;

    while ((e = readdir(d)) != NULL && n_pids < MAX_SCAN_PIDS){
        if (!is_numeric(e->d_name)) continue;
        int pid = atoi(e->d_name);
        if (pid > 0) pids[n_pids++] = pid;
    }
    closedir(d);

    process_t *arr = malloc(sizeof(process_t) * n_pids);
    if (!arr){
        free(pids);
        return 0;
    }

    unsigned long long cur_total = lire_total_jiffies();
    unsigned long long prev_total = get_prev_total();
    unsigned long long delta_total = (prev_total == 0)
        ? 0
        : cur_total - prev_total;

    set_prev_total(cur_total);

    long mem_total = lire_memtotal_kb();
    double uptime = lire_uptime_seconds();
    double hz = (double)HZ();

    int kept = 0;

    for (int i=0; i < n_pids; i++){
        process_t pr;

        if (lire_comm_state_mem_user(pids[i], &pr) != 0)
            continue;

        unsigned long long ticks = 0, start = 0;

        if (lire_proc_times_ticks(pr.pid, &ticks, &start) == 0){
            int idx = find_prev_index(pr.pid);

            if (delta_total > 0 && idx >= 0){
                unsigned long long delta_proc = ticks - g_prev[idx].ticks;
                pr.cpu_percent = (double)delta_proc /
                                 (double)delta_total * 100.0;
                if (pr.cpu_percent < 0.0)
                    pr.cpu_percent = 0.0;
            }
            else pr.cpu_percent = 0.0;

            set_prev_ticks(pr.pid, ticks);

            pr.cpu_time_s = ticks / hz;

            double start_s = start / hz;
            pr.elapsed_s = uptime - start_s;
            if (pr.elapsed_s < 0.0)
                pr.elapsed_s = 0.0;
        }

        if (mem_total > 0)
            pr.mem_percent =
                (double)pr.memory_kb / (double)mem_total * 100.0;

        arr[kept++] = pr;
    }

    free(pids);

    qsort(arr, kept, sizeof(process_t), cmp_cpu_desc);

    *out_arr = arr;
    return kept;
}
