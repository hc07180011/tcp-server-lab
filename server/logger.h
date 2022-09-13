#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CRITICAL "CRITICAL"
#define ERROR "ERROR"
#define WARNING "WARNING"
#define INFO "INFO"
#define DEBUG "DEBUG"

void logging(const char* tag, const char* message) {
    // tag: [CRITICAL] [ERROR] [WARNING] [INFO] [DEBUG]
    time_t now;
    time(&now);
    static char time_string[512];
    sprintf(time_string, "%s", ctime(&now));
    time_string[strlen(time_string) - 1] = '\0';
    fprintf(stderr, "%s [%s]: %s\n", time_string, tag, message);
}

#endif /* LOGGER_H */