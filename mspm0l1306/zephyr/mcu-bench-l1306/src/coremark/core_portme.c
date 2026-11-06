/* CoreMark port for Zephyr on the LP-MSPM0G3507. Time comes from the kernel's millisecond uptime. */
#include <zephyr/kernel.h>
#include "coremark.h"
#include "core_portme.h"

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

static CORETIMETYPE start_time_val, stop_time_val;

void start_time(void)
{
	start_time_val = k_uptime_get_32();
}

void stop_time(void)
{
	stop_time_val = k_uptime_get_32();
}

CORE_TICKS get_time(void)
{
	return (CORE_TICKS)(stop_time_val - start_time_val);
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
	return ((secs_ret)ticks) / (secs_ret)1000;
}

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
	p->portable_id = 0;
}
