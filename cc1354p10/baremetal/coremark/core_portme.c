/* CoreMark port for the LP-EM-CC2340R5, bare metal, TI Arm Clang. One tick = one CPU cycle.
 * Same as the MSPM0G3507 port (mspm0g3507/mcu-bench/baremetal/coremark/core_portme.c)
 * except for the timer header and where the CPU frequency comes from. */
#include <stdarg.h>
#include <stdio.h>
#include "coremark.h"
#include "core_portme.h"
#include "bench_timer.h"
#include <ti/drivers/dpl/ClockP.h>

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

/* CoreMark's printout; bench.c sends it over the UART after the run. */
char cm_log[2048];
unsigned cm_len;
volatile uint64_t cm_ticks;

static CORETIMETYPE start_time_val, stop_time_val;

void start_time(void)
{
	start_time_val = bench_cycles();
}

void stop_time(void)
{
	stop_time_val = bench_cycles();
	cm_ticks = stop_time_val - start_time_val;
}

CORE_TICKS get_time(void)
{
	return (CORE_TICKS)(stop_time_val - start_time_val);
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
	ClockP_FreqHz f;

	ClockP_getCpuFreq(&f); /* 48 MHz on CC23x0 (kernel/nortos/dpl/ClockPLPF3.h) */
	return ((secs_ret)ticks) / (secs_ret)f.lo;
}

ee_u32 default_num_contexts = 1;

int ee_printf(const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(cm_log + cm_len, sizeof(cm_log) - cm_len, fmt, ap);
	va_end(ap);
	if (n > 0) {
		cm_len += (unsigned)n;
		if (cm_len >= sizeof(cm_log)) {
			cm_len = sizeof(cm_log) - 1;
		}
	}
	return n;
}

void portable_init(core_portable *p, int *argc, char *argv[])
{
	(void)argc;
	(void)argv;
	/* bench.c has already started the SysTick counter; do not reset it here. */
	p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
	p->portable_id = 0;
}
