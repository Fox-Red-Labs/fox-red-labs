/* Port of msp430fr243x_euscia0_uart_01 (MSP430FR2433: eUSCI_A0 UART echo at 9600 baud, receive interrupt, LPM3) to the LP-AM13E230.
 * Same program as the LP-MSPM0L1306 port (uart-echo-lpm3/l1306/main.c) on the AM13 driver library. There is no clock to trim:
 * the UART divides a 100 MHz clock set by SysConfig. Default build waits in a loop; -DSLEEP_MODE=1 sleeps with the SLEEP policy and
 * is build-only here (a sleeping chip blocked the debugger on this board; PORT.md). Written fresh from the AM13 driver library. */
#include "device.h"
#include "ti_sdk_dl_config.h"

#define LOG_PUTC(c) DL_UART_transmitDataBlocking(APP_UART_0_INST, (c))
#define LOG_IDLE()  DEVICE_DELAY_MS(2)
#include "log.h"

#ifndef SLEEP_MODE
#define SLEEP_MODE 0
#endif

volatile uint32_t gBytes, gIrqs;

void APP_UART_0_INT_Handler(void)
{
    gIrqs++;
    if (DL_UART_getPendingInterrupt(APP_UART_0_INST) == DL_UART_IIDX_RX) {
        while (!DL_UART_isRXFIFOEmpty(APP_UART_0_INST)) {
            gBytes++;
            uint8_t c = DL_UART_receiveData(APP_UART_0_INST);
            DL_UART_transmitDataBlocking(APP_UART_0_INST, c);
            DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
#ifdef DEBUG_STATS
            if (c == '\n') { log_s("C1 stats"); log_kv("bytes", gBytes); log_kv("irqs", gIrqs); log_nl(); }
#endif
        }
    }
}

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    log_s("C1 start uart-echo-lpm3 am13e230 9600 baud"); log_nl();
#if SLEEP_MODE == 1
    DL_SYSCTL_setPowerPolicyRUNSLEEP();
#endif
    for (;;) {
#if SLEEP_MODE == 1
        __WFI();
#endif
    }
}
