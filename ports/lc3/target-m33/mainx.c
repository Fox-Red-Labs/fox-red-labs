/* LP-MSPM33C321A: LC3 extras (7.5, 5 and 2.5 ms frames, packet loss concealment, two channels), 160 MHz, DWT cycle counter.
 * Results stay in RAM for the probe to read. No UART, screen or audio is touched; no pin is driven. */
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "lc3x.h"
#include "lc3xcfg.h"
volatile lc3xres_t lc3x_res[LC3X_N];
volatile uint32_t lc3x_stack_free;
volatile uint32_t lc3x_done;           /* 0x4C583344 when all runs finished */
extern uint32_t __stack;
static uint32_t cyc(void) { return DWT->CYCCNT; }
int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t *lo = &__stack, *sp = (uint32_t *)__builtin_frame_address(0);
    for (uint32_t *p = lo; p < sp - 64; p++) *p = 0xDEADBEEFu;
    for (int i = 0; i < LC3X_N; i++) {
        static lc3xres_t r;
        lc3xrun(lc3x_cfg[i][0], lc3x_cfg[i][1], lc3x_cfg[i][2], lc3x_cfg[i][3], lc3x_cfg[i][4], &r, cyc);
        for (unsigned k = 0; k < sizeof r / 4; k++) ((volatile uint32_t *)&lc3x_res[i])[k] = ((uint32_t *)&r)[k];
    }
    uint32_t *q = lo; while (q < sp && *q == 0xDEADBEEFu) q++;
    lc3x_stack_free = (uint32_t)((char *)q - (char *)lo);
    lc3x_done = 0x4C583344u;
    for (;;) { __asm volatile ("nop"); }
}
