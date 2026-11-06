/* dataset-stream: see ds_stream.h. */
#include "ds_stream.h"

int ds_init(ds_sampler *s, uint32_t rate_hz, uint32_t now_us)
{
    if (rate_hz == 0 || rate_hz > 1000000u) return -1;
    s->rate_hz = rate_hz;
    s->period_us = 1000000u / rate_hz;
    s->start_us = now_us;
    s->next_us = now_us;   /* first sample is due at once, t_ms = 0 */
    s->taken = 0;
    s->overruns = 0;
    return 0;
}

int ds_due(ds_sampler *s, uint32_t now_us, uint32_t *t_ms)
{
    uint32_t late = now_us - s->next_us;          /* unsigned: wraps correctly */
    if (late >= 0x80000000u) return 0;            /* next_us is still in the future */
    if (late >= s->period_us) {                   /* missed whole periods: skip them, keep the grid */
        uint32_t skip = late / s->period_us;
        s->overruns += skip;
        s->next_us += skip * s->period_us;
    }
    /* Scheduled time since start, in ms, as 64-bit to survive the counter wrap. */
    uint64_t elapsed_us = (uint64_t)(s->taken + s->overruns) * s->period_us;
    *t_ms = (uint32_t)(elapsed_us / 1000u);
    s->next_us += s->period_us;
    s->taken++;
    return 1;
}

/* Append text; returns new position or 0 on overflow. */
static size_t put_str(char *buf, size_t cap, size_t pos, const char *t)
{
    while (*t) {
        if (pos + 1 >= cap) return 0;             /* keep room for a terminating zero */
        buf[pos++] = *t++;
    }
    buf[pos] = '\0';
    return pos;
}

static size_t put_u32(char *buf, size_t cap, size_t pos, uint32_t v)
{
    char tmp[10];
    unsigned n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    if (pos + n + 1 > cap) return 0;
    while (n) buf[pos++] = tmp[--n];
    buf[pos] = '\0';
    return pos;
}

static size_t put_i16(char *buf, size_t cap, size_t pos, int16_t v)
{
    uint32_t m;
    if (v < 0) {
        if (pos + 2 > cap) return 0;
        buf[pos++] = '-';
        m = (uint32_t)(-(int32_t)v);              /* -32768 is safe in 32 bits */
    } else {
        m = (uint32_t)v;
    }
    return put_u32(buf, cap, pos, m);
}

size_t ds_format_header(char *buf, size_t cap, const ds_sampler *s, const char *sensor,
                        const char *const *names, unsigned nch)
{
    size_t p = 0;
    if (cap == 0 || nch == 0 || nch > DS_MAX_CHANNELS) return 0;
    buf[0] = '\0';
    if (!(p = put_str(buf, cap, p, "# dataset-stream v1 rate_hz="))) return 0;
    if (!(p = put_u32(buf, cap, p, s->rate_hz))) return 0;
    if (!(p = put_str(buf, cap, p, " sensor="))) return 0;
    if (!(p = put_str(buf, cap, p, sensor))) return 0;
    if (!(p = put_str(buf, cap, p, "\nt_ms"))) return 0;
    for (unsigned i = 0; i < nch; i++) {
        if (!(p = put_str(buf, cap, p, ","))) return 0;
        if (!(p = put_str(buf, cap, p, names[i]))) return 0;
    }
    if (!(p = put_str(buf, cap, p, "\n"))) return 0;
    return p;
}

size_t ds_format_line(char *buf, size_t cap, uint32_t t_ms, const int16_t *v, unsigned nch)
{
    size_t p = 0;
    if (cap == 0 || nch == 0 || nch > DS_MAX_CHANNELS) return 0;
    buf[0] = '\0';
    if (!(p = put_u32(buf, cap, p, t_ms))) return 0;
    for (unsigned i = 0; i < nch; i++) {
        if (!(p = put_str(buf, cap, p, ","))) return 0;
        if (!(p = put_i16(buf, cap, p, v[i]))) return 0;
    }
    if (!(p = put_str(buf, cap, p, "\n"))) return 0;
    return p;
}

size_t ds_format_overrun(char *buf, size_t cap, uint32_t n)
{
    size_t p = 0;
    if (cap == 0) return 0;
    buf[0] = '\0';
    if (!(p = put_str(buf, cap, p, "# overrun "))) return 0;
    if (!(p = put_u32(buf, cap, p, n))) return 0;
    if (!(p = put_str(buf, cap, p, "\n"))) return 0;
    return p;
}

uint32_t ds_max_rate_hz(uint32_t baud, size_t line_len)
{
    if (line_len == 0) return 0;
    return (uint32_t)((baud / 10u) / line_len);
}
