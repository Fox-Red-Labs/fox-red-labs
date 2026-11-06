/*
 * Bare-metal benchmark for the LP-EM-CC2755P10 (copied from the LP-EM-CC2340R5 bench, cc2340r5/baremetal/bench.c; changes: power header, board name, VIMS CFG in place of CCHCTRL, which the CC27xx has not): GPIO toggle, interrupt latency and EEMBC CoreMark,
 * built with TI Arm Clang on the F3 SDK's NoRTOS "empty" driver example (this file replaces
 * empty.c; main_nortos.c is the SDK's, unchanged).
 *
 * Same method as mspm0g3507/mcu-bench/baremetal/timing/main.c and src/main.c:
 * tight loops timed by a cycle counter, best of 5; interrupt latency by pending a spare IRQ and
 * reading the timer as the handler's first line. Timer: SysTick on the CPU clock with a wrap
 * counter (bench_timer.h). Clock, power and CCFG: SysConfig defaults (bench.syscfg).
 *
 * Output on the XDS110 backchannel UART (CONFIG_UART2_0) at 115200, in the format of
 * mcu-bench/src/main.c:  "R,name,cycles,iterations,description"  and  "I,key,value".
 * The whole report is repeated every 10 seconds after the run, so a late listener still gets it.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC27XX.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_gpio.h)
#include DeviceFamily_constructPath(inc/hw_vims.h)
#include DeviceFamily_constructPath(inc/hw_fcfg.h)

#include "ti_drivers_config.h"
#include "bench_timer.h"

#define BENCH_BOARD "lp_em_cc2755p10"
#define RUNS 5
/* Spare interrupt: LRFD IRQ0 (exception 22, IRQ 6), the same line the Zephyr run used. The radio is not used here. */
#define SPARE_INT INT_LRFD_IRQ0
#define SPARE_IRQN ((IRQn_Type)(SPARE_INT - 16))
#define REPEAT_SECONDS 10U

extern int coremark_main(void);
extern char cm_log[];
extern unsigned cm_len;
extern volatile uint64_t cm_ticks;

volatile uint32_t bench_wraps;
static volatile uint32_t sink;
static volatile uint32_t isr_val;
static UART2_Handle uart;

/* ---------- results, printed after everything has run ---------- */
#define MAX_ROWS 16
static struct {
	const char *name;
	uint32_t x100; /* cycles per operation x 100 */
	uint32_t ops;
	const char *note;
} rows[MAX_ROWS];
static unsigned nrows;

static uint32_t cpu_hz, led_dio, vtor, flws1t, flws2t, cchctrl, irq_samples;
static uint32_t fcfg_flws1t_init, fcfg_flws1t_final, fcfg_flws2t_init, fcfg_flws2t_final;
static uint64_t cycles_at_start, cycles_after_coremark;

static void row(const char *name, uint64_t cycles, uint32_t ops, const char *note)
{
	if (nrows < MAX_ROWS) {
		rows[nrows].name = name;
		rows[nrows].x100 = (uint32_t)((cycles * 100U) / ops);
		rows[nrows].ops = ops;
		rows[nrows].note = note;
		nrows++;
	}
}

/* ---------- output ---------- */
static void out(const char *fmt, ...)
{
	char buf[200];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > (int)sizeof(buf) - 1) {
		n = sizeof(buf) - 1;
	}
	/* The XDS110 backchannel is a plain terminal: send \r\n. */
	for (int i = 0; i < n; i++) {
		size_t w;
		if (buf[i] == '\n') {
			UART2_write(uart, "\r", 1, &w);
		}
		UART2_write(uart, &buf[i], 1, &w);
	}
}

static void out_raw(const char *s, unsigned len)
{
	for (unsigned i = 0; i < len; i++) {
		size_t w;
		if (s[i] == '\n') {
			UART2_write(uart, "\r", 1, &w);
		}
		UART2_write(uart, &s[i], 1, &w);
	}
}

/* ---------- interrupts ---------- */
static void systick_isr(void)
{
	bench_wraps++;
}

static void spare_isr(void)
{
	isr_val = SysTick->VAL;
}

/* ---------- timing helpers (same shape as mcu-bench) ---------- */
#define TIME_LOOP(name, n, note, body)                                    \
	do {                                                              \
		uint64_t best = UINT64_MAX;                               \
		for (int r = 0; r < RUNS; r++) {                          \
			uint64_t t0 = bench_cycles();                     \
			for (uint32_t i = 0; i < (n); i++) {              \
				body;                                     \
			}                                                 \
			uint64_t dt = bench_cycles() - t0;                \
			if (dt < best) {                                  \
				best = dt;                                \
			}                                                 \
		}                                                         \
		row(name, best, (n), note);                               \
	} while (0)

/* 16 back-to-back STR to the toggle register, minus one SysTick read pair. */
static uint32_t toggle_str16_x100(uint32_t pinmask)
{
	volatile uint32_t *reg = (volatile uint32_t *)(GPIO_BASE + GPIO_O_DOUTTGL31_0);
	uint32_t best = UINT32_MAX;

	for (int r = 0; r < RUNS; r++) {
		uint32_t a = SysTick->VAL;
		__asm volatile(
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			"str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n str %1, [%0]\n"
			: : "l"(reg), "l"(pinmask) : "memory");
		uint32_t b = SysTick->VAL;
		uint32_t a2 = SysTick->VAL;
		uint32_t b2 = SysTick->VAL;
		if (a > b && a2 > b2) { /* no wrap in between */
			uint32_t dt = (a - b) - (a2 - b2);
			if (dt < best) {
				best = dt;
			}
		}
	}
	return (best * 100U) / 16U;
}

static void run_timing(void)
{
	uint32_t pinmask = 1U << led_dio;
	volatile uint32_t *tgl = (volatile uint32_t *)(GPIO_BASE + GPIO_O_DOUTTGL31_0);

	TIME_LOOP("timer_read", 1000, "one bench_cycles() call (SysTick plus wrap count, 64-bit)", sink = (uint32_t)bench_cycles());
	TIME_LOOP("empty_loop", 10000, "loop overhead; subtract from the loop rows below", __asm volatile("nop"));
	TIME_LOOP("gpio_toggle_driver", 2000, "GPIO_toggle(CONFIG_GPIO_LED_0), TI GPIO driver call", GPIO_toggle(CONFIG_GPIO_LED_0));
	TIME_LOOP("gpio_toggle_register", 2000, "direct write to GPIO DOUTTGL31_0 in a loop", *tgl = pinmask);

	{
		uint32_t x100 = toggle_str16_x100(pinmask);
		if (nrows < MAX_ROWS) {
			rows[nrows].name = "gpio_toggle_str16";
			rows[nrows].x100 = x100;
			rows[nrows].ops = 16;
			rows[nrows].note = "one write to DOUTTGL31_0: 16 back-to-back STR in inline asm, minus a SysTick read pair";
			nrows++;
		}
	}
	GPIO_write(CONFIG_GPIO_LED_0, 0);

	{
		uint32_t a = SysTick->VAL, b = SysTick->VAL;
		row("systick_read_pair", a - b, 1, "cycles between two back-to-back SysTick VAL reads");
	}

	/* Interrupt latency: pend the spare IRQ, read SysTick in the handler's first line. */
	HwiP_plug(SPARE_INT, (void *)spare_isr);
	NVIC_SetPriority(SPARE_IRQN, 1);
	NVIC_EnableIRQ(SPARE_IRQN);
	{
		uint32_t best = UINT32_MAX, worst = 0, n = 0;

		for (int i = 0; i < 200; i++) {
			isr_val = 0xFFFFFFFFu;
			uint32_t t0 = SysTick->VAL;
			NVIC_SetPendingIRQ(SPARE_IRQN);
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
		irq_samples = n;
		row("irq_latency_best", best, 1, "pend (NVIC ISPR) to handler's first line, direct vector, no wrapper; includes one SysTick read");
		row("irq_latency_worst_of_200", worst, 1, "same, worst case seen");
	}
	NVIC_DisableIRQ(SPARE_IRQN);
}

static void report(unsigned pass)
{
	out("\nBENCH START\n");
	out("I,board,%s\n", BENCH_BOARD);
	out("I,pass,%u\n", pass);
	out("I,cycles_now,%lu\n", (unsigned long)bench_cycles());
	out("I,optimization,%s\n", BENCH_OPT);
	out("I,compiler,TI Arm Clang %d.%d.%d\n", __ti_major__, __ti_minor__, __ti_patchlevel__);
	out("I,sdk,simplelink_lowpower_f3_sdk_9_21_00_36_LTS nortos, empty example + UART2\n");
	out("I,cpu_hz,%lu,ClockP_getCpuFreq\n", (unsigned long)cpu_hz);
	out("I,timer,SysTick on CPU clock with wrap counter; 1 tick = 1 CPU cycle\n");
	out("I,led_dio,%lu,CONFIG_GPIO_LED_0\n", (unsigned long)led_dio);
	out("I,spare_irq,%d,LRFD IRQ0\n", (int)SPARE_IRQN);
	out("I,vtor,0x%08lx\n", (unsigned long)vtor);
	out("I,vims_flws1t,%lu,flash 1T wait states register\n", (unsigned long)flws1t);
	out("I,vims_flws2t,%lu,flash 2T wait states register\n", (unsigned long)flws2t);
	out("I,vims_cfg,0x%lx,raw VIMS CFG register (CC27xx TRM)\n", (unsigned long)cchctrl);
	out("I,fcfg_flws1t,%lu,%lu,factory trim initial and final\n", (unsigned long)fcfg_flws1t_init, (unsigned long)fcfg_flws1t_final);
	out("I,fcfg_flws2t,%lu,%lu,factory trim initial and final\n", (unsigned long)fcfg_flws2t_init, (unsigned long)fcfg_flws2t_final);
	for (unsigned i = 0; i < nrows; i++) {
		out("R,%s,%lu.%02lu,%lu,%s\n", rows[i].name, (unsigned long)(rows[i].x100 / 100U),
		    (unsigned long)(rows[i].x100 % 100U), (unsigned long)rows[i].ops, rows[i].note);
	}
	out("I,irq_samples,%lu\n", (unsigned long)irq_samples);
	out("I,coremark_ticks,%lu%09lu,CPU cycles for the timed CoreMark run (printed as two parts)\n",
	    (unsigned long)(cm_ticks / 1000000000ULL), (unsigned long)(cm_ticks % 1000000000ULL));
	out("I,cycles_at_start,%lu\n", (unsigned long)cycles_at_start);
	out("I,cycles_after_coremark,%lu\n", (unsigned long)cycles_after_coremark);
	out("--- CoreMark output ---\n");
	out_raw(cm_log, cm_len);
	out("--- end CoreMark output ---\n");
	out("I,cycles_now,%lu\n", (unsigned long)bench_cycles());
	out("BENCH DONE\n");
}

void *mainThread(void *arg0)
{
	UART2_Params up;
	ClockP_FreqHz f;

	(void)arg0;
	/* Keep the CPU awake: SysTick must count every cycle, including while the UART waits. */
	Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);
	Power_setConstraint(PowerLPF3_DISALLOW_IDLE);

	GPIO_init();
	GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
	led_dio = CONFIG_GPIO_LED_0;

	UART2_Params_init(&up);
	up.baudRate = 115200;
	uart = UART2_open(CONFIG_UART2_0, &up);
	if (uart == NULL) {
		while (1) {
		}
	}

	ClockP_getCpuFreq(&f);
	cpu_hz = f.lo;
	vtor = SCB->VTOR;
	flws1t = HWREG(VIMS_BASE + VIMS_O_FLWS1T);
	flws2t = HWREG(VIMS_BASE + VIMS_O_FLWS2T);
	cchctrl = HWREG(VIMS_BASE + VIMS_O_CFG);
	fcfg_flws1t_init = fcfg->criticalTrim.flashTrim.flws1t.initial;
	fcfg_flws1t_final = fcfg->criticalTrim.flashTrim.flws1t.final;
	fcfg_flws2t_init = fcfg->criticalTrim.flashTrim.flws2t.initial;
	fcfg_flws2t_final = fcfg->criticalTrim.flashTrim.flws2t.final;

	out("\nbench: starting (timing tests, then CoreMark %d iterations; results follow when done)\n", ITERATIONS);

	HwiP_plug(INT_SYSTICK, (void *)systick_isr);
	bench_timer_start();
	cycles_at_start = bench_cycles();

	run_timing();
	vtor = SCB->VTOR;
	coremark_main();
	cycles_after_coremark = bench_cycles();

	GPIO_write(CONFIG_GPIO_LED_0, 1); /* LED on: done */
	for (unsigned pass = 1;; pass++) {
		report(pass);
		uint64_t t = bench_cycles() + (uint64_t)cpu_hz * REPEAT_SECONDS;
		while (bench_cycles() < t) {
		}
	}
}
