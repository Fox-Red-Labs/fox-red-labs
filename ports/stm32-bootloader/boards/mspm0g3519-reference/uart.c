/* UART layer for the MSPM0G3519 (DriverLib), polled, 1000 ms timeout per call like HAL_UART_Receive(..., UART_TIMEOUT). */
#include "uart.h"
static void tick_start(void) { SysTick->LOAD = 32000u - 1u; SysTick->VAL = 0u; SysTick->CTRL = 5u; }   /* 1 ms at the 32 MHz default clock */
uart_status uart_receive(uint8_t *data, uint16_t length) {
    uint32_t ms = 0;
    tick_start();
    for (uint16_t i = 0; i < length; i++) {
        while (DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
            if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) { if (++ms >= UART_TIMEOUT) return UART_ERROR; }
        }
        data[i] = DL_UART_Main_receiveData(UART_0_INST);
    }
    return UART_OK;
}
uart_status uart_transmit_str(uint8_t *data) {
    while (*data) DL_UART_Main_transmitDataBlocking(UART_0_INST, *data++);
    return UART_OK;
}
uart_status uart_transmit_ch(uint8_t data) {
    DL_UART_Main_transmitDataBlocking(UART_0_INST, data);
    return UART_OK;
}
