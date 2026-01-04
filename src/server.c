#include "../include/network.h"
#include "../include/protocol.h"
#include "../include/process.h"
#include "../include/process_reader.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

/* Écriture complète (gère écritures partielles / EINTR) */
static ssize_t write_all(int fd, const void *buf, size_t count)
{
    size_t total = 0;
    const char *p = buf;
    while (total < count) {
        ssize_t w = write(fd, p + total, count - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) break;
        total += (size_t)w;
    }
    return (ssize_t)total;
}

int main(void)
{
    printf("Démarrage du serveur...\n");

    /* éviter SIGPIPE quand on écrit sur un socket fermé */
    signal(SIGPIPE, SIG_IGN);

    int server_fd = create_server_socket(SERVER_PORT);
    if (server_fd < 0) {
        fprintf(stderr, "Impossible de créer la socket serveur\n");
        return 1;
    }
    printf("Socket serveur créé.\n");

    while (1) {
        int client_fd = wait_client(server_fd);
        if (client_fd < 0) {
            /* erreur d'accept, on attend et on continue */
            sleep(1);
            continue;
        }
        printf("Nouveau client connecté.\n");

        /* Option A : on envoie UNE seule mise à jour, puis on ferme la connexion.
           Le client (actuel) crée une nouvelle connexion à chaque rafraîchissement. */
        process_t processes[128];
        int count = get_processes(processes, 128);
        printf("Nombre de processus récupérés : %d\n", count);

        int32_t net_count = htonl(count);
        if (write_all(client_fd, &net_count, sizeof(int32_t)) != sizeof(int32_t)) {
            if (errno == EPIPE || errno == ECONNRESET) {
                /* Le client s'est déconnecté avant/pendant l'envoi : c'est attendu */
                fprintf(stderr, "Client déconnecté avant l'envoi du compteur (connexion fermée)\n");
            } else {
                perror("Erreur lors de l'envoi du nombre de processus");
            }
            close(client_fd);
            continue;
        }

        size_t bytes = sizeof(process_t) * (size_t)count;
        if (bytes > 0) {
            if (write_all(client_fd, processes, bytes) != (ssize_t)bytes) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    fprintf(stderr, "Client déconnecté avant l'envoi complet des processus\n");
                } else {
                    perror("Erreur lors de l'envoi des processus");
                }
                close(client_fd);
                continue;
            }
        }

        printf("Données envoyées au client. Fermeture de la connexion.\n");
        close(client_fd);
        /* Retour à accept() pour le prochain client/rafraîchissement */
    }

    close(server_fd);
    return 0;
}