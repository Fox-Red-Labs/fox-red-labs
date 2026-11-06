#ifndef __ADC_DEMO_H__
#define __ADC_DEMO_H__
#include "CMS32F04xx.h"
void Adc_Init(void);
void Adc_Hardwait_Init(void);
void Adc_HardNoWait_Init(void);
void Adc_Scan_Init(void);
void Adc_SoftTrig_Init(void);
void adc_interrupt(void);
#endif
