#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include "logger.h"

bool logging_enabled = false;

void init_logger(bool enable) {
    logging_enabled = enable;
}

void log_event(const char *format, ...) {
    if (!logging_enabled) return;

    char time_buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;

    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    FILE *log_file = fopen("log.txt", "a");
    if (log_file!=NULL) {
        fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, (long)tv.tv_usec);
        
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fclose(log_file);
    }
}