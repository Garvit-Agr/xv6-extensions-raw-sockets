#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol_tcp.h"
#include "engine.h"
#include "ui.h" 
#include "logger.h"

int init_tcp_listener(int port) {
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
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
    addr.sin_addr.s_addr=INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr))<0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(sockfd, 5)<0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    return sockfd;
}

int accept_challenge(int listen_sock, char *challenger_name) {
    struct sockaddr_in client_addr;
    socklen_t client_len=sizeof(client_addr);
    
    int client_sock=accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock<0) return -1;

    memset(challenger_name, 0, 50);
    recv(client_sock, challenger_name, 49, 0);
    log_event("Received TCP challenge request from %s", challenger_name);

    return client_sock;
}

int send_challenge(Player *p, const char *my_name) {
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd<0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(p->port);
    inet_pton(AF_INET, p->ip_addr, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))<0) {
        close(sockfd);
        printf("\n> Failed to connect to %s\n", p->name);
        return -1;
    }

    send(sockfd, my_name, strlen(my_name), 0);
    log_event("Sent TCP challenge request to %s (%s:%d)", p->name, p->ip_addr, p->port);
    
    char response[10];
    memset(response, 0, sizeof(response));
    recv(sockfd, response, sizeof(response)-1, 0);
    log_event("Received TCP challenge response from %s: %s", p->name, response);

    if (strcmp(response, "yes")==0) return sockfd;
    else {
        close(sockfd);
        return -1;
    }
}

void play_mastermind(int sock) {
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
    
    log_event("Mastermind locked in secret sequence: %s", secret);
    print_mastermind_board(secret, guesses, feedbacks, attempts);
    printf("> Secret locked in. Waiting for Codebreaker's guesses...\n");

    while (attempts<MAX_ATTEMPTS) {
        char guess[10];
        memset(guess, 0, sizeof(guess));     
        int n=recv(sock, guess, sizeof(guess)-1, 0);
        if (n<=0) {
            log_event("Connection lost while waiting for guess.");
            display_disconnection_error();
            return;
        }
        log_event("Received guess from Codebreaker: %s", guess);

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
                }
                else {
                    strncpy(feedback, fb_input, 9);
                    feedback[9]='\0';
                }
                if (validate_feedback(feedback)) break;
                printf("> Invalid feedback. Must be 5 characters using only 'o', 'x', '-'.\n");
            }
        }
        
        if (send(sock, feedback, strlen(feedback), 0)<0) {
            log_event("Connection lost while sending feedback.");
            display_disconnection_error();
            return;
        }
        log_event("Sent feedback to Codebreaker: %s", feedback);

        strcpy(guesses[attempts], guess);
        strcpy(feedbacks[attempts], feedback);
        attempts++;

        print_mastermind_board(secret, guesses, feedbacks, attempts);

        if (strcmp(feedback, "xxxxx")==0) {
            printf("> Codebreaker broke your sequence! Game Over.\n");
            break;
        }
        else if (attempts==MAX_ATTEMPTS) {
            printf("> Codebreaker ran out of attempts! You win.\n");
            break;
        }
        else printf("> Waiting for Codebreaker's next guess...\n");
    }

    if (send(sock, secret, 5, 0)>=0) log_event("Sent master sequence to Codebreaker at game end: %s", secret);

    printf("> Press Enter to go home...");
    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin);
}

void play_codebreaker(int sock) {
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

            if (send(sock, guess, strlen(guess), 0)<0) {
                log_event("Connection lost while sending guess.");
                display_disconnection_error();
                return;
            }
            log_event("Sent guess to Mastermind: %s", guess);
            
            printf("> Waiting for Mastermind's feedback...\n");
            char feedback[10];
            memset(feedback, 0, sizeof(feedback));
            int n=recv(sock, feedback, 5, 0);
            if (n<=0) {
                log_event("Connection lost while waiting for feedback.");
                display_disconnection_error();
                return;
            }
            feedback[n]='\0';
            log_event("Received feedback from Mastermind: %s", feedback);

            if (!validate_feedback(feedback)) log_event("Received malformed feedback from Mastermind: %s", feedback);

            strcpy(guesses[attempts], guess);
            strcpy(feedbacks[attempts], feedback);
            attempts++;

            print_codebreaker_board(NULL, guesses, feedbacks, attempts);

            if (strcmp(feedback, "xxxxx")==0 || attempts==MAX_ATTEMPTS) {
                int m=recv(sock, revealed_secret, 5, 0);
                if (m>0) {
                    revealed_secret[m]='\0';
                    log_event("Received master sequence from Mastermind: %s", revealed_secret);
                    print_codebreaker_board(revealed_secret, guesses, feedbacks, attempts);
                }
                if (strcmp(feedback, "xxxxx")==0) printf("> You broke the code! Game Over.\n");
                else printf("> Out of attempts! Game Over.\n");

                break;
            }
        }
    }

    printf("> Press Enter to go home...");
    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin);
}