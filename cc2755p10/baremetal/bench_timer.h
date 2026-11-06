/*
 * bench_timer.h: CPU cycle counter for the bare-metal CC2755P10 benchmark (unchanged from the CC2340R5 bench; SysTick exists on the Cortex-M33 too).
 *
 * Same method as mspm0g3507/mcu-bench/sdk-common/bench_timer.h. The Cortex-M0+ has no
 * DWT cycle counter, so SysTick runs on the CPU clock (CLKSOURCE = 1), counting down from
 * 0xFFFFFF, and its interrupt counts each wrap. bench_cycles() returns a 64-bit count of CPU
 * cycles since bench_timer_start(). At 48 MHz one wrap is 0.35 s.
 *
 * The handler is installed with HwiP_plug (driverlib IntRegister: the vector table is copied
 * to RAM once). The F3 SDK's NoRTOS ClockP runs on SYSTIM, not SysTick, so nothing else uses it.
 */
#ifndef BENCH_TIMER_H_
#define BENCH_TIMER_H_
#include <stdint.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(cmsis/device.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include <ti/drivers/dpl/HwiP.h>

extern volatile uint32_t bench_wraps;

static inline void bench_timer_start(void)
{
	SysTick->CTRL = 0;
	SysTick->LOAD = 0xFFFFFFu;
	SysTick->VAL = 0;
	bench_wraps = 0;
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
