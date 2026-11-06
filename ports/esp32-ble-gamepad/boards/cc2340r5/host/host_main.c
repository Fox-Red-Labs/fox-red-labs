/* Host glue: prints the same FRL-GP1 block the board prints, using the same core files. */
#include <stdio.h>
#include <time.h>
#include "../core/frl_gp_check.h"
void frl_out(const char *s) { fputs(s, stdout); }
uint32_t frl_micros(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000u); }
uint32_t frl_heap_free(void) { return 0; }
uint32_t frl_uptime_ms(void) { return frl_micros() / 1000u; }
int main(void) { if (frl_gp_check_init() != 0) { puts("descriptor build failed"); return 1; } frl_gp_check_print(); return 0; }
