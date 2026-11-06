#include "uart_demo.h"

unsigned char UART0_RX_BUF[UART_MAX_RECV_LEN];
unsigned short UART0_RX_STA;
uint8_t Uart0_rx_buf[UART_MAX_RX_LEN];
uint8_t Rxcount, Txcount;
static uint8_t *pTxbuff;
static volatile uint8_t Txcompleteflag;

static const DL_UART_Main_ClockConfig clk = {
    .clockSel = DL_UART_MAIN_CLOCK_BUSCLK, .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1 };
static const DL_UART_Main_Config cfg = {
    .mode = DL_UART_MAIN_MODE_NORMAL, .direction = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE, .parity = DL_UART_MAIN_PARITY_NONE,
    .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS, .stopBits = DL_UART_MAIN_STOP_BITS_ONE };

int8_t Uart0_Init(uint32_t bound)
{
    DL_UART_Main_reset(UART0);
    DL_UART_Main_enablePower(UART0);
    delay_cycles(16);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM9, IOMUX_PINCM9_PF_UART0_TX);
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM10, IOMUX_PINCM10_PF_UART0_RX);
    DL_UART_Main_setClockConfig(UART0, (DL_UART_Main_ClockConfig *) &clk);
    DL_UART_Main_init(UART0, (DL_UART_Main_Config *) &cfg);
    /* 16x oversampling from the 32 MHz bus clock: divisor = 32e6 / (16 * baud) */
    uint32_t div64 = (SystemCoreClock * 4u) / bound;        /* divisor * 64 */
    DL_UART_Main_setOversampling(UART0, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART0, div64 >> 6, div64 & 0x3F);
    DL_UART_Main_enableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(UART0_INT_IRQn);
    DL_UART_Main_enable(UART0);
    return 0;
}
int8_t Uart1_Init(uint32_t bound) { return Uart0_Init(bound); }
int8_t Uart2_Init(uint32_t bound) { return Uart0_Init(bound); }

void Uart0_Send(uint8_t ch) { DL_UART_Main_transmitDataBlocking(UART0, ch); }
char Uart0_Receive(void)    { return (char) DL_UART_Main_receiveDataBlocking(UART0); }

void Uart0_IntSend(uint8_t *tx_buf, uint16_t tx_num)
{
    pTxbuff = tx_buf; Txcount = (uint8_t) tx_num; Txcompleteflag = 0;
    DL_UART_Main_enableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_TX);
    NVIC_SetPendingIRQ(UART0_INT_IRQn);
}
void Uart1_IntSend(uint8_t *tx_buf, uint16_t tx_num) { Uart0_IntSend(tx_buf, tx_num); }
void Uart2_IntSend(uint8_t *tx_buf, uint16_t tx_num) { Uart0_IntSend(tx_buf, tx_num); }
int  Uart1_IntRcv(uint8_t *rx_buf) { int n = Rxcount; memcpy(rx_buf, Uart0_rx_buf, n); Rxcount = 0; return n; }
uint8_t Uart0_Dma_Rcv(uint8_t *buf) { return (uint8_t) Uart1_IntRcv(buf); }
void Uart0_Dma_Send(uint8_t *tx_buf, uint16_t tx_num) { while (tx_num--) Uart0_Send(*tx_buf++); }

void uart0_interrupt(void)
{
    /* receive: keep the bytes, as the Cmsemicon glue did */
    while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
        uint8_t rx = DL_UART_Main_receiveData(UART0);
        Uart0_rx_buf[Rxcount & 0x7F] = rx;
        UART0_RX_BUF[Rxcount % UART_MAX_RECV_LEN] = rx;
        Rxcount = (Rxcount >= UART_MAX_RX_LEN) ? 0 : Rxcount + 1;
    }
    /* transmit: feed the FIFO from the buffer handed to Uart0_IntSend */
    while (Txcount > 0 && !DL_UART_Main_isTXFIFOFull(UART0)) {
        DL_UART_Main_transmitData(UART0, *pTxbuff++);
        if (--Txcount == 0) { Txcompleteflag = 1; DL_UART_Main_disableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_TX); }
    }
}
void UART0_IRQHandler(void)
{
    (void) DL_UART_Main_getPendingInterrupt(UART0);
    if (cms_isr_registered(SCI_IRQn)) cms_isr_dispatch(SCI_IRQn);
    else if (cms_isr_registered(USART1_IRQn)) cms_isr_dispatch(USART1_IRQn);
    else uart0_interrupt();
}
