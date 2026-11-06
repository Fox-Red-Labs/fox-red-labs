/* The independent watchdog, 250 ms in the original. The MSPM0L1306 has one watchdog, WWDT0, on the
 * 32 kHz LFCLK: divide 1, 2^13 counts = 250 ms, no closed window. Nobody refreshes it, so it resets. */
#include "iwdg_demo.h"
void WDT_Config(void)
{
    DL_WWDT_reset(WWDT0); DL_WWDT_enablePower(WWDT0); delay_cycles(16);
    DL_WWDT_initWatchdogMode(WWDT0, DL_WWDT_CLOCK_DIVIDE_1, DL_WWDT_TIMER_PERIOD_12_BITS, DL_WWDT_RUN_IN_SLEEP,
                             DL_WWDT_WINDOW_PERIOD_0, DL_WWDT_WINDOW_PERIOD_0);
    DL_WWDT_setActiveWindow(WWDT0, DL_WWDT_WINDOW0);
}
