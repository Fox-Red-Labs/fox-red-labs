/* Pico SDK on MSPM0: pico/time.h, the sleep calls. Time base: SysTick, see hardware/timer.h. */
#ifndef PICO_MSPM0_TIME_H
#define PICO_MSPM0_TIME_H
#include "hardware/timer.h"
void sleep_us(uint64_t us);
void sleep_ms(uint32_t ms);
#endif
