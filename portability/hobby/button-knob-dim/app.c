/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Fox Red Labs. All rights reserved. BSD-3-Clause, see portability/hobby/LICENSE.
 *
 * Hobby build 1, button and knob dimmer. A button toggles an LED (the board's S2, or a button of your own on the
 * breadboard wired to BTN_B: either one works), a potentiometer sets its brightness through
 * PWM (1 kHz), and the ADC value goes out on the serial port twice a second.
 */
#include "hobby.h"

#define PWM_PERIOD_US 1000u
#define DEBOUNCE_MS 20u
#define PRINT_MS 500u

void app_run(void)
{
    int led_on = 1, btn_prev = 1;
    uint32_t t_edge = 0, t_print = hb_ms();
    uint32_t duty_pct = 0, last_high = 0xFFFFFFFFu;
    hb_printf("button-knob-dim ready\r\n");
    for (;;) {
        uint16_t adc = hb_adc(HB_POT1);
        uint32_t now = hb_ms();
        int btn = hb_in(HB_BTN_A) && hb_in(HB_BTN_B);   /* low when either button is held */
        if (btn != btn_prev && (uint32_t) (now - t_edge) >= DEBOUNCE_MS) {
            t_edge = now;
            btn_prev = btn;
            if (!btn) { /* pressed: the button pulls the pin to ground */
                led_on = !led_on;
            }
        }
        uint32_t high_us = led_on ? ((uint32_t) adc * PWM_PERIOD_US) / 4095u : 0u;
        duty_pct = high_us * 100u / PWM_PERIOD_US;
        if (high_us != last_high) {
            last_high = high_us;
            hb_pwm(HB_LED, PWM_PERIOD_US, high_us);
        }
        if ((uint32_t) (now - t_print) >= PRINT_MS) {
            t_print += PRINT_MS;
            hb_printf("adc=%u led=%s duty=%u\r\n", (uint32_t) adc, led_on ? "on" : "off", duty_pct);
        }
        hb_delay_ms(5);
    }
}
