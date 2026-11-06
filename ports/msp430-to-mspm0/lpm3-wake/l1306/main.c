/* Port of msp430fr243x_LPM3_01 (MSP430FR2433: ACLK from the 32768 Hz crystal, watchdog as a 1 s interval timer, LPM3, LED toggled in
 * the interrupt) to the LP-MSPM0L1306. Written fresh from the MSPM0 DriverLib. Differences from the original are listed in PORT.md.
 * Added for measurement: a line on the serial port per wake, and a SysTick reading across the sleep. SysTick runs from the CPU
 * clock, so the count between going to sleep and waking shows whether the CPU clock really stopped. */
#include "ti_msp_dl_config.h"

#define LOG_PUTC(c) DL_UART_Main_transmitDataBlocking(UART_0_INST, (c))
#define LOG_IDLE()  while (DL_UART_Main_isBusy(UART_0_INST)) { }
#include "log.h"

#ifndef SLEEP_TICKS
#define SLEEP_TICKS 5      /* the first ticks use SLEEP (CPU clock gated only) as the control; later ticks use STANDBY0 */
#endif

static volatile uint32_t gTick;

void GROUP0_IRQHandler(void)
{
    if (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_0) == DL_INTERRUPT_GROUP0_IIDX_WWDT0) {
        if (DL_WWDT_getPendingInterrupt(WWDT0)) { gTick++; }
    }
}

int main(void)
{
    SYSCFG_DL_init();
    log_s("B1 start lpm3-wake l1306 reset_cause="); log_u((uint32_t) DL_SYSCTL_getResetCause()); log_nl();
    NVIC_EnableIRQ(WWDT0_INT_IRQN);
    SysTick->LOAD = 0x00FFFFFFu; SysTick->VAL = 0; SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    for (;;) {
        uint32_t seen = gTick;
        int deep = (seen >= SLEEP_TICKS);
        if (deep) { DL_SYSCTL_setPowerPolicySTANDBY0(); } else { DL_SYSCTL_setPowerPolicyRUN0SLEEP0(); }
        uint32_t t0 = SysTick->VAL;
        while (gTick == seen) { __WFI(); }
        uint32_t d = (t0 - SysTick->VAL) & 0x00FFFFFFu;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
        log_s("B1 tick"); log_kv("n", gTick); log_s(deep ? " mode=STANDBY0" : " mode=SLEEP"); log_kv("systick_during_sleep", d); log_nl();
    }
}
