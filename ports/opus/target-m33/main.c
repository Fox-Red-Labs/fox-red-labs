/* LP-MSPM33C321A: Opus encode and decode of a generated test signal, 160 MHz, DWT cycle counter.
 * Results stay in RAM for the probe to read. No UART, screen or audio is touched; no pin is driven. */
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "opusrun.h"
#include "opuscfg.h"
volatile oprun_t op_res[OP_N];
uint32_t op_det[OP_N][200];            /* per run: 100 packet CRCs, then 100 decoded-frame CRCs */
volatile uint32_t op_stack_free;       /* bytes of the stack never touched, found by painting */
volatile uint32_t op_done;             /* 0x4F505244 when all runs finished */
extern uint32_t __stack;
static uint32_t cyc(void) { return DWT->CYCCNT; }
int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t *lo = &__stack, *sp = (uint32_t *)__builtin_frame_address(0);
    for (uint32_t *p = lo; p < sp - 64; p++) *p = 0xDEADBEEFu;
    for (int i = 0; i < OP_N; i++) {
        static oprun_t r; const int *c = op_cfg[i / OP_NCPLX];
        oprun(c[0], c[1], c[2], c[3], op_cplx[i % OP_NCPLX], &r, op_det[i], op_det[i] + 100, cyc);
        for (unsigned k = 0; k < sizeof r / 4; k++) ((volatile uint32_t *)&op_res[i])[k] = ((uint32_t *)&r)[k];
    }
    uint32_t *q = lo; while (q < sp && *q == 0xDEADBEEFu) q++;
    op_stack_free = (uint32_t)((char *)q - (char *)lo);
    op_done = 0x4F505244u;
    for (;;) { __asm volatile ("nop"); }
}
