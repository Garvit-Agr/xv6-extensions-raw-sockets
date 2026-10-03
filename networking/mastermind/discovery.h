#ifndef DISCOVERY_H
#define DISCOVERY_H

#define MAGIC_PACKET "MST1"
#define BROADCAST_PORT 8888

int init_broadcast_socket(void);
void send_broadcast(int sockfd, const char *name, int listening_port);

#endif