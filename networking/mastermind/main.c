#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "discovery.h"
#include "protocol_tcp.h"
#include "protocol_udp.h"
#include "logger.h"

int main(int argc, char *argv[]) {
    bool enable_logging = false;
    bool cost_cutting=false;

    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--log") == 0) enable_logging=true;
        else if (strcmp(argv[i], "--cost-cutting") == 0) cost_cutting=true;
    }
    init_logger(enable_logging);
    log_event("Application initialized. Logging: %d, Cost-Cutting (UDP): %d", enable_logging, cost_cutting);

    char player_name[50];
    int listening_port=8080;

    printf("Enter your name: ");
    if (fgets(player_name, sizeof(player_name), stdin)!=NULL) {
        player_name[strcspn(player_name, "\n")]='\0';
    }

    int bcast_sock=init_broadcast_socket();
    int game_listen_sock = cost_cutting ? init_udp_game_socket(listening_port) : init_tcp_listener(listening_port);
    
    Player players[MAX_PLAYERS];
    int player_count=0;
    int pending_challenge_sock=-1;
    bool pending_udp_challenge=false;
    struct sockaddr_in pending_udp_addr;
    char pending_challenger_name[50]={0};
    int max_known_id=0;
    int my_id=0;

    log_event("Starting 2.5s network discovery phase...");
    
    struct timespec init_start, init_now;
    clock_gettime(CLOCK_MONOTONIC, &init_start);
    struct pollfd init_pfd={bcast_sock, POLLIN, 0};
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &init_now);
        long elapsed=(init_now.tv_sec-init_start.tv_sec)*1000 + (init_now.tv_nsec-init_start.tv_nsec)/1000000;
        if (elapsed>=2500) break;
        
        if (poll(&init_pfd, 1, 2500-elapsed)>0) {
            receive_broadcasts(bcast_sock, players, &player_count, &max_known_id, 0);
        }
    }
    my_id=max_known_id+1;
    log_event("Discovery complete. Self-assigned ID: %d", my_id);
    
    struct timespec last_bcast;
    clock_gettime(CLOCK_MONOTONIC, &last_bcast);

    send_broadcast(bcast_sock, player_name, listening_port, my_id);

    struct pollfd pfds[3];
    pfds[0].fd=bcast_sock;
    pfds[0].events=POLLIN;
    pfds[1].fd=game_listen_sock;
    pfds[1].events=POLLIN;
    pfds[2].fd=STDIN_FILENO;
    pfds[2].events=POLLIN;

    int needs_redraw=1;

    while (1) {
        if (needs_redraw) {
            print_players(players, player_count);
            if (pending_challenge_sock!=-1 || pending_udp_challenge) {
                printf("> %s challenged you! Accept? (yes/no): ", pending_challenger_name);
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
                receive_broadcasts(bcast_sock, players, &player_count, &max_known_id, my_id);
                needs_redraw=1;
            }

            if (pfds[1].revents & POLLIN) {
                char challenger[50];
                if (cost_cutting) {
                    struct sockaddr_in c_addr;
                    if (accept_udp_challenge(game_listen_sock, challenger, &c_addr)==0) {
                        pending_udp_challenge=true;
                        pending_udp_addr=c_addr;
                        strncpy(pending_challenger_name, challenger, sizeof(pending_challenger_name)-1);
                        pending_challenger_name[sizeof(pending_challenger_name)-1]='\0';
                        needs_redraw=1;
                    }
                } else {
                    int game_sock=accept_challenge(game_listen_sock, challenger);
                    if (game_sock>=0) {
                        if (pending_challenge_sock!=-1) close(pending_challenge_sock);
                        pending_challenge_sock=game_sock;
                        strncpy(pending_challenger_name, challenger, sizeof(pending_challenger_name)-1);
                        pending_challenger_name[sizeof(pending_challenger_name)-1] = '\0';
                        needs_redraw=1;
                    }
                }
            }

            if (pfds[2].revents & POLLIN) {
                char input[100];
                if (fgets(input, sizeof(input), stdin)!=NULL) {
                    input[strcspn(input, "\n")]='\0';

                    if (cost_cutting && pending_udp_challenge) {
                        if (strcmp(input, "yes") == 0) {
                            send_udp_challenge_response(game_listen_sock, &pending_udp_addr, "yes");
                            printf("\n[Game Started (UDP Cost-Cutting)! You are the Codebreaker]\n");
                            play_codebreaker_udp(game_listen_sock, &pending_udp_addr, pending_challenger_name);
                            log_event("UDP Codebreaker game loop terminated. Returning to lobby.");
                        } else {
                            send_udp_challenge_response(game_listen_sock, &pending_udp_addr, "no");
                            printf("\n> Challenge rejected.\n");
                            sleep(1);
                        }
                        pending_udp_challenge=false;
                        needs_redraw=1;
                    } else if (!cost_cutting && pending_challenge_sock!=-1) {
                        if (strcmp(input, "yes")==0) {
                            send(pending_challenge_sock, "yes", 3, 0);
                            log_event("Sent TCP challenge response: yes (Transitioning to Codebreaker)");
                            printf("\n[Game Started! You are the Codebreaker]\n");
                            
                            play_codebreaker(pending_challenge_sock, pending_challenger_name);
                            log_event("Codebreaker game loop terminated. Returning to lobby.");
                        } else {
                            send(pending_challenge_sock, "no", 2, 0);
                            log_event("Sent TCP challenge response: no");
                            printf("\n> Challenge rejected.\n");
                            sleep(1);
                        }
                        close(pending_challenge_sock);
                        pending_challenge_sock=-1;
                        needs_redraw=1;
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
                                if (cost_cutting) {
                                    struct sockaddr_in target_addr;
                                    if (send_udp_challenge(game_listen_sock, target, player_name, &target_addr) == 0) {
                                        log_event("Transitioning to UDP Mastermind loop with ID %d (%s).", target->id, target->name);
                                        printf("\n[Game Started (UDP Cost-Cutting)! You are the Mastermind]\n");
                                        play_mastermind_udp(game_listen_sock, &target_addr, target->name);
                                        log_event("UDP Mastermind game loop terminated. Returning to lobby.");
                                    }
                                    else {
                                        printf("\n> Challenge failed or rejected.\n");
                                        sleep(1);
                                    }
                                }
                                else {
                                    int game_sock=send_challenge(target, player_name);
                                    if (game_sock>=0) {
                                        log_event("Transitioning to Mastermind loop with ID %d (%s).", target->id, target->name);
                                        printf("\n[Game Started! You are the Mastermind]\n");
                                        
                                        play_mastermind(game_sock, target->name);
                                        log_event("Mastermind game loop terminated. Returning to lobby.");
                                        close(game_sock);
                                    } else {
                                        printf("\n> Challenge failed or rejected.\n");
                                        sleep(1);
                                    }
                                }
                            } else {
                                printf("\n> Invalid player ID.\n");
                                sleep(1);
                            }
                        }
                        needs_redraw=1;
                    }
                }
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
    close(game_listen_sock);
    return 0;
}