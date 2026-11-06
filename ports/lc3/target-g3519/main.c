/* Cortex-M0+ model run: the bench for an MSPM0 LaunchPad core (no FPU), built with arm-none-eabi-gcc and run in the Unicorn emulator
 * (tests/emu_m0.py), which answers reads of 0x40000F00 with a MODELLED cycle count. Not for a board: there is no clock setup. */
#include <stdint.h>
#include "lc3run.h"
volatile lc3res_t lc3_res[3];
lc3detail_t lc3_det[3];
volatile uint32_t lc3_done;
static uint32_t now(void) { return *(volatile uint32_t *)0x40000F00u; }
int main(void)
{
    static const int cfg[3][2] = { {16000, 32}, {24000, 48}, {48000, 96} };
    for (int i = 0; i < 3; i++) {
        static lc3res_t r;
        lc3run(cfg[i][0], cfg[i][1], &r, &lc3_det[i], now);
        for (unsigned k = 0; k < sizeof r / 4; k++) ((volatile uint32_t *)&lc3_res[i])[k] = ((uint32_t *)&r)[k];
    }
    lc3_done = 0x4C433344u;
    for (;;) { __asm volatile ("nop"); }
}
