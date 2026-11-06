/* The window watchdog. The original closes the window for the first 64 ms of an 87 ms period and the
 * example then refreshes in a tight loop, which is always too early, so the chip resets at once, forever.
 * On the MSPM0L1306 that reset loop (one every few milliseconds) locks every debugger out and needs a
 * mass erase through the debug mailbox to recover. So here the window
 * is fully open: 125 ms period (divide 1, 2^12 counts of 32 kHz), refreshed by the example's loop, no reset. */
#include "wwdg_demo.h"
void WWDG_Config(void)
{
    DL_WWDT_reset(WWDT0); DL_WWDT_enablePower(WWDT0); delay_cycles(16);
    DL_WWDT_initWatchdogMode(WWDT0, DL_WWDT_CLOCK_DIVIDE_1, DL_WWDT_TIMER_PERIOD_12_BITS, DL_WWDT_RUN_IN_SLEEP,
                             DL_WWDT_WINDOW_PERIOD_0, DL_WWDT_WINDOW_PERIOD_0);
    DL_WWDT_setActiveWindow(WWDT0, DL_WWDT_WINDOW0);
}
