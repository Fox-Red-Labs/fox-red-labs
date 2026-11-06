#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "h264_run.h"
extern int h264_main(void);
uint32_t plat_ticks(void) { return 0; }
void plat_log(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
int main(void) { return h264_main(); }
