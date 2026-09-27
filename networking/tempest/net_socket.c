#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "net_socket.h"
#include "cli.h"

int connect_to_host(const char *hostname, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd=-1;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;

    if ((status=getaddrinfo(hostname, port, &hints, &res))!=0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (p=res;p!=NULL;p=p->ai_next) {
        if ((sockfd=socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1) continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen)==-1) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (p==NULL) {
        freeaddrinfo(res);
        die("tempest: failed to connect to host");
    }

    freeaddrinfo(res);
    return sockfd;
}

void send_data(int sockfd, const char *data, size_t length) {
    size_t total_sent=0;
    size_t bytes_left=length;
    ssize_t n;

    while (total_sent<length) {
        n=send(sockfd, data+total_sent ,bytes_left, 0);

        if (n<=0) die("tempest: failed to send data");

        total_sent+=n;
        bytes_left-=n;
    }
}

void close_socket(int sockfd) {
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}