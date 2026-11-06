/* LP-MSPM33C321A: Plaits voice, 24 engines, 256 blocks of 12 samples each, 160 MHz, DWT cycle counter.
 * Results stay in RAM for the probe to read. No UART, screen, DAC or audio is touched; no pin is driven. */
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "plaitsrun.h"
volatile uint32_t pl_done;            /* 0x504C3254 when all engines finished */
volatile uint32_t pl_stack_free;      /* bytes of the stack never touched, found by painting */
extern "C" uint32_t __stack;
static uint32_t cyc(void) { return DWT->CYCCNT; }
int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t *lo = &__stack, *sp = (uint32_t *)__builtin_frame_address(0);
    for (uint32_t *p = lo; p < sp - 64; p++) *p = 0xDEADBEEFu;
#ifdef PL_KEEP_ENGINE
    pl_keep_engine = PL_KEEP_ENGINE;
#endif
    for (int e = 0; e < PL_ENGINES; e++) pl_run_engine(e, cyc);
    uint32_t *q = lo; while (q < sp && *q == 0xDEADBEEFu) q++;
    pl_stack_free = (uint32_t)((char *)q - (char *)lo);
    pl_done = 0x504C3254u;
    for (;;) { __asm volatile ("nop"); }
}
