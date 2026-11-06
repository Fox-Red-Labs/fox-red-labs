#include "usart_demo.h"
#include "uart_demo.h"
#include "dma_demo.h"
void Usart1_Init(uint32_t bound) { Uart0_Init(bound); }
void Usart_Send(uint8_t ch)      { Uart0_Send(ch); }
char Usart_Receive(void)         { return Uart0_Receive(); }
void usart1_interrupt(void)      { uart0_interrupt(); }
static uint8_t dma_text[] = "Cmsemicon USART1 DMA transmit test!\r\n";
void usart1_dma_transmit(void)
{
    USART1TX_DMA_Config(DMA_VECTOR_USART1_TX, 0, DMA_Mode_Normal, dma_text, (void *) &UART0->TXDATA, sizeof dma_text - 1);
}
