/* Three ways, shared part: the app logic. Plain C, no vendor headers.
 * Each way (DriverLib, TI Drivers, Zephyr) implements the five glue calls below in one file. */
#ifndef APP_H
#define APP_H

#include <stdint.h>

/* Glue: one implementation per way. */
void board_init(void);
uint16_t adc_read(void);                      /* one 12-bit conversion, raw counts */
void uart_write(const char *s, uint32_t len); /* blocking, backchannel UART, 115200 8N1 */
void led_toggle(void);
void sleep_ms(uint32_t ms);

/* Shared logic. */
#define APP_AVG_N 4u
typedef struct {
    uint16_t buf[APP_AVG_N];
    uint32_t sum;
    uint8_t i, n;
} app_avg_t;

uint16_t app_avg_push(app_avg_t *f, uint16_t x);  /* 4-sample moving average */
uint32_t app_format(char *out, uint32_t t, uint16_t raw, uint16_t avg); /* "t=<n> adc=<raw> avg=<avg>\r\n" */
void app_step(uint32_t t);                       /* read, filter, print, toggle */
void app_run(void);                              /* once a second, forever */

#endif
