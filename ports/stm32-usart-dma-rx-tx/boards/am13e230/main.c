/*
 * Port of MaJerle/stm32-usart-uart-dma-rx-tx, projects/usart_rx_idle_line_irq_ringbuff_tx_G0 (MIT),
 * to the LP-AM13E230 (AM13E230x, Cortex-M33) with TI's AM13E2x SDK DriverLib.
 *
 * Same structure as the original: raw DMA RX buffer, lwrb ring buffer for RX data,
 * lwrb ring buffer for TX data sent by TX DMA, and the 5-byte packet parser in the main loop.
 * The USART and DMA setup (usart_init and the interrupt handlers) is rewritten for the AM13 UART
 * on UC4 (PA0 TX, PA1 RX = XDS110 back-channel) and DMA0 channels 0 (RX) and 1 (TX).
 * Chip differences are marked "AM13:" below. Everything else is the original code.
 */

/* Includes */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "ti_sdk_dl_config.h"
#include "lwrb/lwrb.h"

/* AM13: baud rate is one build-time define (-DAPP_BAUD=...) */
#ifndef APP_BAUD
#define APP_BAUD                115200u
#endif

/* AM13: idle time, in the UART's receive-timeout units, after which RX data is considered a finished burst */
#ifndef APP_RX_TIMEOUT
#define APP_RX_TIMEOUT          4u
#endif

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

    /* AM13: device, clock, pins, UART and DMA channel setup come from the SysConfig output */
    Device_Init();
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
    pos = ARRAY_LEN(usart_rx_dma_buffer) - DL_DMA_getTransferSize(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID);
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
        DL_DMA_disableChannel(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID);

        /* Clear all flags */
        DL_DMA_clearInterruptStatus(DMA_TX_DMA_INST, DMA_TX_CHANNEL_INTERRUPT_MASK);

        /* Prepare DMA data and length */
        DL_DMA_setTransferSize(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID, usart_tx_dma_current_len);
        DL_DMA_setSrcAddr(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID, (uint32_t)lwrb_get_linear_block_read_address(&usart_tx_rb));

        /* Start transfer (the UART TX trigger feeds the channel from here) */
        DL_DMA_enableChannel(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID);
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
 * \brief           USART Initialization Function
 * AM13: the pins, UART (FIFOs, 1/2 RX threshold) and the two DMA channels are created by SysConfig.
 * What is set here is what SysConfig cannot express.
 */
void
usart_init(void) {
    uint32_t ibrd;
    uint32_t fbrd;

    /* Baud rate from APP_BAUD: divisor = clock / (16 * baud), 6-bit fraction */
    ibrd = UART_0_INST_FREQUENCY / (16u * APP_BAUD);
    fbrd = (uint32_t)(((uint64_t)(UART_0_INST_FREQUENCY % (16u * APP_BAUD)) * 64u + (8u * APP_BAUD)) / (16u * APP_BAUD));
    DL_UART_disable(UART_0_INST);
    DL_UART_setBaudRateDivisor(UART_0_INST, ibrd, fbrd);

    /* AM13: SysConfig made the channels word wide; the UART data register and our buffers are bytes */
    DL_DMA_setSrcWidth(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, DL_DMA_WIDTH_BYTE);
    DL_DMA_setDestWidth(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, DL_DMA_WIDTH_BYTE);
    DL_DMA_setSrcWidth(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID, DL_DMA_WIDTH_BYTE);
    DL_DMA_setDestWidth(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID, DL_DMA_WIDTH_BYTE);

    /* USART RX DMA: UART data register -> usart_rx_dma_buffer, repeating (the AM13 "circular" mode) */
    DL_DMA_setSrcAddr(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, (uint32_t)&UART_0_INST->uart->RXDATA);
    DL_DMA_setDestAddr(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, (uint32_t)usart_rx_dma_buffer);
    DL_DMA_setTransferSize(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, ARRAY_LEN(usart_rx_dma_buffer));

    /* USART TX DMA: lwrb linear block -> UART data register; source and size set per transfer */
    DL_DMA_setDestAddr(DMA_TX_DMA_INST, DMA_TX_CHANNEL_ID, (uint32_t)&UART_0_INST->uart->TXDATA);

    /* Enable DMA RX HT & TC interrupts (HT = early interrupt at half of the block) */
    DL_DMA_Full_Ch_setEarlyInterruptThreshold(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID, DL_DMA_EARLY_INTERRUPT_THRESHOLD_HALF);
    DL_DMA_clearInterruptStatus(DMA_RX_DMA_INST, DMA_RX_CHANNEL_INTERRUPT_MASK | DMA_RX_CHANNEL_EARLY_INTERRUPT_MASK);
    DL_DMA_enableInterrupt(DMA_RX_DMA_INST, DMA_RX_CHANNEL_INTERRUPT_MASK | DMA_RX_CHANNEL_EARLY_INTERRUPT_MASK);
    /* Enable DMA TX TC interrupts */
    DL_DMA_clearInterruptStatus(DMA_TX_DMA_INST, DMA_TX_CHANNEL_INTERRUPT_MASK);
    DL_DMA_enableInterrupt(DMA_TX_DMA_INST, DMA_TX_CHANNEL_INTERRUPT_MASK);

    /* DMA interrupt: same priority as the UART interrupt (usart_rx_check is not re-entrant) */
    NVIC_SetPriority(DMA0_INT, 0);
    NVIC_EnableIRQ(DMA0_INT);

    /* AM13: no IDLE flag. The receive timeout (no new character for APP_RX_TIMEOUT units) is used instead:
     * it triggers the DMA to flush what is left in the RX FIFO and raises the UART interrupt. */
    DL_UART_setRXInterruptTimeout(UART_0_INST, APP_RX_TIMEOUT);
    DL_UART_enableDMAReceiveEvent(UART_0_INST, DL_UART_DMA_INTERRUPT_RX | DL_UART_DMA_INTERRUPT_RX_TIMEOUT);
    DL_UART_enableDMATransmitEvent(UART_0_INST);
    DL_UART_clearInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_enableInterrupt(UART_0_INST, DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
    NVIC_SetPriority(UC4_INT_IRQn, 0);
    NVIC_EnableIRQ(UC4_INT_IRQn);

    /* Enable USART and DMA RX */
    DL_DMA_enableChannel(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID);
    DL_UART_enable(UART_0_INST);
}

/* Interrupt handlers here */

/**
 * \brief           DMA0 interrupt handler: channel 0 = USART RX, channel 1 = USART TX
 */
void
DMA0_INT_Handler(void) {
    /* Events for DMA Channel 0 = USART DMA RX */
    /* Check half-transfer complete interrupt */
    if (DL_DMA_getRawInterruptStatus(DMA_RX_DMA_INST, DMA_RX_CHANNEL_EARLY_INTERRUPT_MASK)) {
        DL_DMA_clearInterruptStatus(DMA_RX_DMA_INST, DMA_RX_CHANNEL_EARLY_INTERRUPT_MASK);  /* Clear half-transfer complete flag */
        usart_rx_check();                       /* Check for data to process */
    }

    /* Check transfer-complete interrupt */
    if (DL_DMA_getRawInterruptStatus(DMA_RX_DMA_INST, DMA_RX_CHANNEL_INTERRUPT_MASK)) {
        DL_DMA_clearInterruptStatus(DMA_RX_DMA_INST, DMA_RX_CHANNEL_INTERRUPT_MASK);        /* Clear transfer complete flag */
        usart_rx_check();                       /* Check for data to process */
    }

    /* Events for DMA Channel 1 = USART DMA TX */
    /* Check transfer complete */
    if (DL_DMA_getRawInterruptStatus(DMA_TX_DMA_INST, DMA_TX_CHANNEL_INTERRUPT_MASK)) {
        DL_DMA_clearInterruptStatus(DMA_TX_DMA_INST, DMA_TX_CHANNEL_INTERRUPT_MASK);        /* Clear transfer complete flag */
        lwrb_skip(&usart_tx_rb, usart_tx_dma_current_len);/* Skip sent data, mark as read */
        usart_tx_dma_current_len = 0;           /* Clear length variable */
        usart_start_tx_dma_transfer();          /* Start sending more data */
    }

    /* Implement other events when needed */
}

/**
 * \brief           UART (UC4) global interrupt handler
 * AM13: receive timeout stands in for the IDLE line interrupt
 */
void
UC4_IRQHandler(void) {
    if (DL_UART_getRawInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_RX_TIMEOUT_ERROR)) {
        uint32_t guard = 3000;
        uint32_t wait;
        uint16_t sz;

        /* The timeout flag is also the DMA trigger that empties the RX FIFO: clearing it first would
         * cancel that trigger. Wait for the DMA to drain the FIFO, then clear the flag. */
        while (!DL_UART_isRXFIFOEmpty(UART_0_INST) && --guard > 0) {}
        /* If the hardware trigger left bytes behind, push them through the same DMA channel with
         * software requests, one byte per request, waiting for each move to land. */
        while (!DL_UART_isRXFIFOEmpty(UART_0_INST)) {
            sz = DL_DMA_getTransferSize(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID);
            DL_DMA_startTransfer(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID);
            for (wait = 0; wait < 3000 && DL_DMA_getTransferSize(DMA_RX_DMA_INST, DMA_RX_CHANNEL_ID) == sz; ++wait) {}
        }
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        DL_UART_clearInterruptStatus(UART_0_INST, DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);  /* Clear timeout flag */
        usart_rx_check();                       /* Check for data to process */
    }

    /* Implement other events when needed */
}
