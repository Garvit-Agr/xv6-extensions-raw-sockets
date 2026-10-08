#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ui.h"

static void print_colored_feedback(const char *feedback) {
    for (int i = 0; i < (int)strlen(feedback); i++) {
        if (feedback[i] == 'x') printf("\033[32mx\033[0m"); // Green
        else if (feedback[i] == 'o') printf("\033[33mo\033[0m"); // Yellow
        else if (feedback[i] == '-') printf("\033[31m-\033[0m"); // Red
        else printf("%c", feedback[i]);
    }
}

void print_mastermind_board(const char *secret, const char guesses[][10], const char feedbacks[][10], int attempts) {
    printf("\033[2J\033[H");
    printf("=== MASTERMIND BOARD ===\n");
    for (int i=0; i<MAX_ATTEMPTS; i++) {
        if (i<attempts) {
            printf("%s   ", guesses[i]);
            print_colored_feedback(feedbacks[i]);
            printf("\n");
        }
        else printf("*****   *****\n");
    }
    printf("-----\n");
    printf("%s\n\n", secret);
}

void print_codebreaker_board(const char *revealed_secret, const char guesses[][10], const char feedbacks[][10], int attempts) {
    printf("\033[2J\033[H");
    printf("=== CODEBREAKER BOARD ===\n");
    for (int i=0; i<MAX_ATTEMPTS; i++) {
        if (i<attempts) {
            printf("%s   ", guesses[i]);
            print_colored_feedback(feedbacks[i]);
            printf("\n");
        }
        else printf("*****   *****\n");
    }
    printf("-----\n");
    if (revealed_secret != NULL && strlen(revealed_secret) == 5) printf("%s\n\n", revealed_secret);

    else printf("*****\n\n");
}

void display_disconnection_error(const char *opponent_name) {
    printf("\n%s disconnected. Press enter to go home.\n", opponent_name);
    fflush(stdout);
    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin);
}