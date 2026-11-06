/* Cmsemicon's USART1 is their second UART block; here it is UART0, the port to the PC, so its output can be read. */
#ifndef __USART_DEMO_H__
#define __USART_DEMO_H__
#include "CMS32F04xx.h"
void Usart1_Init(uint32_t bound);
void Usart_Send(uint8_t ch);
char Usart_Receive(void);
void usart1_interrupt(void);
void usart1_dma_transmit(void);
#endif
