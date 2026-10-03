#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "discovery.h"

int init_broadcast_socket(void) {
    int sockfd=socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int broadcast_enable=1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable))<0) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int reuse=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family=AF_INET;
    local_addr.sin_port=htons(BROADCAST_PORT);
    local_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr))<0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void send_broadcast(int sockfd, const char *name, int listening_port) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family=AF_INET;
    broadcast_addr.sin_port=htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s|%s|%d", MAGIC_PACKET, name, listening_port);

    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
}