#ifndef PROTOCOL_TCP_H
#define PROTOCOL_TCP_H

#include "discovery.h"

int init_tcp_listener(int port);
int accept_challenge(int listen_sock, char *challenger_name);
int send_challenge(Player *p, const char *my_name);

#endif