/* Test application for the stm32-bootloader port. Linked at 0x00010000.
 * Prints "APP OK v<APP_VER>" once, then a "APP TICK" line from a timer interrupt every 250 ms,
 * so a bootloader that does not move the vector table (VTOR) before the jump is caught. */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#ifndef APP_VER
#define APP_VER 1
#endif
#define STR2(x) #x
#define STR(x) STR2(x)
/* padding so that v2 spans several 1024-byte packets */
#if APP_VER == 2
volatile const uint8_t pad[3000] = { [0] = 1, [2999] = 2 };
#endif
static volatile uint32_t ticks;
static void put(const char *s) { while (*s) DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)*s++); }
void TIMER_TICK_INST_IRQHandler(void) {
    if (DL_TimerG_getPendingInterrupt(TIMER_TICK_INST) == DL_TIMER_IIDX_ZERO) ticks++;
}
int main(void) {
    SYSCFG_DL_init();
    NVIC_EnableIRQ(TIMER_TICK_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_TICK_INST);
    put("\r\nAPP OK v" STR(APP_VER) "\r\n");
#if APP_VER == 2
    if (pad[0] != 1) put("pad?\r\n");
#endif
    uint32_t seen = 0;
    while (1) {
        if (ticks != seen) { seen = ticks; put("APP TICK\r\n"); }
    }
}
