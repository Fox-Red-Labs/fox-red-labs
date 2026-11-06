/* The 15-bit interval timer: TIMG1 on the 32 kHz LFCLK. Cmsemicon's fIL is 32 kHz; the count carries over. */
#include "interval_demo.h"
#include "gpio_demo.h"

uint32_t g_ItIntTaken;
void Interval_Init(uint8_t It_clk, uint16_t counter)
{
    (void) It_clk;
    DL_TimerG_reset(TIMG1); DL_TimerG_enablePower(TIMG1); delay_cycles(16);
    DL_TimerG_ClockConfig c = { .clockSel = DL_TIMER_CLOCK_LFCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0 };
    DL_TimerG_TimerConfig t = { .period = counter, .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .startTimer = DL_TIMER_STOP };
    DL_TimerG_setClockConfig(TIMG1, &c);
    DL_TimerG_initTimerMode(TIMG1, &t);
    DL_TimerG_enableInterrupt(TIMG1, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(TIMG1);
    ISR_Register(LPTIM_IRQn, it_interrupt);
}
void it_interrupt(void)
{
    g_ItIntTaken++;
    LED_Toggle;
}
