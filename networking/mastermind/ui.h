#ifndef UI_H
#define UI_H

#define MAX_ATTEMPTS 12

void print_mastermind_board(const char *secret, const char guesses[][10], const char feedbacks[][10], int attempts);
void print_codebreaker_board(const char *revealed_secret, const char guesses[][10], const char feedbacks[][10], int attempts);
void display_disconnection_error(void);

#endif