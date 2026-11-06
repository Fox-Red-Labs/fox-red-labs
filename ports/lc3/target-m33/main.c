/* LP-MSPM33C321A: LC3 encode and decode of a generated test signal at three LE Audio configurations, 160 MHz, DWT cycle counter.
 * Results stay in RAM for the probe to read. No UART, screen or audio is touched; no pin is driven. */
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "lc3run.h"
volatile lc3res_t lc3_res[3];
lc3detail_t lc3_det[3];
volatile uint32_t lc3_stack_free;      /* bytes of the stack never touched, found by painting */
volatile uint32_t lc3_done;            /* 0x4C433344 when all three finished */
extern uint32_t __stack;               /* lowest stack address (linker) */
static uint32_t cyc(void) { return DWT->CYCCNT; }
int main(void)
{
    static const int cfg[3][2] = { {16000, 32}, {24000, 48}, {48000, 96} };
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t *lo = &__stack, *sp = (uint32_t *)__builtin_frame_address(0);
    for (uint32_t *p = lo; p < sp - 64; p++) *p = 0xDEADBEEFu;
    for (int i = 0; i < 3; i++) {
        static lc3res_t r;
        lc3run(cfg[i][0], cfg[i][1], &r, &lc3_det[i], cyc);
        for (unsigned k = 0; k < sizeof r / 4; k++) ((volatile uint32_t *)&lc3_res[i])[k] = ((uint32_t *)&r)[k];
    }
    uint32_t *q = lo; while (q < sp && *q == 0xDEADBEEFu) q++;
    lc3_stack_free = (uint32_t)((char *)q - (char *)lo);
    lc3_done = 0x4C433344u;
    for (;;) { __asm volatile ("nop"); }
}
