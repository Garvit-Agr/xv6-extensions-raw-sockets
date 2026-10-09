#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "discovery.h"
#include "logger.h"

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200
#endif

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
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
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

void send_broadcast(int sockfd, const char *name, int listening_port, int my_id) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family=AF_INET;
    broadcast_addr.sin_port=htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s|%s|%d|%d", MAGIC_PACKET, name, listening_port, my_id);

    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    log_event("Sent UDP broadcast: %s", payload);
}

void receive_broadcasts(int sockfd, Player players[], int *player_count, int *max_known_id, int my_id) {
    char buffer[512];
    struct sockaddr_in sender_addr;
    socklen_t sender_len=sizeof(sender_addr);

    ssize_t n=recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&sender_addr, &sender_len);
    
    if (n>0) {
        if (strncmp(buffer, MAGIC_PACKET, strlen(MAGIC_PACKET))!=0) return;

        buffer[n]='\0';
        
        char *magic=strtok(buffer, "|");
        char *name=strtok(NULL, "|");
        char *port_str=strtok(NULL, "|");
        char *id_str=strtok(NULL, "|");

        if (magic && name && port_str && id_str && strcmp(magic, MAGIC_PACKET)==0) {
            int port=atoi(port_str);
            int remote_id=atoi(id_str);
            if (remote_id > *max_known_id) *max_known_id=remote_id;

            if (my_id>0 && remote_id==my_id) return;

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender_addr.sin_addr), ip, INET_ADDRSTRLEN);
            
            log_event("Received UDP broadcast from %s (ID: %d, IP: %s, Port: %d)", name, remote_id, ip, port);

            int found=0;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            for (int i=0; i<*player_count; i++) {
                if (strcmp(players[i].ip_addr, ip)==0 && players[i].port==port) {
                    players[i].last_seen=now;
                    strcpy(players[i].name, name);
                    players[i].id=remote_id;
                    if (!players[i].active) players[i].active=1;
                    found=1;
                    break;
                }
            }

            if (!found && *player_count<MAX_PLAYERS && remote_id>0) {
                players[*player_count].id=remote_id;
                strcpy(players[*player_count].name, name);
                strcpy(players[*player_count].ip_addr, ip);
                players[*player_count].port=port;
                players[*player_count].last_seen=now;
                players[*player_count].active=1;
                (*player_count)++;
            }
        }
    }
}

void cleanup_stale_players(Player players[], int *player_count) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    for (int i=0; i<*player_count; i++) {
        if (players[i].active) {
            long elapsed_sec=now.tv_sec-players[i].last_seen.tv_sec;
            if (elapsed_sec>=5) {
                players[i].active=0;
                log_event("Player %s (ID: %d) timed out after %ld seconds of inactivity.", players[i].name, players[i].id, elapsed_sec);
            }
        }
    }
}

void print_players(Player players[], int player_count) {
    Player temp;
    for (int i=0; i<player_count-1; i++) {
        for (int j=i+1; j<player_count; j++) {
            if (players[i].id>players[j].id) {
                temp=players[i];
                players[i]=players[j];
                players[j]=temp;
            }
        }
    }

    printf("\033[H\033[J");
    printf("Players Online:\n");
    printf("ID\t%-15s\t%-15s\tPort\tLast Seen\n", "Name", "IP Addr");
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (int i=0; i<player_count; i++) {
        if (players[i].active) {
            long elapsed_sec=now.tv_sec-players[i].last_seen.tv_sec;
            printf("%d\t%-15.15s\t%-15s\t%d\t%ld s. ago\n", 
                   players[i].id, players[i].name, players[i].ip_addr, players[i].port, elapsed_sec);
        }
    }
    printf("____________________________________________________\n> ");
    fflush(stdout);
}