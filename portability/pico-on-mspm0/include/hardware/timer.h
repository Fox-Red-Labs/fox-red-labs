/* Pico SDK on MSPM0: hardware/timer.h. The Pico has a 64-bit microsecond timer; here SysTick interrupts each millisecond and the
 * microseconds inside the millisecond come from its down-counter. Started before main() (src/runtime.c). */
#ifndef PICO_MSPM0_TIMER_H
#define PICO_MSPM0_TIMER_H
#include "pico/types.h"
uint64_t time_us_64(void);
void busy_wait_us(uint64_t us);
#endif
