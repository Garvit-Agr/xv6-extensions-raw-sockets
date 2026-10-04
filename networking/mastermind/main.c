#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "engine.h"
#include "discovery.h"
#include "protocol_tcp.h"

int main(int argc, char *argv[]) {

    char player_name[50];
    int listening_port=8080;

    printf("Enter your name: ");
    if (fgets(player_name, sizeof(player_name), stdin)!=NULL) {
        player_name[strcspn(player_name, "\n")]='\0';
    }

    int bcast_sock=init_broadcast_socket();
    int tcp_sock=init_tcp_listener(listening_port);
    
    Player players[MAX_PLAYERS];
    int player_count=0;

    struct timespec last_bcast;
    clock_gettime(CLOCK_MONOTONIC, &last_bcast);

    send_broadcast(bcast_sock, player_name, listening_port);

    struct pollfd pfds[3];
    pfds[0].fd=bcast_sock;
    pfds[0].events=POLLIN;
    pfds[1].fd=tcp_sock;
    pfds[1].events=POLLIN;
    pfds[2].fd=STDIN_FILENO;
    pfds[2].events=POLLIN;

    int needs_redraw=1;

    while (1) {
        if (needs_redraw) {
            print_players(players, player_count);
            needs_redraw=0;
        }

        int ret=poll(pfds, 3, 1000);

        if (ret>0) {
            if (pfds[0].revents & POLLIN) {
                receive_broadcasts(bcast_sock, players, &player_count);
                needs_redraw=1;
            }

            if (pfds[1].revents & POLLIN) {
                char challenger[50];
                int game_sock=accept_challenge(tcp_sock, challenger);
                if (game_sock>=0) {
                    printf("\n[Game Started! You are the Mastermind]\n");
                    sleep(2);
                    close(game_sock);
                }
                needs_redraw=1;
            }

            if (pfds[2].revents & POLLIN) {
                char input[100];
                if (fgets(input, sizeof(input), stdin)!=NULL) {
                    input[strcspn(input, "\n")]='\0';
                    if (strncmp(input, "challenge ", 10)==0) {
                        int target_id=atoi(input+10);
                        Player *target=NULL;
                        
                        for (int i=0; i<player_count; i++) {
                            if (players[i].active && players[i].id==target_id) {
                                target=&players[i];
                                break;
                            }
                        }
                        
                        if (target) {
                            int game_sock=send_challenge(target, player_name);
                            if (game_sock>=0) {
                                printf("\n[Game Started! You are the Codebreaker]\n");
                                sleep(2);
                                close(game_sock);
                            }
                        } else {
                            printf("\n> Invalid player ID.\n");
                        }
                        sleep(2);
                    }
                }
                needs_redraw=1;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        long elapsed_ms=(now.tv_sec-last_bcast.tv_sec)*1000+(now.tv_nsec-last_bcast.tv_nsec)/1000000;
        
        if (elapsed_ms>=2000) {
            send_broadcast(bcast_sock, player_name, listening_port);
            last_bcast=now;
        }

        cleanup_stale_players(players, &player_count);
    }

    close(bcast_sock);
    close(tcp_sock);
    return 0;
}