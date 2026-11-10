/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Fox Red Labs. All rights reserved. BSD-3-Clause, see portability/hobby/LICENSE.
 *
 * Hobby build 3, parking sensor. An RCWL-1601 (HC-SR04 protocol, 3.3 V) is triggered ten times a second; the echo
 * pulse is timed in microseconds on the free-running SysTick counter, and distance in cm = echo us / 58. The
 * distance goes out on the serial port, and a passive buzzer beeps faster as the object comes closer.
 */
#include "hobby.h"

#define MEASURE_MS 100u
#define ECHO_WAIT_US 30000u
#define BEEP_FREQ_HZ 2000u
#define BEEP_ON_MS 40u
#define BEEP_MAX_CM 100u

static int measure_cm(void)
{
    hb_out(HB_TRIG, 1);
    hb_delay_us(12);
    hb_out(HB_TRIG, 0);
    uint32_t t0 = hb_us();
    while (!hb_in(HB_ECHO)) {
        if ((uint32_t) (hb_us() - t0) > ECHO_WAIT_US) {
            return -1;
        }
    }
    uint32_t rise = hb_us();
    while (hb_in(HB_ECHO)) {
        if ((uint32_t) (hb_us() - rise) > ECHO_WAIT_US) {
            return -1;
        }
    }
    return (int) ((hb_us() - rise) / 58u);
}

void app_run(void)
{
    uint32_t t_meas = hb_ms(), t_beep = 0, t_off = 0;
    int cm = -1, beeping = 0;
    hb_out(HB_TRIG, 0);
    hb_pwm(HB_BUZZ, 1000000u / BEEP_FREQ_HZ, 0u);
    hb_printf("parking-sensor ready\r\n");
    for (;;) {
        uint32_t now = hb_ms();
        if ((uint32_t) (now - t_meas) >= MEASURE_MS) {
            t_meas += MEASURE_MS;
            cm = measure_cm();
            if (cm < 0) {
                hb_printf("dist=none\r\n");
            } else {
                hb_printf("dist=%u cm\r\n", (uint32_t) cm);
            }
        }
        now = hb_ms();
        if (beeping && (uint32_t) (now - t_off) < 0x80000000u) {
            hb_pwm_off(HB_BUZZ);
            beeping = 0;
        }
        if (cm >= 0 && (uint32_t) cm < BEEP_MAX_CM && !beeping && (uint32_t) (now - t_beep) < 0x80000000u) {
            /* gap between beeps: 60 ms at 0 cm, 560 ms at 100 cm */
            hb_pwm(HB_BUZZ, 1000000u / BEEP_FREQ_HZ, 500000u / BEEP_FREQ_HZ);
            beeping = 1;
            t_off = now + BEEP_ON_MS;
            t_beep = now + BEEP_ON_MS + 60u + 5u * (uint32_t) cm;
        }
        hb_delay_ms(2);
    }
}
