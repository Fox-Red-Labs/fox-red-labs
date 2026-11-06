/* The board side of the post-quantum join, compiled for the Mac: stdin and stdout stand in for the UART.
 *   board_host kat | timing | join | dsa_kat | dsa_timing
 * "Cycles" are nanoseconds from the host clock (cpu_hz 1e9), so the host numbers only prove the logic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include "pqc_join.h"
#include "pqc_sign.h"

static uint64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec; }
static void hrand(uint8_t *o, size_t n) { FILE *f = fopen("/dev/urandom", "rb"); if (!f || fread(o, 1, n, f) != n) exit(3); fclose(f); }
static void hputs(const char *s) { fputs(s, stdout); fflush(stdout); }
static int hgetline(char *buf, size_t max, uint32_t ms)
{
    size_t n = 0;
    for (;;) {
        fd_set fs; struct timeval tv = {ms / 1000, (ms % 1000) * 1000};
        FD_ZERO(&fs); FD_SET(0, &fs);
        if (select(1, &fs, NULL, NULL, &tv) <= 0) return -1;
        char c;
        if (read(0, &c, 1) != 1) exit(0);                  /* the Mac side closed the pipe: done */
        if (c == '\n') { buf[n] = 0; return (int)n; }
        if (n + 1 < max) buf[n++] = c;
    }
}
static void hpaint(void) {}
static uint32_t hused(void) { return 0; }
static void hresult(const char *t, uint64_t c, int ok, const char *note) { printf("R,pqc,%s,%llu,%s,%s\r\n", t, (unsigned long long)c, ok ? "ok" : "FAIL", note); fflush(stdout); }

int main(int argc, char **argv)
{
    pqc_hal_t h = {now_ns, 1000000000u, hrand, hputs, hgetline, hpaint, hused, hresult};
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "kat")) return pqc_kat(&h) ? 0 : 1;
    if (!strcmp(argv[1], "timing")) { pqc_timing(&h); return 0; }
    if (!strcmp(argv[1], "dsa_kat")) return pqc_sign_kat(&h) ? 0 : 1;
    if (!strcmp(argv[1], "dsa_timing")) { pqc_sign_timing(&h); return 0; }
    if (!strcmp(argv[1], "join")) { if (!pqc_kat(&h)) return 1; return pqc_join(&h) ? 0 : 1; }
    return 2;
}
