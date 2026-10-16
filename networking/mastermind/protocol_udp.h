#ifndef PROTOCOL_UDP_H
#define PROTOCOL_UDP_H

#include <netinet/in.h>
#include "discovery.h"

#define CHUNK_SIZE 4

typedef struct {
    int msg_id;
    int seq_num;
    int total_chunks;
    int is_ack;
    char data[CHUNK_SIZE];
} UdpChunk;

int init_udp_game_socket(int port);
int accept_udp_challenge(int udp_sock, char *challenger_name, struct sockaddr_in *challenger_addr);
int send_udp_challenge(int udp_sock, Player *p, const char *my_name, struct sockaddr_in *target_addr);
void send_udp_challenge_response(int udp_sock, struct sockaddr_in *peer_addr, const char *response);

void play_mastermind_udp(int udp_sock, struct sockaddr_in *peer_addr, const char *opponent_name);
void play_codebreaker_udp(int udp_sock, struct sockaddr_in *peer_addr, const char *opponent_name);

#endif