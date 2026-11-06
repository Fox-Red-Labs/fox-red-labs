/*
 * Port of MaJerle/stm32-usart-uart-dma-rx-tx, projects/usart_rx_idle_line_irq_ringbuff_tx_G0
 * to the LP-MSPM0L1306 (4 kB RAM, 64 kB flash; MSPM0 SDK DriverLib). The structure of main.c is kept: raw DMA RX buffer,
 * lwrb ring buffers for RX and TX, TX DMA fed from the TX ring buffer, 5-byte packet parser in main().
 *
 * What the chip cannot do the way the STM32G0 does (see PORT-NOTES.md):
 * - UART has no IDLE-line flag: a 1 ms timer interrupt that sees the RX DMA position stand still is used.
 * - DMA has no circular mode and no half-transfer interrupt: RX DMA is a 64-byte single-block
 *   transfer that is re-armed from its done interrupt; usart_rx_check() also runs from the timer tick.
 */

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lwrb/lwrb.h"

/* Single build-time baud rate define (makefile: BAUD=115200, passed as -DUSART_BAUD) */
#ifndef USART_BAUD
#define USART_BAUD              115200
#endif

/* USART related functions */
void    usart_init(void);
void    usart_rx_check(void);
void    usart_process_data(const void* data, size_t len);
void    usart_send_string(const char* str);
uint8_t usart_start_tx_dma_transfer(void);

#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

/** USART RX buffer for DMA to transfer every received byte RX */
uint8_t usart_rx_dma_buffer[64];

/** Ring buffer instance and data for RX data processed by the application */
lwrb_t usart_rx_rb;
uint8_t usart_rx_rb_data[128];

/** Ring buffer instance and data for TX data sent with TX DMA */
lwrb_t usart_tx_rb;
uint8_t usart_tx_rb_data[128];

/** Length of currently active TX DMA transfer */
volatile size_t usart_tx_dma_current_len;

/** Position in the raw RX DMA buffer up to which data were already processed */
static size_t usart_rx_old_pos;

int
main(void) {
    uint8_t state, cmd, len;

    SYSCFG_DL_init();

    /* Initialize ringbuff for TX & RX */
    lwrb_init(&usart_tx_rb, usart_tx_rb_data, sizeof(usart_tx_rb_data));
    lwrb_init(&usart_rx_rb, usart_rx_rb_data, sizeof(usart_rx_rb_data));

    usart_init();
    usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\r\n");
    usart_send_string("Start sending data to STM32\r\n");

    /* Packet format: START_BYTE, CMD, LEN[, DATA[0], DATA[len - 1]], STOP BYTE */
    state = 0;
    while (1) {
        uint8_t b;

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
                        /* Packet is valid: send response with CMD = 0xFF */
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
    }
}

/** Arm the RX DMA channel for a fresh pass over the raw buffer (no circular mode on this DMA) */
static void
usart_rx_dma_arm(void) {
    DL_DMA_disableChannel(DMA, DMA_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_RX_CHAN_ID, (uint32_t)(&UART_0_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_RX_CHAN_ID, (uint32_t)usart_rx_dma_buffer);
    DL_DMA_setTransferSize(DMA, DMA_RX_CHAN_ID, ARRAY_LEN(usart_rx_dma_buffer));
    DL_DMA_enableChannel(DMA, DMA_RX_CHAN_ID);
    usart_rx_old_pos = 0;
}

/**
 * \brief           Check for new data received with DMA
 * Called only from interrupts (RX timeout, RX DMA done) with the same priority.
 */
void
usart_rx_check(void) {
    size_t pos;

    /* Current position = buffer length - bytes the DMA still has to transfer */
    pos = ARRAY_LEN(usart_rx_dma_buffer) - DL_DMA_getTransferSize(DMA, DMA_RX_CHAN_ID);
    if (pos != usart_rx_old_pos) {
        if (pos > usart_rx_old_pos) {
            usart_process_data(&usart_rx_dma_buffer[usart_rx_old_pos], pos - usart_rx_old_pos);
        } else {
            usart_process_data(&usart_rx_dma_buffer[usart_rx_old_pos], ARRAY_LEN(usart_rx_dma_buffer) - usart_rx_old_pos);
            if (pos > 0) {
                usart_process_data(&usart_rx_dma_buffer[0], pos);
            }
        }
        usart_rx_old_pos = pos;
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

    primask = __get_PRIMASK();
    __disable_irq();
    if (usart_tx_dma_current_len == 0
            && (usart_tx_dma_current_len = lwrb_get_linear_block_read_length(&usart_tx_rb)) > 0) {
        DL_DMA_disableChannel(DMA, DMA_TX_CHAN_ID);
        DL_DMA_setSrcAddr(DMA, DMA_TX_CHAN_ID, (uint32_t)lwrb_get_linear_block_read_address(&usart_tx_rb));
        DL_DMA_setDestAddr(DMA, DMA_TX_CHAN_ID, (uint32_t)(&UART_0_INST->TXDATA));
        DL_DMA_setTransferSize(DMA, DMA_TX_CHAN_ID, usart_tx_dma_current_len);
        DL_DMA_enableChannel(DMA, DMA_TX_CHAN_ID);
        started = 1;
    }
    __set_PRIMASK(primask);
    return started;
}

/** Process received data over UART: write to the RX ringbuffer for the application */
void
usart_process_data(const void* data, size_t len) {
    lwrb_write(&usart_rx_rb, data, len);
}

/** Send string over USART */
void
usart_send_string(const char* str) {
    lwrb_write(&usart_tx_rb, str, strlen(str));
    usart_start_tx_dma_transfer();
}

/** UART0 + DMA setup. Pins, FIFO, DMA channels and triggers come from SysConfig (SYSCFG_DL_init) */
void
usart_init(void) {
    /* Baud rate from the single build-time define, applied with the UART disabled */
    DL_UART_Main_disable(UART_0_INST);
    DL_UART_Main_configBaudRate(UART_0_INST, UART_0_INST_FREQUENCY, USART_BAUD);

    /* The UART has no IDLE-line flag, and its RX timeout never fires here because the DMA empties
     * the RX FIFO on every byte. Stand-in: a 1 ms timer tick that treats "DMA position unchanged
     * since the last tick" as line idle (see TIMER_IDLE_INST_IRQHandler). */
    NVIC_EnableIRQ(TIMER_IDLE_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_IDLE_INST);

    usart_rx_dma_arm();

    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    DL_UART_Main_enable(UART_0_INST);
}

/** UART0 interrupt: RX DMA block done, TX DMA block done */
void
UART_0_INST_IRQHandler(void) {
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_DMA_DONE_RX:
            usart_rx_check();           /* Whole block received: DMA position = buffer length */
            usart_rx_dma_arm();         /* No circular mode: start the next pass */
            break;
        case DL_UART_MAIN_IIDX_DMA_DONE_TX:
            lwrb_skip(&usart_tx_rb, usart_tx_dma_current_len);  /* Skip sent data, mark as read */
            usart_tx_dma_current_len = 0;
            usart_start_tx_dma_transfer();
            break;
        default:
            break;
    }
}

/** 1 ms tick, same priority as the UART interrupt: stands in for the USART IDLE-line interrupt */
void
TIMER_IDLE_INST_IRQHandler(void) {
    static size_t last_pos;
    size_t pos;

    if (DL_TimerG_getPendingInterrupt(TIMER_IDLE_INST) == DL_TIMER_IIDX_ZERO) {
        pos = ARRAY_LEN(usart_rx_dma_buffer) - DL_DMA_getTransferSize(DMA, DMA_RX_CHAN_ID);
        if (pos == last_pos && pos != usart_rx_old_pos) {   /* new data, nothing arrived for 1 tick */
            usart_rx_check();
        }
        last_pos = pos;
    }
}
