/*
 * Bare-metal GPIO toggle and interrupt latency on the LP-MSPM0L1306, ported from the LP-MSPM0G3507
 * test in mspm0g3507/mcu-bench/baremetal/timing. Same method: tight loops timed by a cycle counter, best of 5;
 * interrupt latency by pending a spare IRQ (25, I2C1, unused here; the L1306 has no AES) and timing
 * to the handler's first line. Clock: SysConfig file of TI example gpio_toggle_output (32 MHz SYSOSC).
 * Timer: SysTick on the CPU clock (the Cortex-M0+ has no DWT cycle counter).
 * Results are left in RAM (struct tm) and read with the debugger.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "../common/bench_timer.h"

#define RUNS 5
#define SPARE_IRQ I2C1_INT_IRQn /* 25, I2C1 is not used by this program */
#define N 2000

volatile struct {
	uint32_t magic;            /* 0x7141E001 when done */
	uint32_t cpuclk_hz;
	uint32_t clkstatus;
	uint32_t timer_read;       /* x100 cycles per bench_cycles() call (loop of 1000) */
	uint32_t empty_loop;       /* x100 cycles per iteration */
	uint32_t gpio_toggle_loop; /* x100 cycles per iteration, DL_GPIO_togglePins(GPIOA, PIN_27) */
	uint32_t gpio_toggle_str16;/* x100 cycles per store, 16 back-to-back STR to DOUTTGL31_0 in asm */
	uint32_t systick_pair;     /* cycles between two back-to-back SysTick->VAL reads */
	uint32_t irq_best;         /* cycles, pend to handler's first line, raw (includes one VAL read) */
	uint32_t irq_worst;
	uint32_t irq_samples;
} tm;

static volatile uint32_t sink;
static volatile uint32_t isr_val;

void I2C1_IRQHandler(void)
{
	isr_val = SysTick->VAL;
}

#define TIME_LOOP(field, n, body)                                         \
	do {                                                              \
		uint32_t best = UINT32_MAX;                               \
		for (int r = 0; r < RUNS; r++) {                          \
			uint64_t t0 = bench_cycles();                     \
			for (uint32_t i = 0; i < (n); i++) {              \
				body;                                     \
			}                                                 \
			uint32_t dt = (uint32_t)(bench_cycles() - t0);    \
			if (dt < best) {                                  \
				best = dt;                                \
			}                                                 \
		}                                                         \
		tm.field = (uint32_t)(((uint64_t)best * 100U) / (n));     \
	} while (0)

static uint32_t toggle_str16(void)
{
	volatile uint32_t *reg = &GPIOA->DOUTTGL31_0;
	uint32_t pin = DL_GPIO_PIN_27, best = UINT32_MAX;

	for (int r = 0; r < RUNS; r++) {
		uint32_t a = SysTick->VAL;
		__asm volatile(
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			: : "l"(reg), "l"(pin) : "memory");
		uint32_t b = SysTick->VAL;
		uint32_t a2 = SysTick->VAL;
		uint32_t b2 = SysTick->VAL;
		if (a > b && a2 > b2) { /* no wrap */
			uint32_t dt = (a - b) - (a2 - b2);
			if (dt < best) {
				best = dt;
			}
		}
	}
	return (best * 100U) / 16U;
}

int main(void)
{
	SYSCFG_DL_init();
	/* PA27 (blue LED) is already an output in the gpio_toggle_output SysConfig. */
	DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_27);

	bench_timer_start();
	tm.cpuclk_hz = CPUCLK_FREQ;
	tm.clkstatus = SYSCTL->SOCLOCK.CLKSTATUS;

	TIME_LOOP(timer_read, 1000, sink = (uint32_t)bench_cycles());
	TIME_LOOP(empty_loop, 10000, __asm volatile("nop"));
	TIME_LOOP(gpio_toggle_loop, N, DL_GPIO_togglePins(GPIOA, DL_GPIO_PIN_27));
	tm.gpio_toggle_str16 = toggle_str16();
	DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_27);

	{
		uint32_t a = SysTick->VAL, b = SysTick->VAL;
		tm.systick_pair = a - b;
	}

	NVIC_SetPriority(SPARE_IRQ, 1);
	NVIC_EnableIRQ(SPARE_IRQ);
	{
		uint32_t best = UINT32_MAX, worst = 0, n = 0;

		for (int i = 0; i < 200; i++) {
			isr_val = 0xFFFFFFFFu;
			uint32_t t0 = SysTick->VAL;
			NVIC_SetPendingIRQ(SPARE_IRQ);
			__ISB();
			uint32_t t1 = isr_val;
			if (t1 == 0xFFFFFFFFu || t1 > t0) {
				continue; /* SysTick wrapped in between: skip */
			}
			uint32_t dt = t0 - t1;
			best = dt < best ? dt : best;
			worst = dt > worst ? dt : worst;
			n++;
		}
		tm.irq_best = best;
		tm.irq_worst = worst;
		tm.irq_samples = n;
	}
	NVIC_DisableIRQ(SPARE_IRQ);
	tm.magic = 0x7141E001u;
	while (1) {
		__WFI();
	}
}
