#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>

bool validate_sequence(const char *seq);
void get_feedback(const char *master, const char *attempt, char *feedback);

#endif