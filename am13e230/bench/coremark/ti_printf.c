/*
 * printf for TI's unmodified CoreMark port (benchmarks/coremark) when no debugger console is attached:
 * TI's coremark.h maps ee_printf to the C library printf, which writes through the debugger's console
 * (CIO). This replaces printf with a write into a RAM buffer, cm_log, that the debugger reads afterwards.
 * Nothing else of TI's port is changed.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define CM_LOG_SIZE 2048
char cm_log[CM_LOG_SIZE];
static unsigned cm_len;
/* never executed: its format string makes the linker pull the full printf (the build uses -fno-builtin-printf, so that
 * CoreMark's printf("Correct operation validated...\n") stays a printf call instead of becoming puts, which would skip the buffer) */
static void cm_force_full_printf(void) { char b[8]; snprintf(b, sizeof b, "%f %lu %04x %d %s", 1.0, 1ul, 1u, 1, "x"); }
int printf(const char *fmt, ...)
{
	if (cm_len == 0xFFFFFFFFu) cm_force_full_printf();
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(cm_log + cm_len, sizeof(cm_log) - cm_len, fmt, ap);
	va_end(ap);
	if (n > 0) { cm_len += (unsigned)n; if (cm_len >= sizeof(cm_log)) cm_len = sizeof(cm_log) - 1; }
	return n;
}
