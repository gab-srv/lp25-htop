/* actions.c : envoi simple de signaux, menu optionnel simplifié */
#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>   /* pour pid_t si besoin */

#include "../include/actions.h"

/* fonctions simples envoyant les signaux */
int action_stop(int pid){
    return kill((pid_t)pid, SIGSTOP);
}
int action_continue_(int pid){
    return kill((pid_t)pid, SIGCONT);
}
int action_term(int pid){
    return kill((pid_t)pid, SIGTERM);
}
int action_kill_(int pid){
    return kill((pid_t)pid, SIGKILL);
}

/* Menu d'actions (optionnel) : utilise la fonction restart (définie dans restart.c) */
void actions_menu(void){
    int reponse = 0;
    int pid = 0;

    printf("Action :\n");
    printf("1. Arrêter (SIGTERM)\n");
    printf("2. Mettre en pause (SIGSTOP)\n");
    printf("3. Tuer (SIGKILL)\n");
    printf("4. Redémarrer\n");

    if (scanf("%d", &reponse) != 1) {
        fprintf(stderr, "Lecture choix échouée\n");
        return;
    }
    printf("PID : ");
    if (scanf("%d", &pid) != 1) {
        fprintf(stderr, "Lecture PID échouée\n");
        return;
    }

    switch (reponse){
        case 1:
            if (action_term(pid) == -1) perror("SIGTERM");
            else printf("Processus %d arrêté\n", pid);
            break;
        case 2:
            if (action_stop(pid) == -1) perror("SIGSTOP");
            else printf("Processus %d mis en pause\n", pid);
            break;
        case 3:
            if (action_kill_(pid) == -1) perror("SIGKILL");
            else printf("Processus %d tué\n", pid);
            break;
        case 4:
            /* appeler la fonction restart déclarée dans restart.c */
            extern void action_restart(pid_t pid);
            action_restart((pid_t)pid);
            break;
        default:
            printf("Choix invalide.\n");
            break;
    }
}
