/* Port of msp430fr243x_euscia0_uart_01 (MSP430FR2433: eUSCI_A0 UART echo at 9600 baud, receive interrupt, LPM3) to the LP-MSPM0L1306.
 * Original: DCO tuned to 8 MHz with a software trim loop, UART from SMCLK, auto-clock-enable wakes SMCLK on a receive edge.
 * Port: UART0 from the bus clock (default build, STANDBY0) or from the 32768 Hz LFCLK at 3x oversampling (example-lfclk.syscfg,
 * STANDBY1). Neither needs a clock trim loop; the CPU wakes only for the receive interrupt. Written fresh from the MSPM0 DriverLib. See PORT.md. */
#include "ti_msp_dl_config.h"

#define LOG_PUTC(c) DL_UART_Main_transmitDataBlocking(UART_0_INST, (c))
#define LOG_IDLE()  while (DL_UART_Main_isBusy(UART_0_INST)) { }
#include "log.h"

volatile uint32_t gBytes, gIrqs, gOther;   /* read with a debugger: bytes echoed, receive interrupts, other interrupt causes */

void UART_0_INST_IRQHandler(void)
{
    gIrqs++;
    uint32_t iidx = DL_UART_Main_getPendingInterrupt(UART_0_INST);
    if (iidx != DL_UART_MAIN_IIDX_RX) { gOther = iidx; }
    if (iidx == DL_UART_MAIN_IIDX_RX) {
        /* The next byte can arrive before this handler finishes: the receive FIFO holds it and this loop drains it. */
        while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
            gBytes++;
            uint8_t c = DL_UART_Main_receiveData(UART_0_INST);
            DL_UART_Main_transmitDataBlocking(UART_0_INST, c);
#ifdef DEBUG_STATS
            if (c == '\n') { log_s("C1 stats"); log_kv("bytes", gBytes); log_kv("irqs", gIrqs); log_kv("other", gOther); log_nl(); }
#endif
            DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
        }
    }
}

int main(void)
{
    SYSCFG_DL_init();
    delay_cycles(16000000);                  /* 0.5 s: lets a debugger reset finish before the banner is sent */
    log_s("C1 start uart-echo-lpm3 l1306 9600 baud"); log_nl();
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
#if defined(POLICY_SLEEP)
    DL_SYSCTL_setPowerPolicyRUN0SLEEP0();    /* control build: CPU clock gated but the bus clocks keep running */
#elif defined(POLICY_STANDBY1)
    DL_SYSCTL_setPowerPolicySTANDBY1();      /* needs the LFCLK UART (example-lfclk.syscfg) */
#else
    DL_SYSCTL_setPowerPolicySTANDBY0();
#endif
    DL_SYSCTL_enableSleepOnExit();           /* the main loop never runs again: only the interrupt handler executes */
    for (;;) { __WFI(); }
}
