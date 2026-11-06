/* One-second wake-up tick for the deep-sleep example: TIMG1 on LFCLK, which keeps running in STOP mode. */
#include "it_demo.h"
#include "interval_demo.h"

volatile uint32_t SecondCount;
static void tick(void) { SecondCount++; }
void It_Init(void)
{
    Interval_Init(IT_FSUB, 32767);
    ISR_Register(LPTIM_IRQn, tick);
    IT_Start();
}
