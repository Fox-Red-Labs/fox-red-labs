/* CoreMark port for the LP-MSPM0L1306 (copied from the LP-MSPM0G3507 port), bare metal, TI Arm Clang.
 * One tick = one CPU cycle. The only change: the RAM log is CM_LOG_SIZE bytes, because the chip has 4 KB of SRAM. */
#include <stdarg.h>
#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "coremark.h"
#include "core_portme.h"
#include "../common/bench_timer.h"

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

/* Read by the debugger after the run. */
#ifndef CM_LOG_SIZE
#define CM_LOG_SIZE 2048
#endif
char cm_log[CM_LOG_SIZE];
static unsigned cm_len;
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
	return ((secs_ret)ticks) / (secs_ret)CPUCLK_FREQ;
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
	bench_timer_start();
	p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
	p->portable_id = 0;
}
