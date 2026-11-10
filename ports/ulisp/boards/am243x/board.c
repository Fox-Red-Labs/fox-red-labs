/*
 * Board layer for uLisp on the LP-AM243 (AM2434, Cortex-R5F R5FSS0-0, 800 MHz, 2 MB on-chip RAM), TI Arm Clang, MCU+ SDK nortos.
 * Implements ports/ulisp/shim/frl_hal.h:
 *  - Console: UART0 (the XDS110 second port, 115200) by direct register access (THR/RHR at +0, LSR at +0x14): no driver ring, so a
 *    byte that arrives while a long computation runs can be lost once the 64-byte hardware FIFO is full.
 *  - Time: the SDK's ClockP (microseconds from the SoC timer); milliseconds are microseconds / 1000.
 *  - Pins: index 0 is the board LED on GPIO0 pin 22 (pad GPMC0_AD7, mux mode 7), index 1 is unused. Direct GPIO0 register access.
 */
#include <stdint.h>
#include <stddef.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <kernel/dpl/ClockP.h>
#include <drivers/pinmux.h>
#include "frl_hal.h"

#define UART0 0x02800000u
#define REG(a) (*(volatile uint32_t *)(a))
#define GPIO0 0x00600000u            /* bank 0 (pins 0 to 31): DIR01 +0x10, OUT_DATA01 +0x14, SET_DATA01 +0x18, CLR_DATA01 +0x1C, IN_DATA01 +0x20 */
#define LED_BIT (1u << 22)

uint32_t hal_cpu_hz(void) { return 800000000u; }
uint32_t hal_us(void) { return (uint32_t)ClockP_getTimeUsec(); }
uint32_t hal_ms(void) { return (uint32_t)(ClockP_getTimeUsec() / 1000u); }
void hal_delay_ms(uint32_t ms) { uint64_t e = ClockP_getTimeUsec() + (uint64_t)ms * 1000u; while (ClockP_getTimeUsec() < e) {} }

void hal_putc(int c)
{
    while (!(REG(UART0 + 0x14) & 0x20u)) {}
    REG(UART0) = (uint32_t)c & 0xFFu;
}
int hal_getc(void)
{
    if (REG(UART0 + 0x14) & 1u) return (int)(REG(UART0) & 0xFFu);
    return -1;
}

static uint8_t led_out;
int hal_pin_count(void) { return 1; }
void hal_pin_mode(int i, int mode)
{
    if (i != 0) return;
    led_out = (mode == 1);
    if (mode == 1) REG(GPIO0 + 0x10) &= ~LED_BIT; else REG(GPIO0 + 0x10) |= LED_BIT;   /* DIR: 0 = output */
}
void hal_pin_write(int i, int v) { if (i == 0 && led_out) REG(GPIO0 + (v ? 0x18 : 0x1C)) = LED_BIT; }
int hal_pin_read(int i) { return i == 0 ? ((REG(GPIO0 + 0x20) & LED_BIT) ? 1 : 0) : 0; }

void hal_init(void)
{
    static Pinmux_PerCfg_t led_pad[] = { { PIN_GPMC0_AD7, PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE }, { PINMUX_END, PINMUX_END } };
    Pinmux_config(led_pad, PINMUX_DOMAIN_ID_MAIN);
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
