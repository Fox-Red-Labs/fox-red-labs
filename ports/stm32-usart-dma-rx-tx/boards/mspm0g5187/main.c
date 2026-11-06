/*
 * Port of MaJerle/stm32-usart-uart-dma-rx-tx (MIT) projects/usart_rx_idle_line_irq_ringbuff_tx_G0
 * to the LP-MSPM0G5187 (UART0 on PA10/PA11 = XDS110 back-channel), TI MSPM0 SDK DriverLib.
 * The application structure, buffers and packet parser are the original's; only the peripheral
 * layer (usart_init, interrupt handlers, DMA register access) is rewritten. See PORT-NOTES.md.
 *
 * This example shows how can application implement RX and TX DMA for UART.
 * It uses simple packet example approach and 3 separate buffers:
 *
 * - Raw DMA RX buffer where DMA transfers data from UART to memory
 * - Ringbuff for RX data which are processed by application
 * - Ringbuff for TX data to send using TX DMA
 */

/* Includes */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lwrb/lwrb.h"


/* USART related functions */
void    usart_init(void);
void    usart_rx_check(void);
void    usart_process_data(const void* data, size_t len);
void    usart_send_string(const char* str);
uint8_t usart_start_tx_dma_transfer(void);

/* DMA channels: RX needs a "full" channel (repeat mode + early interrupt); channels 0..5 are full on this chip */
#define USART_RX_DMA_CH         0
#define USART_TX_DMA_CH         1

/* Console baud rate, the one build-time define. UART clock = BUSCLK = 32 MHz, 16x oversampling.
 * 64 * (clk / (16 * baud)) rounded, split into integer and 6-bit fractional divisor */
#ifndef UART_BAUD
#define UART_BAUD               115200UL
#endif
#define UART_CLK_HZ             32000000UL
#define UART_DIV64              ((4UL * UART_CLK_HZ + UART_BAUD / 2) / UART_BAUD)
#define UART_IBRD               (UART_DIV64 >> 6)
#define UART_FBRD               (UART_DIV64 & 0x3F)

/* IDLE line stand-in: the UART receive timeout only fires while bytes sit in the RX FIFO, so it cannot
 * report a line that DMA has already emptied. SysTick polls the DMA position every 1 ms instead. */
#define USART_IDLE_POLL_HZ      1000UL
#define CPU_CLK_HZ              32000000UL

/**
 * \brief           Calculate length of statically allocated array
 */
#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

/**
 * \brief           USART RX buffer for DMA to transfer every received byte RX
 * \note            Contains raw data that are about to be processed by different events
 */
uint8_t
usart_rx_dma_buffer[64];

/**
 * \brief           Ring buffer instance for TX data
 */
lwrb_t
usart_rx_rb;

/**
 * \brief           Ring buffer data array for RX DMA
 */
uint8_t
usart_rx_rb_data[128];

/**
 * \brief           Ring buffer instance for TX data
 */
lwrb_t
usart_tx_rb;

/**
 * \brief           Ring buffer data array for TX DMA
 */
uint8_t
usart_tx_rb_data[128];

/**
 * \brief           Length of currently active TX DMA transfer
 */
volatile size_t
usart_tx_dma_current_len;

/**
 * \brief           Application entry point
 */
int
main(void) {
    uint8_t state, cmd, len;

    /* MCU Configuration: power, pins, UART0 and default 32 MHz clock come from SysConfig */
    SYSCFG_DL_init();

    /* Initialize ringbuff for TX & RX */
    lwrb_init(&usart_tx_rb, usart_tx_rb_data, sizeof(usart_tx_rb_data));
    lwrb_init(&usart_rx_rb, usart_rx_rb_data, sizeof(usart_rx_rb_data));

    /* Initialize all configured peripherals */
    usart_init();
    usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\r\n");
    usart_send_string("Start sending data to STM32\r\n");

    /* After this point, do not use usart_send_string function anymore */
    /* Send packet data over UART from PC (or other STM32 device) */

    /* Infinite loop */
    state = 0;
    while (1) {
        uint8_t b;

        /* Process RX ringbuffer */

        /* Packet format: START_BYTE, CMD, LEN[, DATA[0], DATA[len - 1]], STOP BYTE */
        /* DATA bytes are included only if LEN > 0 */
        /* An example, send sequence of these bytes: 0x55, 0x01, 0x01, 0xFF, 0xAA */

        /* Read byte by byte */

        if (lwrb_read(&usart_rx_rb, &b, 1) == 1) {
            switch (state) {
                case 0: {           /* Wait for start byte */
                    if (b == 0x55) {
                        ++state;
                    }
                    break;
                }
                case 1: {           /* Check packet command */
                    cmd = b;
                    ++state;
                    break;
                }
                case 2: {           /* Packet data length */
                    len = b;
                    ++state;
                    if (len == 0) {
                        ++state;    /* Ignore data part if len = 0 */
                    }
                    break;
                }
                case 3: {           /* Data for command */
                    --len;          /* Decrease for received character */
                    if (len == 0) {
                        ++state;
                    }
                    break;
                }
                case 4: {           /* End of packet */
                    if (b == 0xAA) {
                        /* Packet is valid */

                        /* Send out response with CMD = 0xFF */
                        b = 0x55;   /* Start byte */
                        lwrb_write(&usart_tx_rb, &b, 1);
                        cmd = 0xFF; /* Command = 0xFF = OK response */
                        lwrb_write(&usart_tx_rb, &cmd, 1);
                        b = 0x00;   /* Len = 0 */
                        lwrb_write(&usart_tx_rb, &b, 1);
                        b = 0xAA;   /* Stop byte */
                        lwrb_write(&usart_tx_rb, &b, 1);

                        /* Flush everything */
                        usart_start_tx_dma_transfer();
                    }
                    state = 0;
                    break;
                }
            }
        }

        /* Do other tasks ... */
    }
}

/**
 * \brief           Check for new data received with DMA
 *
 * User must select context to call this function from:
 * - Only interrupts (DMA HT, DMA TC, UART IDLE) with same preemption priority level
 * - Only thread context (outside interrupts)
 *
 * If called from both context-es, exclusive access protection must be implemented
 * This mode is not advised as it usually means architecture design problems
 *
 * When IDLE interrupt is not present, application must rely only on thread context,
 * by manually calling function as quickly as possible, to make sure
 * data are read from raw buffer and processed.
 *
 * Not doing reads fast enough may cause DMA to overflow unread received bytes,
 * hence application will lost useful data.
 *
 * Solutions to this are:
 * - Improve architecture design to achieve faster reads
 * - Increase raw buffer size and allow DMA to write more data before this function is called
 */
void
usart_rx_check(void) {
    static size_t old_pos;
    size_t pos;

    /* Calculate current position in buffer and check for new data available */
    pos = ARRAY_LEN(usart_rx_dma_buffer) - DL_DMA_getTransferSize(DMA, USART_RX_DMA_CH);
    if (pos != old_pos) {                       /* Check change in received data */
        if (pos > old_pos) {                    /* Current position is over previous one */
            /*
             * Processing is done in "linear" mode.
             *
             * Application processing is fast with single data block,
             * length is simply calculated by subtracting pointers
             *
             * [   0   ]
             * [   1   ] <- old_pos |------------------------------------|
             * [   2   ]            |                                    |
             * [   3   ]            | Single block (len = pos - old_pos) |
             * [   4   ]            |                                    |
             * [   5   ]            |------------------------------------|
             * [   6   ] <- pos
             * [   7   ]
             * [ N - 1 ]
             */
            usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);
        } else {
            /*
             * Processing is done in "overflow" mode..
             *
             * Application must process data twice,
             * since there are 2 linear memory blocks to handle
             *
             * [   0   ]            |---------------------------------|
             * [   1   ]            | Second block (len = pos)        |
             * [   2   ]            |---------------------------------|
             * [   3   ] <- pos
             * [   4   ] <- old_pos |---------------------------------|
             * [   5   ]            |                                 |
             * [   6   ]            | First block (len = N - old_pos) |
             * [   7   ]            |                                 |
             * [ N - 1 ]            |---------------------------------|
             */
            usart_process_data(&usart_rx_dma_buffer[old_pos], ARRAY_LEN(usart_rx_dma_buffer) - old_pos);
            if (pos > 0) {
                usart_process_data(&usart_rx_dma_buffer[0], pos);
            }
        }
        old_pos = pos;                          /* Save current position as old for next transfers */
    }
}

/**
 * \brief           Check if DMA is active and if not try to send data
 * \return          `1` if transfer just started, `0` if on-going or no data to transmit
 */
uint8_t
usart_start_tx_dma_transfer(void) {
    uint32_t primask;
    uint8_t started = 0;

    /*
     * First check if transfer is currently in-active,
     * by examining the value of usart_tx_dma_current_len variable.
     *
     * This variable is set before DMA transfer is started and cleared in DMA TX complete interrupt.
     *
     * It is not necessary to disable the interrupts before checking the variable:
     *
     * When usart_tx_dma_current_len == 0
     *    - This function is called by either application or TX DMA interrupt
     *    - When called from interrupt, it was just reset before the call,
     *         indicating transfer just completed and ready for more
     *    - When called from an application, transfer was previously already in-active
     *         and immediate call from interrupt cannot happen at this moment
     *
     * When usart_tx_dma_current_len != 0
     *    - This function is called only by an application.
     *    - It will never be called from interrupt with usart_tx_dma_current_len != 0 condition
     *
     * Disabling interrupts before checking for next transfer is advised
     * only if multiple operating system threads can access to this function w/o
     * exclusive access protection (mutex) configured,
     * or if application calls this function from multiple interrupts.
     *
     * This example assumes worst use case scenario,
     * hence interrupts are disabled prior every check
     */
    primask = __get_PRIMASK();
    __disable_irq();
    if (usart_tx_dma_current_len == 0
            && (usart_tx_dma_current_len = lwrb_get_linear_block_read_length(&usart_tx_rb)) > 0) {
        /* Disable channel if enabled */
        DL_DMA_disableChannel(DMA, USART_TX_DMA_CH);

        /* Clear flag left from the previous transfer */
        DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL1);

        /* Prepare DMA data and length */
        DL_DMA_setTransferSize(DMA, USART_TX_DMA_CH, usart_tx_dma_current_len);
        DL_DMA_setSrcAddr(DMA, USART_TX_DMA_CH, (uint32_t)lwrb_get_linear_block_read_address(&usart_tx_rb));

        /* Start transfer */
        DL_DMA_enableChannel(DMA, USART_TX_DMA_CH);
        started = 1;
    }
    __set_PRIMASK(primask);
    return started;
}

/**
 * \brief           Process received data over UART
 * Data are written to RX ringbuffer for application processing at latter stage
 * \param[in]       data: Data to process
 * \param[in]       len: Length in units of bytes
 */
void
usart_process_data(const void* data, size_t len) {
    lwrb_write(&usart_rx_rb, data, len);  /* Write data to receive buffer */
}

/**
 * \brief           Send string over USART
 * \param[in]       str: String to send
 */
void
usart_send_string(const char* str) {
    lwrb_write(&usart_tx_rb, str, strlen(str));   /* Write data to transmit buffer */
    usart_start_tx_dma_transfer();
}

/**
 * \brief           UART0 + DMA Initialization Function
 */
void
usart_init(void) {
    /* Baud rate from the build-time define (SysConfig value is a placeholder) */
    DL_UART_disable(UART_0_INST);
    DL_UART_setBaudRateDivisor(UART_0_INST, UART_IBRD, UART_FBRD);

    /* FIFO: RX threshold 1 entry so DMA moves every byte; no flow control */
    DL_UART_enableFIFOs(UART_0_INST);
    DL_UART_setRXFIFOThreshold(UART_0_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_setTXFIFOThreshold(UART_0_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    /* USART RX DMA Init: repeat-single mode is the closest thing to STM32 circular mode */
    DL_DMA_Config rx_cfg = {
        .transferMode  = DL_DMA_FULL_CH_REPEAT_SINGLE_TRANSFER_MODE,
        .extendedMode  = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_INCREMENT,
        .srcIncrement  = DL_DMA_ADDR_UNCHANGED,
        .destWidth     = DL_DMA_WIDTH_BYTE,
        .srcWidth      = DL_DMA_WIDTH_BYTE,
        .trigger       = DMA_UC0_RX_BD_TRIG,
        .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, USART_RX_DMA_CH, &rx_cfg);
    DL_DMA_setSrcAddr(DMA, USART_RX_DMA_CH, (uint32_t)&UART_0_INST->uart->RXDATA);
    DL_DMA_setDestAddr(DMA, USART_RX_DMA_CH, (uint32_t)usart_rx_dma_buffer);
    DL_DMA_setTransferSize(DMA, USART_RX_DMA_CH, ARRAY_LEN(usart_rx_dma_buffer));

    /* USART TX DMA Init */
    DL_DMA_Config tx_cfg = {
        .transferMode  = DL_DMA_SINGLE_TRANSFER_MODE,
        .extendedMode  = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED,
        .srcIncrement  = DL_DMA_ADDR_INCREMENT,
        .destWidth     = DL_DMA_WIDTH_BYTE,
        .srcWidth      = DL_DMA_WIDTH_BYTE,
        .trigger       = DMA_UC0_TX_BD_TRIG,
        .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, USART_TX_DMA_CH, &tx_cfg);
    DL_DMA_setDestAddr(DMA, USART_TX_DMA_CH, (uint32_t)&UART_0_INST->uart->TXDATA);

    /* Enable DMA RX HT (early interrupt at half of the block) & TC interrupts */
    DL_DMA_Full_Ch_setEarlyInterruptThreshold(DMA, USART_RX_DMA_CH, DL_DMA_EARLY_INTERRUPT_THRESHOLD_HALF);
    DL_DMA_enableInterrupt(DMA, DL_DMA_FULL_CH_INTERRUPT_EARLY_CHANNEL0 | DL_DMA_INTERRUPT_CHANNEL0);
    /* Enable DMA TX TC interrupt */
    DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL1);

    /* DMA interrupt init */
    NVIC_SetPriority(DMA_INT_IRQn, 0);
    NVIC_EnableIRQ(DMA_INT_IRQn);

    /* UART DMA requests */
    DL_UART_enableDMAReceiveEvent(UART_0_INST, DL_UART_DMA_INTERRUPT_RX);
    DL_UART_enableDMATransmitEvent(UART_0_INST);

    /* Idle-line stand-in: SysTick, same priority as DMA */
    SysTick_Config(CPU_CLK_HZ / USART_IDLE_POLL_HZ);
    NVIC_SetPriority(SysTick_IRQn, 0);

    /* Enable USART and DMA RX */
    DL_DMA_enableChannel(DMA, USART_RX_DMA_CH);
    DL_UART_enable(UART_0_INST);
}

/* Interrupt handlers here */

/**
 * \brief           DMA interrupt handler: RX channel HT/TC and TX channel TC
 */
void
DMA_IRQHandler(void) {
    DL_DMA_EVENT_IIDX iidx;

    while ((iidx = DL_DMA_getPendingInterrupt(DMA)) != DL_DMA_EVENT_IIDX_NO_INTR) {
        switch (iidx) {
            case DL_DMA_FULL_CH_EVENT_IIDX_EARLY_IRQ_DMACH0:    /* Half-transfer equivalent */
            case DL_DMA_EVENT_IIDX_DMACH0: {                    /* Transfer complete */
                usart_rx_check();               /* Check for data to process */
                break;
            }
            case DL_DMA_EVENT_IIDX_DMACH1: {    /* TX channel transfer complete */
                lwrb_skip(&usart_tx_rb, usart_tx_dma_current_len);/* Skip sent data, mark as read */
                usart_tx_dma_current_len = 0;   /* Clear length variable */
                usart_start_tx_dma_transfer();  /* Start sending more data */
                break;
            }
            default:
                break;
        }
    }
}

/**
 * \brief           SysTick handler: replaces the USART IDLE line interrupt
 */
void
SysTick_Handler(void) {
    usart_rx_check();                           /* Check for data to process */
}
