/* Linux platform for the h264 harness: plat_ticks() is CLOCK_MONOTONIC in nanoseconds (cyc= fields are ns). */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "h264_run.h"
extern int h264_main(void);
uint32_t plat_ticks(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000000000ull + t.tv_nsec); }
void plat_log(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); fflush(stdout); }
int main(void) { return h264_main(); }
