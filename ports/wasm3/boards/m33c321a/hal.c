// Board layer for the LP-MSPM33C321A: LED on PA2 (blue), SysTick microsecond timer, console into a RAM log.
// The Education BoosterPack covers the serial pins on this bench, so text goes to frl_log and the results to frl_result;
// a debugger reads both. The clock stays at the 32 MHz oscillator the chip starts on (no SysConfig output is used).
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "../../common/frl_hal.h"

#define LED_PORT GPIOA
#define LED_PIN  DL_GPIO_PIN_2
#define LED_IOMUX IOMUX_PINCM3

#define CPU_HZ 32000000u
char frl_log[4096];
volatile uint32_t frl_log_len;

uint32_t hal_cpu_hz(void) { return CPU_HZ; }
static volatile uint32_t tick_ms;
void SysTick_Handler(void) { tick_ms++; }
uint32_t hal_us(void) {
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (CPU_HZ / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }
void hal_putc(int c) { uint32_t n = frl_log_len; if (n < sizeof frl_log - 1) { frl_log[n] = (char)c; frl_log_len = n + 1; } }
void hal_led(int on) { if (on) DL_GPIO_setPins(LED_PORT, LED_PIN); else DL_GPIO_clearPins(LED_PORT, LED_PIN); }
void hal_init(void) {
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);
    for (volatile int i = 0; i < 64; i++) {}   // power-up settle, no DWT cycle counter needed
    SysTick->LOAD = CPU_HZ / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    DL_GPIO_initDigitalOutput(LED_IOMUX);
    DL_GPIO_clearPins(LED_PORT, LED_PIN);
    DL_GPIO_enableOutput(LED_PORT, LED_PIN);
}
