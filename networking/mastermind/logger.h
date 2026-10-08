#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

extern bool logging_enabled;

void init_logger(bool enable);
void log_event(const char *format, ...);

#endif