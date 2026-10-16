#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stddef.h>
#include <time.h>

int connect_to_host(const char *hostname, const char *port, struct timespec start);
void send_data(int sockfd, const char *data, size_t length, struct timespec start);
char *receive_data(int sockfd, struct timespec start);
void close_socket(int sockfd);

#endif