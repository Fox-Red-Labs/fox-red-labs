/* Cmsemicon's gpio_demo.h: the examples' LEDs on the LP-MSPM0L1306. LED and LED1 = LED1 (red, PA0); LED2 = RGB blue (PA27). */
#ifndef __GPIO_DEMO_H__
#define __GPIO_DEMO_H__
#include "CMS32F04xx.h"
#define LED_ON       PORT_ClrBit(PORTB, PIN8)
#define LED_OFF      PORT_SetBit(PORTB, PIN8)
#define LED_Toggle   PORT_ToggleBit(PORTB, PIN8)
#define LED1_ON      PORT_ClrBit(PORTB, PIN6)
#define LED1_OFF     PORT_SetBit(PORTB, PIN6)
#define LED1_Toggle  PORT_ToggleBit(PORTB, PIN6)
#define LED2_ON      PORT_ClrBit(PORTB, PIN7)
#define LED2_OFF     PORT_SetBit(PORTB, PIN7)
#define LED2_Toggle  PORT_ToggleBit(PORTB, PIN7)
void Led_Init(void);
void GPIO_Init(void);
#endif
