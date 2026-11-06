/* Cmsemicon's tim_demo.h interfaces (every variant), on the MSPM0L1306's TIMG0, TIMG2 and TIMG4. */
#ifndef __TIM_DEMO_H__
#define __TIM_DEMO_H__
#include "CMS32F04xx.h"
void TIM1_Config(void);          /* timer1: 17.57 kHz PWM; STB/sleep: periodic interrupt; adc: PWM whose CC4 triggers the ADC */
void TIM1_Config_TRGO(void);
void TIM1_DMA_Config(void);
void TIM3_Config(void);
void TIM14_Config(void);
void TIM14_timing_Config(void);
void TIM16_Config(void);
void TIM17_Config(void);
void timer1_CC_interrupt(void);
void timer1_Update_interrupt(void);
void timer3_interrupt(void);
void timer14_interrupt(void);
void timer16_interrupt(void);
void timer17_interrupt(void);
#endif
