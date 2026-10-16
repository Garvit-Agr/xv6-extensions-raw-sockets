#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "net_socket.h"
#include "cli.h"

#define BUFFER_SIZE 8192

static long get_remaining_ms(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms=(now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000;
    long remaining_ms=10000-elapsed_ms;
    if (remaining_ms<=0) die("tempest: request timed out");
    return remaining_ms;
}

int connect_to_host(const char *hostname, const char *port, struct timespec start) {
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

        int flags=fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        int ret=connect(sockfd, p->ai_addr, p->ai_addrlen);
        if (ret==-1 && errno==EINPROGRESS) {
            long remaining_ms=get_remaining_ms(start);
            struct pollfd pfd={sockfd, POLLOUT, 0};
            int poll_ret=poll(&pfd, 1, remaining_ms);
            if (poll_ret==0) die("tempest: request timed out");
            if (poll_ret<0) {
                close(sockfd);
                continue;
            }
            int err=0;
            socklen_t errlen=sizeof(err);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &errlen)<0 || err!=0) {
                close(sockfd);
                continue;
            }
        }
        else if (ret==-1) {
            close(sockfd);
            continue;
        }

        fcntl(sockfd, F_SETFL, flags);
        break;
    }

    if (p==NULL) {
        freeaddrinfo(res);
        die("tempest: failed to connect to host");
    }

    freeaddrinfo(res);
    return sockfd;
}

void send_data(int sockfd, const char *data, size_t length, struct timespec start) {
    size_t total_sent=0;
    size_t bytes_left=length;
    ssize_t n;
    struct pollfd pfd;

    pfd.fd=sockfd;
    pfd.events=POLLOUT;

    while (total_sent<length) {
        long remaining_ms=get_remaining_ms(start);

        int ret=poll(&pfd, 1, remaining_ms);
        
        if (ret==0) die("tempest: request timed out");
        if (ret==-1) die("tempest: poll failed");

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

char *receive_data(int sockfd, struct timespec start) {
    size_t capacity=BUFFER_SIZE;
    char *buffer=malloc(capacity);
    if (!buffer) die("tempest: malloc failed");
    
    size_t total_received=0;
    ssize_t bytes_read;
    struct pollfd pfd;

    pfd.fd=sockfd;
    pfd.events=POLLIN;

    while (1) {
        long remaining_ms=get_remaining_ms(start);

        int ret=poll(&pfd, 1, remaining_ms);
        
        if (ret==0) die("tempest: request timed out");
        if (ret==-1) die("tempest: poll failed");

        bytes_read=recv(sockfd, buffer+total_received, capacity-total_received-1, 0);
        
        if (bytes_read<=0) break;

        total_received+=bytes_read;
        
        if (capacity-total_received<1024) {
            capacity*=2;
            buffer=realloc(buffer, capacity);
            if (!buffer) die("tempest: realloc failed");
        }
    }

    if (bytes_read==-1) die("tempest: recv failed");

    buffer[total_received]='\0';
    return buffer;
}