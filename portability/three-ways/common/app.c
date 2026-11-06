/* Three ways, shared part. No vendor includes: this file builds unchanged for every way and on a Mac. */
#include "app.h"

uint16_t app_avg_push(app_avg_t *f, uint16_t x)
{
    if (f->n == APP_AVG_N) {
        f->sum -= f->buf[f->i];   /* drop the oldest sample */
    } else {
        f->n++;                   /* first three lines average what there is */
    }
    f->buf[f->i] = x;
    f->sum += x;
    f->i = (uint8_t)((f->i + 1u) % APP_AVG_N);
    return (uint16_t)(f->sum / f->n);
}

static char *put_u32(char *p, uint32_t v)
{
    char tmp[10];
    int k = 0;
    do {
        tmp[k++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (k > 0) {
        *p++ = tmp[--k];
    }
    return p;
}

static char *put_str(char *p, const char *s)
{
    while (*s != '\0') {
        *p++ = *s++;
    }
    return p;
}

uint32_t app_format(char *out, uint32_t t, uint16_t raw, uint16_t avg)
{
    char *p = out;
    p = put_str(p, "t=");
    p = put_u32(p, t);
    p = put_str(p, " adc=");
    p = put_u32(p, raw);
    p = put_str(p, " avg=");
    p = put_u32(p, avg);
    p = put_str(p, "\r\n");
    return (uint32_t)(p - out);
}

static app_avg_t avg;

void app_step(uint32_t t)
{
    char line[40];
    uint16_t raw = adc_read();
    uint16_t a = app_avg_push(&avg, raw);
    uart_write(line, app_format(line, t, raw, a));
    led_toggle();
}

void app_run(void)
{
    uint32_t t = 0;
    for (;;) {
        app_step(t++);
        sleep_ms(1000u);
    }
}
