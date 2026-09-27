#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stddef.h>

int connect_to_host(const char *hostname, const char *port);
void send_data(int sockfd, const char *data, size_t length);
char *receive_data(int sockfd);
void close_socket(int sockfd);

#endif