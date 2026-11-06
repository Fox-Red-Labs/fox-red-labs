/* CoreMark port for the LP-EM-CC35X1, FreeRTOS thread, TI Arm Clang. One tick = one CPU cycle (DWT CYCCNT).
 * NOT RUN YET (written 2026-10-07). The 32-bit counter wraps after 26.8 s at 160 MHz, so the FreeRTOS tick
 * count is kept as a second clock: cm_run.c prints both, and a run longer than 25 s is timed by the ticks. */
#include <stdarg.h>
#include <stdio.h>
#include "coremark.h"
#include "core_portme.h"
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(cmsis/device.h)
#include <FreeRTOS.h>
#include <task.h>

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

char cm_log[2048];
unsigned cm_len;
volatile uint32_t cm_cycles, cm_ticks_rtos;
static uint32_t start_cyc, start_tick;

#ifndef CM_CPU_HZ
#define CM_CPU_HZ 160000000u /* CC3551E data sheet SWRS337B: 160 MHz Cortex-M33; cm_run.c prints the clock it reads */
#endif

void start_time(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	start_tick = xTaskGetTickCount();
	start_cyc = DWT->CYCCNT;
}

void stop_time(void)
{
	cm_cycles = DWT->CYCCNT - start_cyc;
	cm_ticks_rtos = xTaskGetTickCount() - start_tick;
}

CORE_TICKS get_time(void)
{
	/* past 25 s the 32-bit cycle count may have wrapped: use the RTOS tick (configTICK_RATE_HZ) instead */
	/* measured 2026-10-08: DWT->CYCCNT reads 0 on this chip as shipped, so the RTOS tick (1 ms) is the clock in that case */
	if (cm_cycles == 0 || (uint64_t)cm_ticks_rtos * 1000u / configTICK_RATE_HZ > 25000u)
		return (CORE_TICKS)cm_ticks_rtos * (CM_CPU_HZ / configTICK_RATE_HZ);
	return (CORE_TICKS)cm_cycles;
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
	return ((secs_ret)ticks) / (secs_ret)CM_CPU_HZ;
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
		if (cm_len >= sizeof(cm_log))
			cm_len = sizeof(cm_log) - 1;
	}
	return n;
}

void portable_init(core_portable *p, int *argc, char *argv[])
{
	(void)argc;
	(void)argv;
	p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
	p->portable_id = 0;
}
