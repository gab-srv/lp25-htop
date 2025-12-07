/* restart.c : relancer proprement un processus en lisant /proc/<pid>/exe et cmdline */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h> /* PATH_MAX */

#define MAX_ARGS 64

/* Vérifie si pid existe (0 si non) */
static int pid_exists(pid_t pid){
    if (pid <= 0) return 0;
    if (kill(pid, 0) == 0) return 1;
    return (errno == EPERM);
}

/* Lit /proc/<pid>/exe et /proc/<pid>/cmdline pour construire argv */
static int read_cmdline(pid_t pid, char *exe_path, size_t exe_sz,
                        char *buf, size_t buf_sz, char *argvv[MAX_ARGS])
{
    char linkpath[128];
    snprintf(linkpath, sizeof(linkpath), "/proc/%d/exe", (int)pid);

    ssize_t n = readlink(linkpath, exe_path, exe_sz - 1);
    if (n < 0) return -1;
    exe_path[n] = '\0';

    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    size_t r = fread(buf, 1, buf_sz - 1, f);
    fclose(f);
    if (r == 0) return -1;
    buf[r] = '\0';

    size_t i = 0, argc = 0;
    while (i < r && argc < MAX_ARGS - 1){
        size_t len = strlen(&buf[i]);
        if (len == 0) break;
        argvv[argc++] = &buf[i];
        i += len + 1;
    }
    argvv[argc] = NULL;

    if (argc == 0 || argvv[0][0] == '\0')
        argvv[0] = exe_path;

    return (int)argc;
}

/* Implémentation publique */
void action_restart(pid_t pid)
{
    if (!pid_exists(pid)){
        printf("PID %d inexistant.\n", (int)pid);
        return;
    }

    char exe_path[PATH_MAX];
    char cmdbuf[4096];
    char *argvv[MAX_ARGS] = {0};

    int argc = read_cmdline(pid, exe_path, sizeof(exe_path),
                            cmdbuf, sizeof(cmdbuf), argvv);

    if (argc < 1){
        perror("read_cmdline");
        return;
    }

    printf("Redémarrage de PID %d → %s\n", (int)pid, exe_path);

    /* arrêt propre */
    kill(pid, SIGTERM);
    usleep(200 * 1000);

    if (pid_exists(pid))
        kill(pid, SIGKILL);

    /* relance */
    pid_t child = fork();
    if (child == 0){
        execv(exe_path, argvv);
        perror("execv");
        _exit(127);
    } else if (child < 0){
        perror("fork");
    } else {
        printf("Nouveau PID : %d\n", (int)child);
    }
}
