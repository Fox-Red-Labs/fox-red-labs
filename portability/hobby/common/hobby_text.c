/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Fox Red Labs. All rights reserved. BSD-3-Clause, see portability/hobby/LICENSE.
 *
 * Tiny printf for the hobby builds: %u %d %s %c %%, an optional 0 flag and a width ("%02u", "%5d"). No floating
 * point, no heap, about 400 bytes of flash. Output goes to hb_puts one character-run at a time.
 */
#include <stdarg.h>
#include "hobby.h"

static char *put_num(char *end, uint32_t v)
{
    do {
        *--end = (char) ('0' + v % 10u);
        v /= 10u;
    } while (v);
    return end;
}

static void format(char *line, uint32_t cap, const char *fmt, va_list ap)
{
    char tmp[12];
    uint32_t n = 0;
    for (; *fmt && n < cap - 14; fmt++) {
        if (*fmt != '%') {
            line[n++] = *fmt;
            continue;
        }
        fmt++;
        char pad = ' ';
        uint32_t w = 0;
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            w = w * 10u + (uint32_t) (*fmt - '0');
            fmt++;
        }
        const char *s = 0;
        char *e = tmp + sizeof tmp, *b;
        uint32_t len, neg = 0;
        switch (*fmt) {
        case 'u':
            b = put_num(e, va_arg(ap, uint32_t));
            s = b;
            len = (uint32_t) (e - b);
            break;
        case 'd': {
            int32_t v = va_arg(ap, int32_t);
            neg = v < 0;
            b = put_num(e, neg ? (uint32_t) 0 - (uint32_t) v : (uint32_t) v);
            s = b;
            len = (uint32_t) (e - b);
            break;
        }
        case 's':
            s = va_arg(ap, const char *);
            for (len = 0; s[len]; len++) {
            }
            break;
        case 'c':
            tmp[0] = (char) va_arg(ap, int);
            s = tmp;
            len = 1;
            break;
        default:
            line[n++] = *fmt;
            continue;
        }
        if (neg && pad == '0') {
            line[n++] = '-';
            neg = 0;
            if (w) {
                w--;
            }
        }
        for (uint32_t i = len + neg; i < w && n < cap - 14; i++) {
            line[n++] = pad;
        }
        if (neg) {
            line[n++] = '-';
        }
        for (uint32_t i = 0; i < len && n < cap - 2; i++) {
            line[n++] = s[i];
        }
    }
    line[n] = 0;
}

void hb_snprintf(char *out, uint32_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    format(out, cap, fmt, ap);
    va_end(ap);
}

void hb_printf(const char *fmt, ...)
{
    char line[96];
    va_list ap;
    va_start(ap, fmt);
    format(line, sizeof line, fmt, ap);
    va_end(ap);
    hb_puts(line);
}
