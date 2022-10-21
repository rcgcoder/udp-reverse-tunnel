#include "misc.h"

#include <time.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * return timestamp in milliseconds
 * 
 * @return current system time in milliseconds
 */
uint64_t millisec() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}

/**
 * print to stdout, prefix with <level> for prety systemd log level coloring
 * 
 * @param level log levek
 * @param fmt format string for printf
 * @param ... args for printf
 */
void print(log_level_t level, char* fmt, ...) {
    printf("<%d>", level);
    va_list arglist;
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    puts("");
    fflush(stdout);
}