#ifndef __DMA_DEMO_H__
#define __DMA_DEMO_H__
#include "CMS32F04xx.h"
void Dma_Config(DMA_VECTOR_t dma_vector, uint8_t ctrl_data, DMA_Mode_t mode, void *src_adr, void *dst_adr, uint16_t count);
void SPI00_DMA_Config(void);
void SPI_DMA_Config(void);
void USART1RX_DMA_Config(DMA_VECTOR_t dma_vector, uint8_t ctrl_data, DMA_Mode_t mode, void *src_adr, void *dst_adr, uint16_t count);
void USART1TX_DMA_Config(DMA_VECTOR_t dma_vector, uint8_t ctrl_data, DMA_Mode_t mode, void *src_adr, void *dst_adr, uint16_t count);
#endif
