/*
 * Board layer for uLisp on the LP-EM-CC2755P10 (CC2755P10, Cortex-M33 96 MHz, 162 kB RAM), TI Arm Clang, SimpleLink Low Power F3 SDK, NoRTOS.
 * Implements ports/ulisp/shim/frl_hal.h: the back-channel UART2 at 115200 (receive ring in the driver), SysTick at 1 kHz for
 * milliseconds and its down-counter for microseconds, two LEDs (Arduino pins 13 = red DIO16, 14 = green DIO12).
 * main() is the bench's main_nortos.c (cc2755p10/bench/board), unchanged; it calls mainThread() below.
 */
#include <stdint.h>
#include <stddef.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>
#include "ti_drivers_config.h"
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(cmsis/device.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include "frl_hal.h"

static UART2_Handle uart;
static uint32_t cpu_hz = 96000000;
static volatile uint32_t tick_ms;
static uint8_t led_out[2];
/* On the CC27xx the GPIO driver index is the DIO number (LED0 = DIO16, LED1 = DIO12), not 0 and 1: index 1 is the UART TX pin. */
static const uint_least8_t led_idx[2] = { CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_1 };

static void systick_isr(void) { tick_ms++; }
uint32_t hal_cpu_hz(void) { return cpu_hz; }
uint32_t hal_ms(void) { return tick_ms; }
uint32_t hal_us(void)
{
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (cpu_hz / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

void hal_putc(int c)
{
    uint8_t b = (uint8_t)c;
    size_t w;
    UART2_write(uart, &b, 1, &w);
}
int hal_getc(void)
{
    uint8_t b;
    size_t n = 0;
    UART2_readTimeout(uart, &b, 1, &n, 0);
    return n == 1 ? b : -1;
}

int hal_pin_count(void) { return 2; }
void hal_pin_mode(int i, int mode)
{
    if (i < 0 || i > 1) return;
    led_out[i] = (mode == 1);
    if (mode == 1) GPIO_setConfig(led_idx[i], GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    else if (mode == 2) GPIO_setConfig(led_idx[i], GPIO_CFG_IN_PU);
    else if (mode == 3) GPIO_setConfig(led_idx[i], GPIO_CFG_IN_PD);
    else GPIO_setConfig(led_idx[i], GPIO_CFG_IN_NOPULL);
}
void hal_pin_write(int i, int v) { if (i >= 0 && i <= 1 && led_out[i]) GPIO_write(led_idx[i], v ? 1 : 0); }
int hal_pin_read(int i) { return (i >= 0 && i <= 1) ? (int)GPIO_read(led_idx[i]) : 0; }

void hal_init(void)
{
    UART2_Params up;
    ClockP_FreqHz f;
    Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);
    Power_setConstraint(PowerLPF3_DISALLOW_IDLE);
    UART2_Params_init(&up);
    up.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &up);
    ClockP_getCpuFreq(&f);
    cpu_hz = f.lo;
    HwiP_plug(INT_SYSTICK, (void *)systick_isr);
    SysTick->CTRL = 0;
    SysTick->LOAD = cpu_hz / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    GPIO_setConfig(led_idx[0], GPIO_CFG_IN_NOPULL);
    GPIO_setConfig(led_idx[1], GPIO_CFG_IN_NOPULL);
#ifdef FRL_LED_OUT
    hal_pin_mode(0, 1);
#endif
}

int strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = *(const unsigned char *)a, y = *(const unsigned char *)b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y || x == 0) return x - y;
    }
}
