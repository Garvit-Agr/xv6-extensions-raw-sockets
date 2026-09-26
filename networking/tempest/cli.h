#ifndef CLI_H
#define CLI_H
#include <stdbool.h>

typedef struct {
    char *city_name;
    bool raw_output;
} TempestConfig;

TempestConfig parse_arguments(int argc, char *argv[]);
void die(const char *msg);

#endif