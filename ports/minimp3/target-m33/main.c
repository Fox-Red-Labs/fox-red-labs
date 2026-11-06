/* LP-MSPM33C321A: decode two MP3 clips with minimp3 at 160 MHz, DWT cycle counter, results in RAM. No UART, screen or audio is touched. */
#include "ti_msp_dl_config.h"
void mp3bench(uint32_t (*now)(void));
static uint32_t cyc(void) { return DWT->CYCCNT; }
int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    mp3bench(cyc);
    for (;;) { __asm volatile ("nop"); }
}
