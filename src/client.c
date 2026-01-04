#include "network.h"
#include "protocol.h"
#include "process.h"
#include "client.h"
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>

/* Lecture complète (gère les lectures partielles / EINTR) */
static ssize_t read_all(int fd, void *buf, size_t count)
{
    size_t total = 0;
    char *p = buf;
    while (total < count) {
        ssize_t r = read(fd, p + total, count - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break; /* EOF */
        total += (size_t)r;
    }
    return (ssize_t)total;
}

int fetch_processes(process_t *list, int max)
{
    int sock = connect_to_server("127.0.0.1", SERVER_PORT);
    if (sock < 0) return 0;

    int32_t net_count;
    if (read_all(sock, &net_count, sizeof(int32_t)) != sizeof(int32_t)) {
        close(sock);
        return 0;
    }
    int count = ntohl(net_count);

    if (count > max) count = max;
    if (count < 0) count = 0;

    size_t bytes = sizeof(process_t) * (size_t)count;
    if (bytes > 0) {
        if (read_all(sock, list, bytes) != (ssize_t)bytes) {
            close(sock);
            return 0;
        }
    }

    close(sock);

    return count;
}