#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <poll.h>

#include "discovery.h"
#include "logger.h"

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK (uint32_t)0x7f000001
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

static int is_self(struct in_addr addr, int port, int my_port) {
    if (port!=my_port) return 0;
    if (addr.s_addr==htonl(INADDR_LOOPBACK)) return 1;
    struct ifaddrs *ifap, *ifa;
    int match=0;
    if (getifaddrs(&ifap)==0) {
        for (ifa=ifap; ifa!=NULL; ifa=ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
                struct sockaddr_in *s=(struct sockaddr_in *)ifa->ifa_addr;
                if (s->sin_addr.s_addr==addr.s_addr) {
                    match=1;
                    break;
                }
            }
        }
        freeifaddrs(ifap);
    }
    return match;
}

void send_broadcast(int sockfd, const char *name, int listening_port) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family=AF_INET;
    broadcast_addr.sin_port=htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);

    char payload[256];
    snprintf(payload, sizeof(payload), "%s|%s|%d", MAGIC_PACKET, name, listening_port);

    ssize_t ret=sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    if (ret<0) {
        struct ifaddrs *ifap, *ifa;
        if (getifaddrs(&ifap)==0) {
            for (ifa=ifap; ifa!=NULL; ifa=ifa->ifa_next) {
                if (ifa->ifa_broadaddr && ifa->ifa_broadaddr->sa_family==AF_INET) {
                    if (strcmp(ifa->ifa_name, "lo0")==0 || strcmp(ifa->ifa_name, "lo")==0) continue;
                    struct sockaddr_in *baddr=(struct sockaddr_in *)ifa->ifa_broadaddr;
                    broadcast_addr.sin_addr=baddr->sin_addr;
                    sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
                }
            }
            freeifaddrs(ifap);
        }
    }
    log_event("Sent UDP broadcast: %s", payload);
}

void receive_broadcasts(int sockfd, Player players[], int *player_count, int my_listening_port) {
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

        if (magic && name && port_str && strlen(name)>0 && strcmp(magic, MAGIC_PACKET)==0) {
            int port=atoi(port_str);
            if (is_self(sender_addr.sin_addr, port, my_listening_port)) return;

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender_addr.sin_addr), ip, INET_ADDRSTRLEN);
            
            log_event("Received UDP broadcast from %s (IP: %s, Port: %d)", name, ip, port);

            int found=0;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            for (int i=0; i<*player_count; i++) {
                if (strcmp(players[i].ip_addr, ip)==0 && players[i].port==port) {
                    players[i].last_seen=now;
                    snprintf(players[i].name, sizeof(players[i].name), "%s", name);
                    if (!players[i].active) players[i].active=1;
                    found=1;
                    break;
                }
            }

            if (!found && *player_count<MAX_PLAYERS) {
                players[*player_count].id=(*player_count)+1;
                snprintf(players[*player_count].name, sizeof(players[*player_count].name), "%s", name);
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
    
    int new_count=0;
    for (int i=0; i<*player_count; i++) {
        long elapsed_sec=now.tv_sec-players[i].last_seen.tv_sec;
        if (players[i].active && elapsed_sec<5) {
            players[new_count]=players[i];
            players[new_count].id=new_count+1;
            new_count++;
        }
        else if (players[i].active) {
            log_event("Player %s (ID: %d) timed out after %ld seconds of inactivity.", players[i].name, players[i].id, elapsed_sec);
        }
    }
    *player_count=new_count;
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

void drain_broadcast_socket(int sockfd) {
    struct pollfd pfd={sockfd, POLLIN, 0};
    char dummy[512];
    while (poll(&pfd, 1, 0)>0 && (pfd.revents & POLLIN)) {
        recvfrom(sockfd, dummy, sizeof(dummy), 0, NULL, NULL);
    }
}