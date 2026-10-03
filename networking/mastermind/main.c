#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "discovery.h"

int main(int argc, char *argv[]) {
    
    char player_name[50];
    int listening_port=8080;

    printf("Enter your name: ");
    if (fgets(player_name, sizeof(player_name), stdin)!=NULL) {
        player_name[strcspn(player_name, "\n")]='\0';
    }

    int bcast_sock=init_broadcast_socket();
    struct timespec last_bcast;
    clock_gettime(CLOCK_MONOTONIC, &last_bcast);

    send_broadcast(bcast_sock, player_name, listening_port);

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        long elapsed_ms=(now.tv_sec-last_bcast.tv_sec)*1000+(now.tv_nsec-last_bcast.tv_nsec)/1000000;
        
        if (elapsed_ms>=2000) {
            send_broadcast(bcast_sock, player_name, listening_port);
            last_bcast=now;
        }

        usleep(100000);
    }

    close(bcast_sock);
    return 0;
}