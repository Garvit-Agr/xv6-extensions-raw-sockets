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
    printf("Secret Sequence: %s\n", secret);
    printf("------------------------\n");
    for (int i = 0; i < attempts; i++) {
        printf("Attempt %2d: %s | Feedback: ", i + 1, guesses[i]);
        print_colored_feedback(feedbacks[i]);
        printf("\n");
    }
    printf("------------------------\n");
}

void print_codebreaker_board(const char guesses[][10], const char feedbacks[][10], int attempts) {
    printf("\033[2J\033[H");
    printf("=== CODEBREAKER BOARD ===\n");
    printf("-------------------------\n");
    for (int i = 0; i < attempts; i++) {
        printf("Attempt %2d: %s | Feedback: ", i + 1, guesses[i]);
        print_colored_feedback(feedbacks[i]);
        printf("\n");
    }
    printf("-------------------------\n");
}

void display_disconnection_error(void) {
    printf("\n\033[31m[!]\033[0m \033[1;31mNETWORK ERROR: The opponent abruptly disconnected.\033[0m\n");
    printf("> Gracefully terminating the match and returning to lobby in 3 seconds...\n");
    sleep(3);
}