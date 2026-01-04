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
#include <stdbool.h>

#include "../include/process_reader.h"
#include "../include/process.h"

#define MAX_LINE_LENGTH 1024
#define MAX_SCAN_PIDS   4096

/* ---------- Variables globales pour le calcul du CPU ---------- */
static unsigned long long prev_total_jiffies = 0;

/* Remplacement de l'ancienne approche dangereuse prev_proc_jiffies[pid]:
   table simple (pid,ticks) pour stocker les jiffies précédentes par pid. */
typedef struct {
    int pid; /* 0 = entrée libre (on ignore les PID 0/1 dans la collecte) */
    unsigned long long ticks;
} prev_entry_t;

static prev_entry_t prev_procs[MAX_SCAN_PIDS] = {0};

/* Cherche et retourne les jiffies précédentes pour pid, 0 si absent */
static unsigned long long prev_proc_get(int pid)
{
    for (int i = 0; i < MAX_SCAN_PIDS; ++i) {
        if (prev_procs[i].pid == pid) return prev_procs[i].ticks;
    }
    return 0;
}

/* Insère ou met à jour l'entrée pour pid */
static void prev_proc_set(int pid, unsigned long long ticks)
{
    for (int i = 0; i < MAX_SCAN_PIDS; ++i) {
        if (prev_procs[i].pid == pid) {
            prev_procs[i].ticks = ticks;
            return;
        }
    }
    /* sinon trouver une case libre */
    for (int i = 0; i < MAX_SCAN_PIDS; ++i) {
        if (prev_procs[i].pid == 0) {
            prev_procs[i].pid = pid;
            prev_procs[i].ticks = ticks;
            return;
        }
    }
    /* si table pleine, écrase la première (simple stratégie) */
    prev_procs[0].pid = pid;
    prev_procs[0].ticks = ticks;
}

/* ---------- Utils ---------- */

static int is_numeric(const char *s) {
    if (!*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static long HZ(void) {
    static long hz = 0;
    if (!hz) hz = sysconf(_SC_CLK_TCK);
    return hz;
}

static double lire_uptime_seconds(void) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return 0.0;
    double up = 0.0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return up;
}

static long lire_memtotal_kb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char key[64];
    long val = 0;
    while (fscanf(f, "%63s %ld %*s", key, &val) == 2) {
        if (strcmp(key, "MemTotal:") == 0) {
            fclose(f);
            return val;
        }
        int c; while ((c = fgetc(f)) != '\n' && c != EOF);
    }
    fclose(f);
    return 0;
}

static unsigned long long lire_total_jiffies(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;

    char line[MAX_LINE_LENGTH];
    unsigned long long v[10] = {0}, total = 0;

    if (fgets(line, sizeof(line), f) && strncmp(line, "cpu ", 4) == 0) {
        int n = sscanf(line+4, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &v[0], &v[1], &v[2], &v[3], &v[4],
                       &v[5], &v[6], &v[7], &v[8], &v[9]);
        for (int i = 0; i < n; i++) total += v[i];
    }

    fclose(f);
    return total;
}

static void uid_to_name(uid_t uid, char out[32]) {
    struct passwd pwd, *res = NULL;
    char buf[4096];

    if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &res) == 0 && res) {
        strncpy(out, pwd.pw_name, 31);
        out[31] = '\0';
    }
    else snprintf(out, 32, "%u", (unsigned)uid);
}

/* ---------- Lire cmdline et construire un nom d'affichage court ----------
   - on prend le premier argument (la commande)
   - on prend le basename si c'est un chemin
   - on ajoute les premiers arguments (tronqués) si présents
   - fallback sur /proc/<pid>/comm si cmdline vide
*/
static void read_cmdline_or_comm(int pid, char out[256], size_t out_size) {
    char path[128];
    char buf[512];

    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        if (n > 0) {
            /* cmdline contient des '\0' entre args → remplacer par espace */
            for (size_t i = 0; i < n; ++i) if (buf[i] == '\0') buf[i] = ' ';
            buf[n] = '\0';
            /* enlever éventuels espaces de fin */
            while (n > 0 && buf[n-1] == ' ') { buf[n-1] = '\0'; --n; }

            /* extraire le premier token (commande) */
            char first[256] = {0};
            size_t i = 0;
            while (i < n && buf[i] != ' ' && i + 1 < sizeof(first)) {
                first[i] = buf[i];
                i++;
            }
            first[i] = '\0';

            /* basename */
            char *base = strrchr(first, '/');
            const char *name = base ? base + 1 : first;

            /* construire la partie arguments (limiter la longueur) */
            const char *args = (i < n && buf[i] == ' ') ? (buf + i + 1) : NULL;
            if (args && *args) {
                /* on garde seulement un extrait des arguments pour lisibilité */
                char args_trim[200] = {0};
                size_t max_args = sizeof(args_trim) - 1;
                strncpy(args_trim, args, max_args);
                args_trim[max_args] = '\0';
                /* remplacer multiples espaces par un seul (optionnel) */
                char args_compact[200] = {0};
                size_t p = 0;
                bool last_space = false;
                for (size_t k = 0; args_trim[k] != '\0' && p + 1 < sizeof(args_compact); ++k) {
                    if (args_trim[k] == ' ') {
                        if (!last_space) {
                            args_compact[p++] = ' ';
                            last_space = true;
                        }
                    } else {
                        args_compact[p++] = args_trim[k];
                        last_space = false;
                    }
                }
                args_compact[p] = '\0';

                /* assembler "name args_compact", tronquer si nécessaire */
                snprintf(out, out_size, "%s %s", name, args_compact);
                out[out_size - 1] = '\0';
            } else {
                /* juste le nom */
                strncpy(out, name, out_size - 1);
                out[out_size - 1] = '\0';
            }

            /* Si trop long, tronquer proprement */
            if (strlen(out) >= out_size - 1) {
                out[out_size - 4] = '.';
                out[out_size - 3] = '.';
                out[out_size - 2] = '.';
                out[out_size - 1] = '\0';
            }
            return;
        }
    }

    /* fallback : /proc/<pid>/comm */
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (f) {
        if (fgets(out, (int)out_size, f)) {
            size_t len = strlen(out);
            if (len && out[len-1] == '\n') out[len-1] = '\0';
        } else {
            out[0] = '\0';
        }
        fclose(f);
    } else {
        out[0] = '\0';
    }
}

/* ---------- Lecture /proc/[pid]/stat ---------- */

static int lire_proc_times_ticks(int pid, unsigned long long *ticks_io, unsigned long long *start_io) {
    char path[128], line[MAX_LINE_LENGTH];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(line, sizeof(line), f)) {
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
         tok = strtok_r(NULL, " ", &save), field++) {
        if (field == 14) ut = strtoull(tok, NULL, 10);
        else if (field == 15) st = strtoull(tok, NULL, 10);
        else if (field == 22) {
            start = strtoull(tok, NULL, 10);
            break;
        }
    }

    *ticks_io = ut + st;
    *start_io = start;
    return 0;
}

/* ---------- Lecture /proc/[pid]/status + comm (mis à jour pour cmdline) ---------- */

static int lire_comm_state_mem_user(int pid, process_t *pr) {
    memset(pr, 0, sizeof(*pr));

    pr->pid = pid;
    strcpy(pr->state, "N/A");
    strcpy(pr->name,  "N/A");
    strcpy(pr->user,  "?");
    pr->memory_kb = 0;

    char path[128], line[MAX_LINE_LENGTH];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    uid_t uid = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "State:%*s %7s", pr->state) == 1) continue;

        if (strncmp(line, "VmRSS:", 6) == 0) {
            int kb = 0;
            if (sscanf(line, "VmRSS: %d", &kb) == 1) {
                pr->memory_kb = kb;
            }
        }
        else if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line, "Uid:%u", &uid);
        }
    }
    fclose(f);

    uid_to_name(uid, pr->user);

    /* lire la commande complète si possible et produire un nom d'affichage court */
    char cmdline_display[256] = {0};
    read_cmdline_or_comm(pid, cmdline_display, sizeof(cmdline_display));
    if (cmdline_display[0] != '\0') {
        strncpy(pr->name, cmdline_display, sizeof(pr->name)-1);
        pr->name[sizeof(pr->name)-1] = '\0';
    } else {
        strncpy(pr->name, "N/A", sizeof(pr->name)-1);
    }

    return 0;
}

/* ---------- Collecte de TOUS les processus + tri + filtrage des threads noyau ---------- */

static int cmp_cpu_desc(const void *a, const void *b) {
    const process_t *x = (const process_t*)a;
    const process_t *y = (const process_t*)b;

    if (y->cpu_percent > x->cpu_percent) return 1;
    if (y->cpu_percent < x->cpu_percent) return -1;
    if (y->mem_percent > x->mem_percent) return 1;
    if (y->mem_percent < x->mem_percent) return -1;

    return x->pid - y->pid;
}

int get_processes(process_t *list, int max) {
    int *pids = malloc(sizeof(int) * MAX_SCAN_PIDS);
    if (!pids) return 0;

    DIR *d = opendir("/proc");
    if (!d) {
        free(pids);
        return 0;
    }

    struct dirent *e;
    int n_pids = 0;

    while ((e = readdir(d)) != NULL && n_pids < MAX_SCAN_PIDS) {
        if (!is_numeric(e->d_name)) continue;
        int pid = atoi(e->d_name);
        if (pid <= 1) continue; // Ignorer les PID 0 et 1 (système)
        pids[n_pids++] = pid;
    }
    closedir(d);

    unsigned long long cur_total = lire_total_jiffies();
    unsigned long long delta_total = (prev_total_jiffies == 0) ? 1 : cur_total - prev_total_jiffies;
    prev_total_jiffies = cur_total;

    long mem_total = lire_memtotal_kb();
    double uptime = lire_uptime_seconds();
    double hz = (double)HZ();

    int kept = 0;

    for (int i = 0; i < n_pids; i++) {
        process_t pr;

        if (lire_comm_state_mem_user(pids[i], &pr) != 0)
            continue;

        unsigned long long ticks = 0, start = 0;

        if (lire_proc_times_ticks(pr.pid, &ticks, &start) == 0) {
            unsigned long long prev_ticks = prev_proc_get(pr.pid);
            unsigned long long delta_proc = (prev_ticks == 0) ? 0 : ticks - prev_ticks;
            prev_proc_set(pr.pid, ticks);

            pr.cpu_time_s = ticks / hz;

            double start_s = start / hz;
            pr.elapsed_s = uptime - start_s;
            if (pr.elapsed_s < 0.0)
                pr.elapsed_s = 0.0;

            if (delta_total > 0) {
                pr.cpu_percent = ((double)delta_proc / (double)delta_total) * 100.0;
            }
        }

        if (mem_total > 0)
            pr.mem_percent = (double)pr.memory_kb / (double)mem_total * 100.0;

        bool skip = false;
        if (pr.memory_kb == 0 && strcmp(pr.user, "root") == 0) {
            static const char *skip_prefixes[] = {
                "kworker", "rcu_", "migration", "ksoftirqd", "cpuhp", "kthreadd",
                "kswapd", "khungtaskd", "watchdog", "oom_reaper", "kdevtmpfs",
                "khugepaged", NULL
            };
            for (const char **pfx = skip_prefixes; *pfx; ++pfx) {
                size_t L = strlen(*pfx);
                if (strncmp(pr.name, *pfx, L) == 0) {
                    skip = true;
                    break;
                }
            }
            if (!skip && (strchr(pr.name, '/') != NULL || pr.name[0] == '[')) {
                skip = true;
            }
        }
        if (skip) continue;

        if (kept >= max)
            break;

        list[kept++] = pr;
    }

    free(pids);

    qsort(list, kept, sizeof(process_t), cmp_cpu_desc);

    return kept;
}