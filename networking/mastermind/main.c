#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>

#include "engine.h"
#include "discovery.h"
#include "protocol_tcp.h"

int main(int argc, char *argv[]) {

    char player_name[50];
    int listening_port=8081;

    printf("Enter your name: ");
    if (fgets(player_name, sizeof(player_name), stdin)!=NULL) {
        player_name[strcspn(player_name, "\n")]='\0';
    }

    int bcast_sock=init_broadcast_socket();
    int tcp_sock=init_tcp_listener(listening_port);
    
    Player players[MAX_PLAYERS];
    int player_count=0;
    int pending_challenge_sock=-1;
    int max_known_id=0;
    int my_id=0;
    struct timespec init_start, init_now;
    clock_gettime(CLOCK_MONOTONIC, &init_start);
    struct pollfd init_pfd={bcast_sock, POLLIN, 0};
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &init_now);
        long elapsed=(init_now.tv_sec-init_start.tv_sec)*1000 + (init_now.tv_nsec-init_start.tv_nsec)/1000000;
        if (elapsed>=2500) break;
        
        if (poll(&init_pfd, 1, 2500-elapsed)>0) {
            receive_broadcasts(bcast_sock, players, &player_count, &max_known_id);
        }
    }
    my_id=max_known_id+1;
    
    struct timespec last_bcast;
    clock_gettime(CLOCK_MONOTONIC, &last_bcast);

    send_broadcast(bcast_sock, player_name, listening_port, my_id);

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
            if (pending_challenge_sock != -1) {
                printf("> Challenge pending! Accept? (yes/no): ");
                fflush(stdout);
            }
            needs_redraw=0;
        }

        struct timespec loop_start;
        clock_gettime(CLOCK_MONOTONIC, &loop_start);
        long elapsed_since_bcast=(loop_start.tv_sec-last_bcast.tv_sec)*1000 + (loop_start.tv_nsec-last_bcast.tv_nsec)/1000000;
        int poll_timeout = 2000 - elapsed_since_bcast;
        if (poll_timeout < 0) poll_timeout = 0;

        int ret=poll(pfds, 3, poll_timeout);

        if (ret>0) {
            if (pfds[0].revents & POLLIN) {
                receive_broadcasts(bcast_sock, players, &player_count, &max_known_id);
                needs_redraw=1;
            }

            if (pfds[1].revents & POLLIN) {
                char challenger[50];
                int game_sock=accept_challenge(tcp_sock, challenger);
                if (game_sock>=0) {
                    if (pending_challenge_sock!=-1) close(pending_challenge_sock);
                    pending_challenge_sock=game_sock;
                    needs_redraw=1;
                }
            }

            if (pfds[2].revents & POLLIN) {
                char input[100];
                if (fgets(input, sizeof(input), stdin)!=NULL) {
                    input[strcspn(input, "\n")]='\0';

                    if (pending_challenge_sock!=-1) {
                        if (strcmp(input, "yes")==0) {
                            send(pending_challenge_sock, "yes", 3, 0);
                            printf("\n[Game Started! You are the Mastermind]\n");
                            sleep(2);
                        } else {
                            send(pending_challenge_sock, "no", 2, 0);
                            printf("\n> Challenge rejected.\n");
                            sleep(1);
                        }
                        close(pending_challenge_sock);
                        pending_challenge_sock=-1;
                    } else if (strncmp(input, "challenge ", 10)==0) {
                        int target_id=atoi(input+10);
                        
                        if (target_id == my_id) {
                            printf("\n> You cannot challenge yourself.\n");
                            sleep(1);
                        } else {
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
                                } else {
                                    printf("\n> Challenge failed or rejected.\n");
                                    sleep(1);
                                }
                            } else {
                                printf("\n> Invalid player ID.\n");
                                sleep(1);
                            }
                        }
                    }
                }
                needs_redraw=1;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        long elapsed_ms=(now.tv_sec-last_bcast.tv_sec)*1000+(now.tv_nsec-last_bcast.tv_nsec)/1000000;
        
        if (elapsed_ms>=2000) {
            send_broadcast(bcast_sock, player_name, listening_port, my_id);
            last_bcast=now;
        }

        int active_before=0;
        for (int i=0; i<player_count; i++) if (players[i].active) active_before++;
        
        cleanup_stale_players(players, &player_count);
        
        int active_after=0;
        for (int i=0; i<player_count; i++) if (players[i].active) active_after++;
        
        if (active_before != active_after) needs_redraw=1;
    }

    close(bcast_sock);
    close(tcp_sock);
    return 0;
}