/* Test harness, not part of the MSPM0 program: samples the two PWM pins (PA3, PA4) from the SysTick interrupt, which
   starts before main(), and counts how often each reads high. Read `probe` over the debug probe after a few seconds. */
#include "ti_msp_dl_config.h"
#define N 400000u
volatile struct { uint32_t magic, samples, high_pa3, high_pa4, edges_pa3; } probe;
static uint32_t last3;
void SysTick_Handler(void)
{
    if (probe.samples >= N) { SysTick->CTRL = 0; probe.magic = 0x50574d31u; return; }
    /* a pin set to a peripheral output function has its input buffer off; turn it on so DIN shows the pin */
    IOMUX->SECCFG.PINCM[3] |= IOMUX_PINCM_INENA_ENABLE;
    IOMUX->SECCFG.PINCM[4] |= IOMUX_PINCM_INENA_ENABLE;
    uint32_t d = GPIO0->DIN31_0;
    probe.samples++;
    probe.high_pa3 += (d >> 3) & 1u;
    probe.high_pa4 += (d >> 4) & 1u;
    probe.edges_pa3 += (((d >> 3) & 1u) != last3);
    last3 = (d >> 3) & 1u;
}
__attribute__((constructor)) static void probe_start(void) { SysTick_Config(1999u); }
