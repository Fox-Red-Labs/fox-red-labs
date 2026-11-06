/* DMA channel 0 of the MSPM0L1306 (3 channels in all) for the Cmsemicon DMA calls. */
#include "dma_demo.h"

static uint8_t trig_of(DMA_VECTOR_t v)
{
    switch (v) {
    case DMA_VECTOR_ADC:        return DMA_ADC0_EVT_GEN_BD_TRIG;
    case DMA_VECTOR_USART1_TX:  return DMA_UART0_TX_TRIG;
    case DMA_VECTOR_USART1_RX:  return DMA_UART0_RX_TRIG;
    case DMA_VECTOR_SPI_TX:     return DMA_SPI0_TX_TRIG;
    case DMA_VECTOR_SPI_RX:     return DMA_SPI0_RX_TRIG;
    default:                    return DMA_GENERIC_SUB0_TRIG;
    }
}
void Dma_Config(DMA_VECTOR_t dma_vector, uint8_t ctrl_data, DMA_Mode_t mode, void *src_adr, void *dst_adr, uint16_t count)
{
    (void) ctrl_data;
    bool to_mem = (dma_vector == DMA_VECTOR_ADC || dma_vector == DMA_VECTOR_USART1_RX || dma_vector == DMA_VECTOR_SPI_RX);
    DL_DMA_Config d = { .transferMode = DL_DMA_SINGLE_TRANSFER_MODE, .extendedMode = DL_DMA_NORMAL_MODE,
        .destIncrement = to_mem ? DL_DMA_ADDR_INCREMENT : DL_DMA_ADDR_UNCHANGED,
        .srcIncrement  = to_mem ? DL_DMA_ADDR_UNCHANGED : DL_DMA_ADDR_INCREMENT,
        .destWidth = (dma_vector == DMA_VECTOR_ADC) ? DL_DMA_WIDTH_HALF_WORD : DL_DMA_WIDTH_BYTE,
        .srcWidth  = (dma_vector == DMA_VECTOR_ADC) ? DL_DMA_WIDTH_HALF_WORD : DL_DMA_WIDTH_BYTE,
        .trigger = trig_of(dma_vector), .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL };   /* peripheral triggers are "external" to the DMA */
    (void) mode;
    DL_DMA_initChannel(DMA, 0, &d);
    DL_DMA_setSrcAddr(DMA, 0, (uint32_t) src_adr);
    DL_DMA_setDestAddr(DMA, 0, (uint32_t) dst_adr);
    DL_DMA_setTransferSize(DMA, 0, count);
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    NVIC_EnableIRQ(DMA_INT_IRQn);
    DL_DMA_enableChannel(DMA, 0);
    if (dma_vector == DMA_VECTOR_ADC) { DL_ADC12_enableDMA(ADC0); DL_ADC12_setDMASamplesCnt(ADC0, 1); DL_ADC12_enableDMATrigger(ADC0, DL_ADC12_DMA_MEM0_RESULT_LOADED); }
}
void USART1RX_DMA_Config(DMA_VECTOR_t v, uint8_t c, DMA_Mode_t m, void *s, void *d, uint16_t n) { Dma_Config(DMA_VECTOR_USART1_RX, c, m, s, d, n); (void) v; }
void USART1TX_DMA_Config(DMA_VECTOR_t v, uint8_t c, DMA_Mode_t m, void *s, void *d, uint16_t n) { Dma_Config(DMA_VECTOR_USART1_TX, c, m, s, d, n); (void) v; DL_UART_Main_enableDMATransmitEvent(UART0); }
void SPI00_DMA_Config(void) { }
void SPI_DMA_Config(void) { }
