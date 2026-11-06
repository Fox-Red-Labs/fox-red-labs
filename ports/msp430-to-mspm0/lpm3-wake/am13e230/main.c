/* Port of msp430fr243x_LPM3_01 to the LP-AM13E230: same program as the LP-MSPM0L1306 port (lpm3-wake/l1306/main.c) on the AM13 driver
 * library. Differences: Device_Init(), AM13 power-policy names (RUNSLEEP, STANDBY0), WWDT interrupt handler named by SysConfig.
 * SysTick runs from the CPU clock, so its count across the sleep shows whether the CPU clock stopped. See PORT.md. */
#include "device.h"
#include "ti_sdk_dl_config.h"

#define LOG_PUTC(c) DL_UART_transmitDataBlocking(APP_UART_0_INST, (c))
#define LOG_IDLE()  DEVICE_DELAY_MS(2)
#include "log.h"

#ifndef DEEP_POLICY               /* STOP: the interval timer in power domain 0 wakes it. STANDBY0 is the other deep mode; see PORT.md */
#define DEEP_POLICY STOP
#endif
#define POLICY_CALL_(m) DL_SYSCTL_setPowerPolicy##m()
#define POLICY_CALL(m)  POLICY_CALL_(m)
#define STR_(x) #x
#define STR(x)  STR_(x)
#ifndef SLEEP_TICKS
#define SLEEP_TICKS 5
#endif

static volatile uint32_t gTick;

void APP_WWDT_0_INT_Handler(void) { gTick++; }

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    log_s("B1 start lpm3-wake am13e230 reset_cause="); log_u((uint32_t) DL_SYSCTL_getResetCause()); log_nl();
    SysTick->LOAD = 0x00FFFFFFu; SysTick->VAL = 0; SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    for (;;) {
        uint32_t seen = gTick;
        int deep = (seen >= SLEEP_TICKS);
        if (deep) { POLICY_CALL(DEEP_POLICY); } else { DL_SYSCTL_setPowerPolicyRUNSLEEP(); }
        uint32_t t0 = SysTick->VAL;
        while (gTick == seen) { __WFI(); }
        uint32_t d = (t0 - SysTick->VAL) & 0x00FFFFFFu;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
        log_s("B1 tick"); log_kv("n", gTick); log_s(deep ? " mode=" STR(DEEP_POLICY) : " mode=SLEEP"); log_kv("systick_during_sleep", d); log_nl();
    }
}
