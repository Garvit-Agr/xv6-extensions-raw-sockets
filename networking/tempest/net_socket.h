#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stddef.h>
#include <sys/types.h>

int connect_to_host(const char *hostname, const char *port);
void send_data(int sockfd, const char *data, size_t length);
void close_socket(int sockfd);

#endif