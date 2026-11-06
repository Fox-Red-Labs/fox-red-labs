/*
 * Port of MaJerle/stm32-usart-uart-dma-rx-tx, projects/usart_rx_idle_line_irq_ringbuff_tx_G0
 * (upstream commit c242589d, MIT) to the LP-MSPM33C321A (Cortex-M33), TI MSPM33 SDK 1.04.00.00.
 *
 * Same structure as the original:
 * - Raw DMA RX buffer where DMA transfers data from UART to memory (circular)
 * - lwrb ring buffer (unchanged middleware) for RX data which are processed by the application
 * - lwrb ring buffer for TX data to send using TX DMA
 * - 5-byte packet parser in the main loop
 *
 * What the chip does differently from the STM32G0 (details in README.md):
 * - No DMA half-transfer interrupt: only DMA "block done" (TC) is available.
 * - No USART IDLE flag. The UART's receive timeout (RTOUT) only fires while bytes sit in the RX FIFO, and
 *   the DMA empties the FIFO as each byte arrives, so it can never fire here. Closest mechanism used
 *   instead: a 1 ms SysTick interrupt calls usart_rx_check() (about 11 bit times at 115200).
 * - The UART's own DMA_DONE_RX / DMA_DONE_TX interrupts share the single UC12 vector (no DMA vector).
 * - No DMAMUX: the DMA channel trigger is chosen with DL_DMA_Config.trigger (set by SysConfig).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "lwrb/lwrb.h"

/* Single build-time baud rate define: make USART_BAUD=9600 (default below) */
#ifndef USART_BAUD
#define USART_BAUD              115200
#endif

/* 64 * UART_clk / (16 * baud), rounded: integer part is IBRD, low 6 bits are FBRD */
#define USART_DIV64             ((4u * UART_0_INST_FREQUENCY + (USART_BAUD) / 2u) / (USART_BAUD))
#define USART_IBRD              (USART_DIV64 >> 6)
#define USART_FBRD              (USART_DIV64 & 0x3Fu)

/* Idle-line stand-in: SysTick period in ticks of the 32 MHz CPU clock (1 ms) */
#define USART_IDLE_POLL_TICKS   (CPUCLK_FREQ / 1000)

/* USART related functions */
void    usart_init(void);
void    usart_rx_check(void);
void    usart_process_data(const void* data, size_t len);
void    usart_send_string(const char* str);
uint8_t usart_start_tx_dma_transfer(void);

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
 * \brief           Ring buffer instance for RX data
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

    /* Clocks, pins, UART and DMA channels are set up by SysConfig (32 MHz default clock) */
    SYSCFG_DL_init();

    /* Initialize ringbuff for TX & RX */
    lwrb_init(&usart_tx_rb, usart_tx_rb_data, sizeof(usart_tx_rb_data));
    lwrb_init(&usart_rx_rb, usart_rx_rb_data, sizeof(usart_rx_rb_data));

    /* Initialize all configured peripherals */
    usart_init();
    usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\r\n");
    usart_send_string("Start sending data to STM32\r\n");

    /* After this point, do not use usart_send_string function anymore */
    /* Send packet data over UART from PC (or other device) */

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
 * - Only interrupts (RX DMA done, SysTick idle poll) with same preemption priority level
 * - Only thread context (outside interrupts)
 *
 * UC12 and SysTick are both priority 0, so they cannot preempt each other.
 */
void
usart_rx_check(void) {
    static size_t old_pos;
    size_t pos;

    /*
     * Calculate current position in buffer and check for new data available.
     * The repeat-single DMA channel counts its remaining transfers down; it reloads after the last one,
     * so a momentary reading of 0 (= buffer length) is position 0.
     */
    pos = (ARRAY_LEN(usart_rx_dma_buffer) - DL_DMA_getTransferSize(DMA0, DMA0_CH1_CHAN_ID))
          % ARRAY_LEN(usart_rx_dma_buffer);
    if (pos != old_pos) {                       /* Check change in received data */
        if (pos > old_pos) {                    /* Current position is over previous one */
            /* "Linear" mode: single block, length is pos - old_pos */
            usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);
        } else {
            /* "Overflow" mode: two blocks, first to the end of the buffer, then from its start */
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
     * Transfer is in-active when usart_tx_dma_current_len == 0. The variable is set before the DMA
     * transfer is started and cleared in the TX DMA done interrupt.
     * As in the original, interrupts are disabled around the check.
     */
    primask = __get_PRIMASK();
    __disable_irq();
    if (usart_tx_dma_current_len == 0
            && (usart_tx_dma_current_len = lwrb_get_linear_block_read_length(&usart_tx_rb)) > 0) {
        /* Disable channel if enabled */
        DL_DMA_disableChannel(DMA0, DMA0_CH0_CHAN_ID);

        /* Prepare DMA data and length */
        DL_DMA_setSrcAddr(DMA0, DMA0_CH0_CHAN_ID, (uint32_t)lwrb_get_linear_block_read_address(&usart_tx_rb));
        DL_DMA_setTransferSize(DMA0, DMA0_CH0_CHAN_ID, usart_tx_dma_current_len);

        /* Start transfer */
        DL_DMA_enableChannel(DMA0, DMA0_CH0_CHAN_ID);
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
 * \brief           UART + DMA initialization (pins, clocks and channel modes come from SysConfig)
 */
void
usart_init(void) {
    /* Changing the divisor and FIFO settings needs the UART disabled */
    DL_UART_disable(UART_0_INST);
    DL_UART_setOversampling(UART_0_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_setBaudRateDivisor(UART_0_INST, USART_IBRD, USART_FBRD);

    /* RX DMA: UART RXDATA -> circular raw buffer. Repeat-single mode reloads after the last transfer */
    DL_DMA_setSrcAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)(&UART_0_INST->uart->RXDATA));
    DL_DMA_setDestAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)usart_rx_dma_buffer);
    DL_DMA_setTransferSize(DMA0, DMA0_CH1_CHAN_ID, ARRAY_LEN(usart_rx_dma_buffer));

    /* TX DMA: ring buffer -> UART TXDATA (source and length are set per transfer) */
    DL_DMA_setDestAddr(DMA0, DMA0_CH0_CHAN_ID, (uint32_t)(&UART_0_INST->uart->TXDATA));

    /* Every received byte triggers the RX DMA (FIFO level set to one entry in the .syscfg) */

    /* Interrupts: DMA done RX (TC), DMA done TX (TC) */
    DL_UART_enableInterrupt(UART_0_INST, DL_UART_INTERRUPT_DMA_DONE_RX | DL_UART_INTERRUPT_DMA_DONE_TX);
    NVIC_SetPriority(UART_0_INST_INT_IRQN, 0);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

    /* Idle-line stand-in: SysTick every 1 ms, same priority as the UART interrupt */
    SysTick_Config(USART_IDLE_POLL_TICKS);
    NVIC_SetPriority(SysTick_IRQn, 0);

    /* Enable USART and DMA RX */
    DL_DMA_enableChannel(DMA0, DMA0_CH1_CHAN_ID);
    DL_UART_enable(UART_0_INST);
}

/* Interrupt handlers here */

/**
 * \brief           SysTick, 1 ms: stand-in for the USART IDLE line interrupt
 */
void
SysTick_Handler(void) {
    usart_rx_check();
}

/**
 * \brief           UC12 (UART) interrupt: RX DMA done, TX DMA done
 */
void
UART_0_INST_IRQHandler(void) {
    DL_UART_IIDX iidx;

    while ((iidx = DL_UART_getPendingInterrupt(UART_0_INST)) != DL_UART_IIDX_NO_INTERRUPT) {
        switch (iidx) {
            case DL_UART_IIDX_DMA_DONE_RX:      /* Same job as the DMA TC interrupt on the STM32 */
                usart_rx_check();
                break;
            case DL_UART_IIDX_DMA_DONE_TX:      /* TX DMA transfer complete */
                lwrb_skip(&usart_tx_rb, usart_tx_dma_current_len);/* Skip sent data, mark as read */
                usart_tx_dma_current_len = 0;   /* Clear length variable */
                usart_start_tx_dma_transfer();  /* Start sending more data */
                break;
            default:
                break;
        }
    }
}
