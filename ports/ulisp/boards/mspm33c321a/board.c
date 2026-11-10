/*
 * Board layer for uLisp on the LP-MSPM33C321A (MSPM33C321A, Cortex-M33 160 MHz, 256 kB RAM), TI Arm Clang, MSPM33 SDK DriverLib, no RTOS.
 * Implements ports/ulisp/shim/frl_hal.h: the back-channel UART (UC12, PA10 TX, PA11 RX) at 115200 with a receive ring filled by the UART
 * interrupt, SysTick at 1 kHz for milliseconds and its down-counter for microseconds, two LEDs (Arduino pins 13 = red PC26, 14 = green PC27).
 * The clock tree is TI's sysctl_mclk_syspll example (160 MHz SYSPLL); ulisp.syscfg is that file plus the UART.
 */
#include <stdint.h>
#include <stddef.h>
#include "ti_msp_dl_config.h"
#include "frl_hal.h"

#define RXN 512
static volatile uint8_t rxbuf[RXN];
static volatile uint16_t rxh, rxt;
static volatile uint32_t tick_ms;
static uint8_t led_out[2];
static const uint32_t led_pin[2] = { GPIO_LEDS_LED_RED_PIN, GPIO_LEDS_LED_GREEN_PIN };
static const uint32_t led_mux[2] = { GPIO_LEDS_LED_RED_IOMUX, GPIO_LEDS_LED_GREEN_IOMUX };

void SysTick_Handler(void) { tick_ms++; }
void UART_0_INST_IRQHandler(void)
{
    while (!DL_UART_isRXFIFOEmpty(UART_0_INST)) {
        uint8_t b = DL_UART_receiveData(UART_0_INST);
        uint16_t n = (uint16_t)((rxh + 1) % RXN);
        if (n != rxt) { rxbuf[rxh] = b; rxh = n; }
    }
    DL_UART_clearInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
}

uint32_t hal_cpu_hz(void) { return CPUCLK_FREQ; }
uint32_t hal_ms(void) { return tick_ms; }
uint32_t hal_us(void)
{
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (CPUCLK_FREQ / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

void hal_putc(int c)
{
    while (DL_UART_isTXFIFOFull(UART_0_INST)) {}
    DL_UART_transmitData(UART_0_INST, (uint8_t)c);
}
int hal_getc(void)
{
    if (rxt == rxh) return -1;
    uint8_t b = rxbuf[rxt];
    rxt = (uint16_t)((rxt + 1) % RXN);
    return b;
}

int hal_pin_count(void) { return 2; }
void hal_pin_mode(int i, int mode)
{
    if (i < 0 || i > 1) return;
    led_out[i] = (mode == 1);
    /* The input stage stays on in every mode, so a pin set as output reads back its level. */
    DL_GPIO_RESISTOR r = mode == 2 ? DL_GPIO_RESISTOR_PULL_UP : mode == 3 ? DL_GPIO_RESISTOR_PULL_DOWN : DL_GPIO_RESISTOR_NONE;
    DL_GPIO_initDigitalInputFeatures(led_mux[i], DL_GPIO_INVERSION_DISABLE, r, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    if (mode == 1) { DL_GPIO_clearPins(GPIO_LEDS_PORT, led_pin[i]); DL_GPIO_enableOutput(GPIO_LEDS_PORT, led_pin[i]); }
    else DL_GPIO_disableOutput(GPIO_LEDS_PORT, led_pin[i]);
}
void hal_pin_write(int i, int v)
{
    if (i < 0 || i > 1 || !led_out[i]) return;
    if (v) DL_GPIO_setPins(GPIO_LEDS_PORT, led_pin[i]); else DL_GPIO_clearPins(GPIO_LEDS_PORT, led_pin[i]);
}
int hal_pin_read(int i) { return (i >= 0 && i <= 1) ? (DL_GPIO_readPins(GPIO_LEDS_PORT, led_pin[i]) ? 1 : 0) : 0; }

void hal_init(void)
{
    SYSCFG_DL_init();
    hal_pin_mode(0, 0); hal_pin_mode(1, 0);
#ifdef FRL_LED_OUT
    hal_pin_mode(0, 1);
#endif
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    SysTick->CTRL = 0;
    SysTick->LOAD = CPUCLK_FREQ / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
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
