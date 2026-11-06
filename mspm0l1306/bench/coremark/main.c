/*
 * Bare-metal CoreMark on the LP-MSPM0L1306. Clock comes from the SysConfig file of a TI SDK
 * example for this board (syscfg32: gpio_toggle_output, SYSOSC 32 MHz; syscfg4: the same file
 * with SYSOSC at 4 MHz, the setting gpamp_buffer_to_adc uses). The example's GPIOs are set up
 * but not toggled, so no interrupt other than SysTick runs.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>

extern int coremark_main(void);
extern volatile uint64_t cm_ticks;

volatile struct {
	uint32_t magic;     /* 0xC0DE0001 when CoreMark has returned */
	uint32_t cpuclk_hz;
	uint32_t clkstatus;
	uint32_t ticks_lo;  /* CPU cycles for the timed run */
	uint32_t ticks_hi;
} cm_result;

int main(void)
{
	SYSCFG_DL_init();
	cm_result.cpuclk_hz = CPUCLK_FREQ;
	cm_result.clkstatus = SYSCTL->SOCLOCK.CLKSTATUS;
	coremark_main();
	cm_result.ticks_lo = (uint32_t)cm_ticks;
	cm_result.ticks_hi = (uint32_t)(cm_ticks >> 32);
	cm_result.magic = 0xC0DE0001u;
	while (1) {
		__WFI();
	}
}
