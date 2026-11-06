/*
 * Bare-metal CoreMark on the LP-AM13E230. Clock and pins from the SysConfig file of TI's own
 * benchmarks/coremark example for this board (am13e230x_lp/m33_nortos/example.syscfg), so the
 * clock is TI's (200 MHz in the SDK's ACI benchmark README; the data sheet allows 250 MHz with
 * 4 flash wait states). Device_Init() is the SDK's board start-up (ti_sdk_config device_support).
 * Results stay in RAM (cm_result, cm_log) for the debugger to read.
 */
#include "device.h"
#include "ti_sdk_dl_config.h"
#include <stdint.h>

extern int coremark_main(void);
extern volatile uint64_t cm_ticks;

volatile struct {
	uint32_t magic;     /* 0xC0DE0001 when CoreMark has returned */
	uint32_t cpu_hz;
	uint32_t ticks_lo;
	uint32_t ticks_hi;
} cm_result;

int main(void)
{
	Device_Init();
	SYSCFG_DL_init();
#ifdef LOW_CLK_DIV
	/* Low-clock variant: TI's 200 MHz setup first, then the PLL divider (2 = 100 MHz, 4 = 50 MHz) and the
	 * flash wait states the SDK documents for that speed (50 to 100 MHz: 1; under 50 MHz: 0). Divide first, then
	 * lower the wait states, with the flash cache off while they change (the order dl_fri.h and SysConfig use). */
	DL_SYSCTL_enablePLLDivider(LOW_CLK_DIV == 2 ? DL_SYSCTL_PLL_DIVIDER_DIV2 : DL_SYSCTL_PLL_DIVIDER_DIV4);
	DL_FRI_disableDLB();
	DL_FRI_disableCache();
	DL_FRI_setReadWaitStates(LOW_CLK_WS);
	DL_FRI_enableDLB();
	DL_FRI_enableCache();
#endif
	cm_result.cpu_hz = CPU_HZ;
	coremark_main();
	cm_result.ticks_lo = (uint32_t)cm_ticks;
	cm_result.ticks_hi = (uint32_t)(cm_ticks >> 32);
	cm_result.magic = 0xC0DE0001u;
	while (1) {
		__WFI();
	}
}
