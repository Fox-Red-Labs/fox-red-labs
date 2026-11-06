/* Tiny serial log helpers shared by the three ports: no printf, no heap, no libc stdio (4 kB of RAM on the L1306).
 * The including file defines LOG_PUTC(c) and LOG_IDLE() for its board before this header. Written fresh for this port. */
#ifndef FRL_LOG_H
#define FRL_LOG_H
#include <stdint.h>

static inline void log_s(const char *s) { while (*s) LOG_PUTC((uint8_t) *s++); }

static inline void log_u(uint32_t v)
{
    char b[11]; int i = 10; b[i] = 0;
    do { b[--i] = (char) ('0' + v % 10); v /= 10; } while (v);
    log_s(&b[i]);
}

static inline void log_kv(const char *k, uint32_t v) { log_s(" "); log_s(k); log_s("="); log_u(v); }
static inline void log_nl(void) { log_s("\r\n"); LOG_IDLE(); }
#endif
