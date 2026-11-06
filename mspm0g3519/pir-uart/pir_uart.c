/* HC-SR501 PIR OUT on J3 pin 23 (PA27). Prints "PIR <0|1> t=<ms>" 4 times a second and
 * "CHANGE PIR <0|1> t=<ms>" on every change, 115200 8N1 on UART0 (PA10 TX), the XDS110 backchannel. */
#include "ti_msp_dl_config.h"
#include <stdio.h>

static volatile uint32_t gMs;
void SysTick_Handler(void) { gMs++; }

static void put(const char *s)
{
    while (*s) DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) *s++);
}

static int pir(void) { return DL_GPIO_readPins(GPIO_PIR_PORT, GPIO_PIR_OUT_PIN) ? 1 : 0; }

int main(void)
{
    char b[48];
    SYSCFG_DL_init();
    DL_SYSTICK_config(32000);                 /* 32 MHz CPU clock: 1 ms tick */
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();
    put("\r\nPIR logger: OUT on PA27 (J3 pin 23)\r\n");
    int last = pir();
    uint32_t next = 250;
    for (;;) {
        uint32_t t = gMs;
        int v = pir();
        if (v != last) {
            last = v;
            snprintf(b, sizeof b, "CHANGE PIR %d t=%lu\r\n", v, (unsigned long) t);
            put(b);
        }
        if ((int32_t) (t - next) >= 0) {
            next += 250;
            snprintf(b, sizeof b, "PIR %d t=%lu\r\n", v, (unsigned long) t);
            put(b);
        }
    }
}
