#ifndef UI_H
#define UI_H

#define MAX_ATTEMPTS 20

void print_mastermind_board(const char *secret, const char guesses[][10], const char feedbacks[][10], int attempts);
void print_codebreaker_board(const char guesses[][10], const char feedbacks[][10], int attempts);

#endif