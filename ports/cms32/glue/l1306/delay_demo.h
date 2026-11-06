#ifndef __DELAY_DEMO_H__
#define __DELAY_DEMO_H__
#include "CMS32F04xx.h"
void delay_init(uint32_t sysclk);
void m0_delay_ms(uint32_t ms);
void m0_delay_us(uint32_t us);
void delayMS(uint32_t n);
void delayUS(uint32_t n);
#endif
