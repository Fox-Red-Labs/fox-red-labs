/*
 * bench_timer.h: CPU cycle counter for bare-metal MSPM0 benchmarks.
 *
 * The Cortex-M0+ has no DWT cycle counter, so this uses SysTick clocked from the
 * CPU clock (CLKSOURCE = 1), counting down from 0xFFFFFF, with an interrupt that
 * counts each wrap. bench_cycles() returns a 64-bit count of CPU cycles since
 * bench_timer_start(). Results are read out of RAM with the debugger.
 */
#ifndef BENCH_TIMER_H_
#define BENCH_TIMER_H_
#include <stdint.h>
#include <ti/devices/msp/msp.h>

volatile uint32_t bench_wraps; /* global so the debugger can find it */

void SysTick_Handler(void)
{
    bench_wraps++;
}

static inline void bench_timer_start(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0xFFFFFFu;
    SysTick->VAL  = 0;
    bench_wraps   = 0;
    NVIC_SetPriority(SysTick_IRQn, 0);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    __enable_irq();
}

static inline uint64_t bench_cycles(void)
{
    uint32_t hi, lo;
    do {
        hi = bench_wraps;
        lo = SysTick->VAL;
    } while (hi != bench_wraps);
    return ((uint64_t)hi << 24) + (0xFFFFFFu - lo);
}

#endif /* BENCH_TIMER_H_ */
