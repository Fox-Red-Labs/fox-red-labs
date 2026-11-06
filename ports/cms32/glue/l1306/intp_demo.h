#ifndef __INTP_DEMO_H__
#define __INTP_DEMO_H__
#include "CMS32F04xx.h"
#define INTP0 0x0001
extern uint8_t flag;
void INTP_Start(uint16_t pinMsk);
void INTP_Stop(uint16_t pinMsk);
void Intp_Init(void);
void Key_Init(void);
void key_interrupt(void);
void intp0_interrupt(void);
void intp4To15_interrupt(void);
#endif
