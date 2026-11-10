/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Fox Red Labs. All rights reserved. BSD-3-Clause, see portability/hobby/LICENSE.
 *
 * Hobby builds, the portable layer. Each build's app.c includes only this header and stdint.h. One glue file per
 * software layer (driverlib/glue_dl.c) implements it; the pins come from the generated boards/<id>/hobby_pins.h.
 */
#ifndef HOBBY_H
#define HOBBY_H

#include <stdint.h>

/* Every signal any build uses. A build touches only the ones its PINS.md lists. */
typedef enum {
    HB_BTN_A, HB_BTN_B, HB_LED, HB_LED_RED, HB_LED_YEL, HB_LED_GRN, HB_BUZZ, HB_TRIG, HB_ECHO, HB_DHT,
    HB_SCL, HB_SDA, HB_SERVO1, HB_SERVO2, HB_PIR, HB_TM_CLK, HB_TM_DIO, HB_POT1, HB_POT2, HB_N
} hb_sig_t;

void hb_init(void);                                  /* clocks, pins, timers, ADC, UART at 115200 8N1 */
uint32_t hb_us(void);                                /* free-running microseconds, wraps at 2^32; call at least every 100 ms */
uint32_t hb_ms(void);                                /* free-running milliseconds, same rule */
void hb_delay_us(uint32_t us);
void hb_delay_ms(uint32_t ms);

void hb_out(hb_sig_t s, int level);                  /* push-pull output */
int hb_in(hb_sig_t s);                               /* 1 = high; buttons are active low */
void hb_od(hb_sig_t s, int level);                   /* 0 pulls the line low, 1 lets it go (pull-ups hold it high) */

uint16_t hb_adc(hb_sig_t pot);                       /* one 12-bit conversion of POT1 or POT2, 0 to 4095 */
void hb_pwm(hb_sig_t s, uint32_t period_us, uint32_t high_us); /* period 50 to 65000 us; high_us 0 = always low */
void hb_pwm_off(hb_sig_t s);

void hb_puts(const char *s);                         /* blocking, backchannel UART */
void hb_snprintf(char *out, uint32_t cap, const char *fmt, ...); /* same formats; output is cut to cap - 14 characters */
void hb_printf(const char *fmt, ...);                /* %u %d %s %c, optional 0 and width digits (%02u); see hobby_text.c */

/* Each build's app.c defines this; the glue's main() calls hb_init() and then app_run(). It never returns. */
void app_run(void);

#endif
