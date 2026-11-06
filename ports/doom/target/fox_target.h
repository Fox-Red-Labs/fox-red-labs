/* Force-included into every engine file in the M33 build (-include fox_target.h).
 * GPL-2.0, see ../LICENSE. No files and no stdio streams on the target:
 * fopen finds nothing, printf and fprintf go to a small log hook. */
#ifndef FOX_TARGET_H
#define FOX_TARGET_H
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

int fox_printf(const char *fmt, ...);
int fox_vprintf(const char *fmt, va_list ap);

#undef fopen
#define fopen(p, m) ((FILE *)0)
#undef printf
#define printf fox_printf
#undef fprintf
#define fprintf(f, ...) fox_printf(__VA_ARGS__)
#undef vfprintf
#define vfprintf(f, fmt, ap) fox_vprintf(fmt, ap)
#undef fflush
#define fflush(f) 0
#endif
