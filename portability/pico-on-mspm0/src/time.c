/* Pico SDK on MSPM0: hardware/timer.h and pico/time.h. SysTick at 1 kHz counts milliseconds; the down-counter gives the
 * microseconds inside the current millisecond. */
#include "ti_msp_dl_config.h"
#include "hardware/timer.h"
#include "pico/time.h"
#define TICK (CPUCLK_FREQ / 1000u)
static volatile uint32_t s_ms;
void SysTick_Handler(void) { s_ms++; }
void pico_time_init(void) { SysTick_Config(TICK); }
uint64_t time_us_64(void)
{
    uint32_t ms, v;
    do { ms = s_ms; v = SysTick->VAL; } while (ms != s_ms);           /* reread if the tick fired in between */
    if (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk && v > TICK / 2) { ms++; }  /* counter wrapped, interrupt not yet taken */
    return (uint64_t) ms * 1000u + (TICK - 1u - v) / (CPUCLK_FREQ / 1000000u);
}
void busy_wait_us(uint64_t us) { uint64_t t = time_us_64() + us; while (time_us_64() < t) { } }
void sleep_us(uint64_t us) { busy_wait_us(us); }
void sleep_ms(uint32_t ms) { busy_wait_us((uint64_t) ms * 1000u); }
