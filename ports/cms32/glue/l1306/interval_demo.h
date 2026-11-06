#ifndef __INTERVAL_DEMO_H__
#define __INTERVAL_DEMO_H__
#include "CMS32F04xx.h"
extern uint32_t g_ItIntTaken;
void it_interrupt(void);
void Interval_Init(uint8_t It_clk, uint16_t counter);
#endif
