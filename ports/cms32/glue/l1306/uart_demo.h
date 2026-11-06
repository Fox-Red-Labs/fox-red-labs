/* Cmsemicon's uart_demo.h interface, on UART0 of the LP-MSPM0L1306 (PA8 TX, PA9 RX: the USB serial port). */
#ifndef __UART_DEMO_H__
#define __UART_DEMO_H__
#include "CMS32F04xx.h"

#define UART_MAX_RECV_LEN 32
#define UART_MAX_RX_LEN   64
#define UART_MAX_TX_LEN   128
extern unsigned char UART0_RX_BUF[UART_MAX_RECV_LEN];
extern unsigned short UART0_RX_STA;
extern uint8_t Uart0_rx_buf[UART_MAX_RX_LEN];
extern uint8_t Rxcount, Txcount;

int8_t Uart0_Init(uint32_t bound);
int8_t Uart1_Init(uint32_t bound);   /* Uart1 and Uart2 also land on UART0: one port to the PC */
int8_t Uart2_Init(uint32_t bound);
void Uart0_Send(uint8_t ch);
char Uart0_Receive(void);
void Uart0_IntSend(uint8_t *tx_buf, uint16_t tx_num);
void Uart1_IntSend(uint8_t *tx_buf, uint16_t tx_num);
int  Uart1_IntRcv(uint8_t *rx_buf);
void Uart2_IntSend(uint8_t *tx_buf, uint16_t tx_num);
uint8_t Uart0_Dma_Rcv(uint8_t *buf);
void Uart0_Dma_Send(uint8_t *tx_buf, uint16_t tx_num);
void uart0_interrupt(void);
#endif
