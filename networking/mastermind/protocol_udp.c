#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#include "protocol_udp.h"
#include "engine.h"
#include "ui.h"
#include "logger.h"

static int send_udp_chunked(int sock, const char *msg, struct sockaddr_in *dest) {
    int len=strlen(msg);
    int total_chunks=(len+CHUNK_SIZE-1)/CHUNK_SIZE;
    if (total_chunks==0) total_chunks=1; 

    int acked[256]={0}; 
    struct timespec last_sent[256];
    memset(last_sent, 0, sizeof(last_sent));
    int chunks_acked=0;
    
    while (chunks_acked<total_chunks) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        for (int i=0; i<total_chunks; i++) {
            if (!acked[i]) {
                long elapsed_ms=0;
                if (last_sent[i].tv_sec!=0) {
                    elapsed_ms=(now.tv_sec-last_sent[i].tv_sec)*1000 + (now.tv_nsec-last_sent[i].tv_nsec)/1000000;
                }
                
                if (last_sent[i].tv_sec==0 || elapsed_ms>=100) {
                    UdpChunk chunk;
                    memset(&chunk, 0, sizeof(chunk));
                    chunk.seq_num=i;
                    chunk.total_chunks=total_chunks;
                    chunk.is_ack=0;
                    
                    int copy_len=CHUNK_SIZE;
                    if (i==total_chunks-1 && len%CHUNK_SIZE!=0) copy_len=len % CHUNK_SIZE;
                    
                    memcpy(chunk.data, msg+(i*CHUNK_SIZE), copy_len);

                    sendto(sock, &chunk, sizeof(chunk), 0, (struct sockaddr *)dest, sizeof(*dest));
                    
                    if (last_sent[i].tv_sec==0) log_event("Sent UDP chunk %d/%d", i+1, total_chunks);
                    else log_event("Retransmitted UDP chunk %d/%d (No ACK received within 0.1s)", i+1, total_chunks);

                    last_sent[i]=now;
                }
            }
        }
        
        struct pollfd pfd={sock, POLLIN, 0};
        if (poll(&pfd, 1, 10)>0) {
            UdpChunk recv_chunk;
            struct sockaddr_in from_addr;
            socklen_t from_len=sizeof(from_addr);
            
            ssize_t n=recvfrom(sock, &recv_chunk, sizeof(recv_chunk), 0, (struct sockaddr *)&from_addr, &from_len);
            if (n>0 && recv_chunk.is_ack && recv_chunk.seq_num<total_chunks && !acked[recv_chunk.seq_num]) {
                acked[recv_chunk.seq_num]=1;
                chunks_acked++;
                log_event("Received ACK for chunk %d/%d", recv_chunk.seq_num+1, total_chunks);
            }
        }
    }
    return 0;
}

static int recv_udp_chunked(int sock, char *buffer, int max_len, struct sockaddr_in *src) {
    memset(buffer, 0, max_len);
    int received_chunks=0;
    int expected_chunks=1;
    int first_packet_received=0;
    socklen_t src_len=sizeof(*src);
    
    UdpChunk chunk_buffer[256];
    int chunk_received[256]={0};

    while (received_chunks<expected_chunks) {
        struct pollfd pfd={sock, POLLIN, 0};
        if (poll(&pfd, 1, 30000)<=0) return -1;
        
        UdpChunk chunk;
        ssize_t n=recvfrom(sock, &chunk, sizeof(chunk), 0, (struct sockaddr *)src, &src_len);
        if (n<=0) return -1;

        if (chunk.is_ack) continue; 

        UdpChunk ack;
        memset(&ack, 0, sizeof(ack));
        ack.seq_num=chunk.seq_num;
        ack.is_ack=1;
        sendto(sock, &ack, sizeof(ack), 0, (struct sockaddr *)src, src_len);
        log_event("Sent ACK for chunk %d/%d", chunk.seq_num+1, chunk.total_chunks);

        if (!first_packet_received) {
            expected_chunks=chunk.total_chunks;
            first_packet_received=1;
        }
        
        if (chunk.seq_num < 256 && !chunk_received[chunk.seq_num]) {
            chunk_received[chunk.seq_num]=1;
            chunk_buffer[chunk.seq_num]=chunk;
            
            log_event("Buffered incoming UDP chunk %d/%d", chunk.seq_num+1, chunk.total_chunks);
            received_chunks++;
        }
    }
    
    int current_offset = 0;
    for (int i=0; i<expected_chunks; i++) {
        if (chunk_received[i]) {
            int copy_len=CHUNK_SIZE;
            if (current_offset + copy_len > max_len - 1) {
                copy_len = max_len - 1 - current_offset;
            }
            memcpy(buffer + current_offset, chunk_buffer[i].data, copy_len);
            current_offset += copy_len;
        }
    }
    
    log_event("Aggregated and reordered %d UDP chunks into contiguous string.", expected_chunks);
    return 1;
}

int init_udp_game_socket(int port) {
    int sockfd=socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))<0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int accept_udp_challenge(int udp_sock, char *challenger_name, struct sockaddr_in *challenger_addr) {
    if (recv_udp_chunked(udp_sock, challenger_name, 50, challenger_addr)<0) return -1;
    log_event("Received UDP challenge request from %s", challenger_name);
    return 0;
}

void send_udp_challenge_response(int udp_sock, struct sockaddr_in *peer_addr, const char *response) {
    send_udp_chunked(udp_sock, response, peer_addr);
    log_event("Sent UDP challenge response: %s", response);
}

int send_udp_challenge(int udp_sock, Player *p, const char *my_name, struct sockaddr_in *target_addr) {
    memset(target_addr, 0, sizeof(*target_addr));
    target_addr->sin_family=AF_INET;
    target_addr->sin_port=htons(p->port);
    inet_pton(AF_INET, p->ip_addr, &target_addr->sin_addr);

    send_udp_chunked(udp_sock, my_name, target_addr);
    log_event("Sent UDP challenge request to %s (%s:%d)", p->name, p->ip_addr, p->port);

    char response[16];
    if (recv_udp_chunked(udp_sock, response, sizeof(response), target_addr) < 0) {
        log_event("UDP challenge request to %s timed out.", p->name);
        return -1;
    }

    log_event("Received UDP challenge response from %s: %s", p->name, response);
    if (strcmp(response, "yes")==0) {
        return 0;
    }
    return -1;
}

void play_mastermind_udp(int udp_sock, struct sockaddr_in *peer_addr, const char *opponent_name) {
    char secret[100];
    char guesses[MAX_ATTEMPTS][10];
    char feedbacks[MAX_ATTEMPTS][10];
    int attempts=0;

    printf("\033[2J\033[H");
    while (1) {
        printf("Enter your 5-digit secret sequence: ");
        if (fgets(secret, sizeof(secret), stdin)!=NULL) {
            secret[strcspn(secret, "\n")]='\0';
            if (validate_sequence(secret)) break;
            printf("> Invalid sequence. Must be exactly 5 digits.\n");
        }
    }

    log_event("Mastermind (UDP) locked in secret sequence: %s", secret);
    print_mastermind_board(secret, guesses, feedbacks, attempts);
    printf("> Secret locked in. Waiting for Codebreaker's guesses...\n");

    while (attempts<MAX_ATTEMPTS) {
        char guess[10];
        
        if (recv_udp_chunked(udp_sock, guess, sizeof(guess), peer_addr) < 0) {
            log_event("UDP connection lost while waiting for guess from %s.", opponent_name);
            display_disconnection_error(opponent_name);
            return;
        }
        log_event("Received UDP guess from Codebreaker (%s): %s", opponent_name, guess);

        char auto_fb[10];
        memset(auto_fb, 0, sizeof(auto_fb));
        get_feedback(secret, guess, auto_fb);

        char fb_input[100];
        char feedback[10];
        while (1) {
            printf("> Codebreaker guessed %s. Press Enter to send feedback [%s] (or type 5-char feedback): ", guess, auto_fb);
            if (fgets(fb_input, sizeof(fb_input), stdin)!=NULL) {
                fb_input[strcspn(fb_input, "\n")]='\0';
                if (strlen(fb_input)==0) {
                    strcpy(feedback, auto_fb);
                } else {
                    strncpy(feedback, fb_input, 9);
                    feedback[9]='\0';
                }
                if (validate_feedback(feedback)) break;
                printf("> Invalid feedback. Must be 5 characters using only 'o', 'x', '-'.\n");
            }
        }

        if (send_udp_chunked(udp_sock, feedback, peer_addr) < 0) {
            log_event("UDP send error while sending feedback to %s.", opponent_name);
            display_disconnection_error(opponent_name);
            return;
        }
        log_event("Sent UDP feedback to Codebreaker (%s): %s", opponent_name, feedback);

        strcpy(guesses[attempts], guess);
        strcpy(feedbacks[attempts], feedback);
        attempts++;

        print_mastermind_board(secret, guesses, feedbacks, attempts);

        if (strcmp(feedback, "xxxxx")==0) {
            printf("> Codebreaker broke your sequence! Game Over.\n");
            break;
        } else if (attempts==MAX_ATTEMPTS) {
            printf("> Codebreaker ran out of attempts! You win.\n");
            break;
        } else {
            printf("> Waiting for Codebreaker's next guess...\n");
        }
    }

    send_udp_chunked(udp_sock, secret, peer_addr);
    log_event("Sent UDP master sequence to Codebreaker (%s) at game end: %s", opponent_name, secret);

    printf("> Press Enter to go home...");
    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin);
}

void play_codebreaker_udp(int udp_sock, struct sockaddr_in *peer_addr, const char *opponent_name) {
    char guesses[MAX_ATTEMPTS][10];
    char feedbacks[MAX_ATTEMPTS][10];
    char revealed_secret[10]={0};
    int attempts=0;

    print_codebreaker_board(NULL, guesses, feedbacks, attempts);

    while (attempts<MAX_ATTEMPTS) {
        char guess[100];
        printf("> Enter your 5-digit guess: ");

        if (fgets(guess, sizeof(guess), stdin)!=NULL) {
            guess[strcspn(guess, "\n")]='\0';
            if (!validate_sequence(guess)) {
                printf("> Invalid sequence. Must be exactly 5 digits.\n");
                continue;
            }

            if (send_udp_chunked(udp_sock, guess, peer_addr)<0) {
                log_event("UDP error while sending guess to %s.", opponent_name);
                display_disconnection_error(opponent_name);
                return;
            }
            log_event("Sent UDP guess to Mastermind (%s): %s", opponent_name, guess);

            printf("> Waiting for Mastermind's feedback...\n");
            char feedback[10];
            
            if (recv_udp_chunked(udp_sock, feedback, sizeof(feedback), peer_addr) < 0) {
                log_event("UDP connection lost while waiting for feedback from %s.", opponent_name);
                display_disconnection_error(opponent_name);
                return;
            }
            log_event("Received UDP feedback from Mastermind (%s): %s", opponent_name, feedback);

            if (!validate_feedback(feedback)) {
                log_event("Received malformed UDP feedback from Mastermind (%s): %s", opponent_name, feedback);
            }

            strcpy(guesses[attempts], guess);
            strcpy(feedbacks[attempts], feedback);
            attempts++;

            print_codebreaker_board(NULL, guesses, feedbacks, attempts);

            if (strcmp(feedback, "xxxxx")==0 || attempts==MAX_ATTEMPTS) {
                if (recv_udp_chunked(udp_sock, revealed_secret, sizeof(revealed_secret), peer_addr) < 0) {
                    log_event("UDP connection lost while receiving master sequence from %s.", opponent_name);
                    display_disconnection_error(opponent_name);
                    return;
                }
                log_event("Received UDP master sequence from Mastermind (%s): %s", opponent_name, revealed_secret);
                print_codebreaker_board(revealed_secret, guesses, feedbacks, attempts);

                if (strcmp(feedback, "xxxxx")==0) {
                    printf("> You broke the code! Game Over.\n");
                } else {
                    printf("> Out of attempts! Game Over.\n");
                }
                break;
            }
        }
    }

    printf("> Press Enter to go home...");
    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin);
}