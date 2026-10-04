#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol_tcp.h"

int init_tcp_listener(int port) {
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd<0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    int opt=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr))<0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(sockfd, 5)<0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    return sockfd;
}

int accept_challenge(int listen_sock, char *challenger_name) {
    struct sockaddr_in client_addr;
    socklen_t client_len=sizeof(client_addr);
    
    int client_sock=accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock<0) return -1;

    memset(challenger_name, 0, 50);
    recv(client_sock, challenger_name, 49, 0);

    printf("\n> %s challenged you! Accept? (yes/no): ", challenger_name);
    fflush(stdout);

    char response[10];
    if (fgets(response, sizeof(response), stdin)!=NULL) {
        response[strcspn(response, "\n")]='\0';
        if (strcmp(response, "yes")==0) {
            send(client_sock, "yes", 3, 0);
            return client_sock;
        }
    }
    
    send(client_sock, "no", 2, 0);
    close(client_sock);
    return -1;
}

int send_challenge(Player *p, const char *my_name) {
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd<0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(p->port);
    inet_pton(AF_INET, p->ip_addr, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))<0) {
        close(sockfd);
        printf("\n> Failed to connect to %s\n", p->name);
        return -1;
    }

    send(sockfd, my_name, strlen(my_name), 0);
    
    char response[10];
    memset(response, 0, sizeof(response));
    recv(sockfd, response, sizeof(response)-1, 0);

    if (strcmp(response, "yes")==0) {
        printf("\n> %s accepted the challenge!\n", p->name);
        return sockfd;
    } else {
        printf("\n> %s rejected the challenge.\n", p->name);
        close(sockfd);
        return -1;
    }
}