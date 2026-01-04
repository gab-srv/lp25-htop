#ifndef NETWORK_H
#define NETWORK_H

int create_server_socket(int port);
int wait_client(int server_fd);

int connect_to_server(const char *ip, int port);

#endif