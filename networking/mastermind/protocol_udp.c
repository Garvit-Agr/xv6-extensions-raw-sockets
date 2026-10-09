#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include "protocol_udp.h"
#include "engine.h"
#include "ui.h"
#include "logger.h"

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
    socklen_t addr_len=sizeof(*challenger_addr);
    memset(challenger_name, 0, 50);

    ssize_t n=recvfrom(udp_sock, challenger_name, 49, 0, (struct sockaddr *)challenger_addr, &addr_len);
    if (n<=0) return -1;
    challenger_name[n]='\0';

    log_event("Received UDP challenge request from %s", challenger_name);
    return 0;
}

void send_udp_challenge_response(int udp_sock, struct sockaddr_in *peer_addr, const char *response) {
    sendto(udp_sock, response, strlen(response), 0, (struct sockaddr *)peer_addr, sizeof(*peer_addr));
    log_event("Sent UDP challenge response: %s", response);
}

int send_udp_challenge(int udp_sock, Player *p, const char *my_name, struct sockaddr_in *target_addr) {
    memset(target_addr, 0, sizeof(*target_addr));
    target_addr->sin_family=AF_INET;
    target_addr->sin_port=htons(p->port);
    inet_pton(AF_INET, p->ip_addr, &target_addr->sin_addr);

    sendto(udp_sock, my_name, strlen(my_name), 0, (struct sockaddr *)target_addr, sizeof(*target_addr));
    log_event("Sent UDP challenge request to %s (%s:%d)", p->name, p->ip_addr, p->port);

    struct pollfd pfd={udp_sock, POLLIN, 0};
    if (poll(&pfd, 1, 10000)<=0) {
        log_event("UDP challenge request to %s timed out.", p->name);
        return -1;
    }

    char response[16];
    memset(response, 0, sizeof(response));
    struct sockaddr_in from_addr;
    socklen_t from_len=sizeof(from_addr);
    ssize_t n=recvfrom(udp_sock, response, sizeof(response) - 1, 0, (struct sockaddr *)&from_addr, &from_len);
    if (n<=0) return -1;
    response[n]='\0';

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
        memset(guess, 0, sizeof(guess));
        struct sockaddr_in from_addr;
        socklen_t from_len=sizeof(from_addr);

        ssize_t n=recvfrom(udp_sock, guess, sizeof(guess) - 1, 0, (struct sockaddr *)&from_addr, &from_len);
        if (n<=0) {
            log_event("UDP connection lost while waiting for guess from %s.", opponent_name);
            display_disconnection_error(opponent_name);
            return;
        }
        guess[n]='\0';
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

        if (sendto(udp_sock, feedback, strlen(feedback), 0, (struct sockaddr *)peer_addr, sizeof(*peer_addr))<0) {
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

    sendto(udp_sock, secret, 5, 0, (struct sockaddr *)peer_addr, sizeof(*peer_addr));
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

            if (sendto(udp_sock, guess, strlen(guess), 0, (struct sockaddr *)peer_addr, sizeof(*peer_addr))<0) {
                log_event("UDP error while sending guess to %s.", opponent_name);
                display_disconnection_error(opponent_name);
                return;
            }
            log_event("Sent UDP guess to Mastermind (%s): %s", opponent_name, guess);

            printf("> Waiting for Mastermind's feedback...\n");
            char feedback[10];
            memset(feedback, 0, sizeof(feedback));
            struct sockaddr_in from_addr;
            socklen_t from_len=sizeof(from_addr);
            ssize_t n=recvfrom(udp_sock, feedback, 5, 0, (struct sockaddr *)&from_addr, &from_len);
            if (n<=0) {
                log_event("UDP connection lost while waiting for feedback from %s.", opponent_name);
                display_disconnection_error(opponent_name);
                return;
            }
            feedback[n]='\0';
            log_event("Received UDP feedback from Mastermind (%s): %s", opponent_name, feedback);

            if (!validate_feedback(feedback)) {
                log_event("Received malformed UDP feedback from Mastermind (%s): %s", opponent_name, feedback);
            }

            strcpy(guesses[attempts], guess);
            strcpy(feedbacks[attempts], feedback);
            attempts++;

            print_codebreaker_board(NULL, guesses, feedbacks, attempts);

            if (strcmp(feedback, "xxxxx")==0 || attempts==MAX_ATTEMPTS) {
                ssize_t m=recvfrom(udp_sock, revealed_secret, 5, 0, (struct sockaddr *)&from_addr, &from_len);
                if (m<=0) {
                    log_event("UDP connection lost while receiving master sequence from %s.", opponent_name);
                    display_disconnection_error(opponent_name);
                    return;
                }
                revealed_secret[m]='\0';
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