/*
 * Port of MaJerle/stm32-usart-uart-dma-rx-tx, projects/usart_rx_idle_line_irq_ringbuff_tx_G0
 * (MIT) to the LP-MSPM0C1104 (1 kB RAM, 16 kB flash). The main loop, packet parser, lwrb ring
 * buffers (middlewares/lwrb, unchanged) and usart_rx_check() keep the original structure.
 *
 * Differences forced by the chip (see README.md):
 * - No UART DMA on the C1104: one DMA channel, no UART trigger (SysConfig: "UART does not support
 *   DMA for MSPM0Cxx"). The raw RX buffer is filled by the UART interrupt instead of a DMA channel.
 * - No DMA HT/TC: usart_rx_check() runs from the UART RX-timeout interrupt (the closest thing to
 *   the STM32 IDLE line interrupt) and when the raw buffer wraps.
 * - TX is interrupt-driven from the TX ring buffer, not DMA.
 */

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lwrb/lwrb.h"

#ifndef USART_BAUD
#define USART_BAUD              115200      /* single build-time define: make BAUD=... */
#endif
#ifndef USART_RX_RAW_LEN
#define USART_RX_RAW_LEN        64
#endif
#ifndef USART_RX_RB_LEN
#define USART_RX_RB_LEN         128
#endif
#ifndef USART_TX_RB_LEN
#define USART_TX_RB_LEN         128
#endif

void    usart_init(void);
void    usart_rx_check(void);
void    usart_process_data(const void* data, size_t len);
void    usart_send_string(const char* str);
uint8_t usart_start_tx_transfer(void);

#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

/* Raw RX buffer (filled by the UART interrupt; the STM32 original fills it with DMA) */
uint8_t usart_rx_dma_buffer[USART_RX_RAW_LEN];
volatile size_t usart_rx_wr_pos;                /* stands in for N - DMA data length */

lwrb_t usart_rx_rb;
uint8_t usart_rx_rb_data[USART_RX_RB_LEN];
lwrb_t usart_tx_rb;
uint8_t usart_tx_rb_data[USART_TX_RB_LEN];

/**
 * \brief           Application entry point
 */
int
main(void) {
    uint8_t state, cmd, len;

    /* MCU Configuration: SysConfig output (24 MHz clock, UART0 pins, FIFO, RX timeout) */
    SYSCFG_DL_init();

    /* Initialize ringbuff for TX & RX */
    lwrb_init(&usart_tx_rb, usart_tx_rb_data, sizeof(usart_tx_rb_data));
    lwrb_init(&usart_rx_rb, usart_rx_rb_data, sizeof(usart_rx_rb_data));

    /* Initialize all configured peripherals */
    usart_init();
    usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE interrupts\r\n");
    usart_send_string("Start sending data to STM32\r\n");

    /* After this point, do not use usart_send_string function anymore */
    /* Send packet data over UART from PC (or other MSPM0 device) */

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
                        usart_start_tx_transfer();
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
 * \brief           Check for new data in the raw RX buffer (same logic as the original)
 * Called only from the UART interrupt, so no exclusive access protection is needed.
 */
void
usart_rx_check(void) {
    static size_t old_pos;
    size_t pos;

    pos = usart_rx_wr_pos;
    if (pos != old_pos) {
        if (pos > old_pos) {
            usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);
        } else {
            usart_process_data(&usart_rx_dma_buffer[old_pos], ARRAY_LEN(usart_rx_dma_buffer) - old_pos);
            if (pos > 0) {
                usart_process_data(&usart_rx_dma_buffer[0], pos);
            }
        }
        old_pos = pos;
    }
}

/**
 * \brief           Start sending the TX ring buffer: enable the TX FIFO interrupt, which feeds the FIFO
 * \return          `1` if transmission was started, `0` if already running or nothing to send
 */
uint8_t
usart_start_tx_transfer(void) {
    uint8_t started = 0;

    if (lwrb_get_linear_block_read_length(&usart_tx_rb) > 0
            && !(DL_UART_Main_getEnabledInterrupts(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX))) {
        DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
        started = 1;
    }
    return started;
}

void
usart_process_data(const void* data, size_t len) {
    lwrb_write(&usart_rx_rb, data, len);
}

void
usart_send_string(const char* str) {
    lwrb_write(&usart_tx_rb, str, strlen(str));
    usart_start_tx_transfer();
}

void
usart_init(void) {
    /* Baud rate from the single define; UART clock is the 24 MHz bus clock set by SysConfig */
    DL_UART_Main_disable(UART_0_INST);
    DL_UART_Main_configBaudRate(UART_0_INST, UART_0_INST_FREQUENCY, USART_BAUD);
    DL_UART_Main_enable(UART_0_INST);

    NVIC_SetPriority(UART_0_INST_INT_IRQN, 0);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

/**
 * \brief           UART0 interrupt: RX data into the raw buffer, RX timeout = idle line, TX FIFO refill
 */
void
UART_0_INST_IRQHandler(void) {
    uint8_t b;

    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_RX:
        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR: {
            /* Drain the RX FIFO into the raw buffer (what DMA does on the STM32) */
            while (DL_UART_Main_receiveDataCheck(UART_0_INST, &b)) {
                size_t w = usart_rx_wr_pos;
                usart_rx_dma_buffer[w] = b;
                if (++w >= ARRAY_LEN(usart_rx_dma_buffer)) {
                    w = 0;
                }
                usart_rx_wr_pos = w;
                if (w == ARRAY_LEN(usart_rx_dma_buffer) / 2 || w == 0) {
                    usart_rx_check();           /* half / full, like DMA HT and TC */
                }
            }
            usart_rx_check();                   /* FIFO empty after the timeout = idle line */
            break;
        }
        case DL_UART_MAIN_IIDX_TX: {
            while (!DL_UART_Main_isTXFIFOFull(UART_0_INST)
                    && lwrb_get_linear_block_read_length(&usart_tx_rb) > 0) {
                DL_UART_Main_transmitData(UART_0_INST,
                    *(const uint8_t*)lwrb_get_linear_block_read_address(&usart_tx_rb));
                lwrb_skip(&usart_tx_rb, 1);
            }
            if (lwrb_get_linear_block_read_length(&usart_tx_rb) == 0) {
                DL_UART_Main_disableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
            }
            break;
        }
        default:
            break;
    }
}
