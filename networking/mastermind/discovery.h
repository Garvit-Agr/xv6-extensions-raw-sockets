#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <time.h>

#define MAGIC_PACKET "MST1"
#define BROADCAST_PORT 8888
#define MAX_PLAYERS 10

typedef struct {
    int id;
    char name[50];
    char ip_addr[16];
    int port;
    struct timespec last_seen;
    int active;
} Player;

int init_broadcast_socket(void);
void send_broadcast(int sockfd, const char *name, int listening_port, int my_id);
void receive_broadcasts(int sockfd, Player players[], int *player_count, int *max_known_id, int my_id);
void cleanup_stale_players(Player players[], int *player_count);
void print_players(Player players[], int player_count);

#endif