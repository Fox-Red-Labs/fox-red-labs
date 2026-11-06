/* LP-MSPM0G3519 (Cortex-M0+, no FPU, 80 MHz): decode two MP3 clips with minimp3, results in RAM. 
 * The clock code is the 80 MHz SYSPLL setup of ports/wasm3/boards/g3519/hal.c (same repository). Timing is a 32-bit count: SysTick (24 bit, down) plus an overflow
 * counter in its interrupt. With -DMP3_EMU the clock setup is left out (the emulator has no PLL). */
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
void mp3bench(uint32_t (*now)(void));
// SYSPLL to 80 MHz CPU, 40 MHz bus. Errata SYSPLL_ERR_01: check the PLL against the oscillator with the frequency counter and retry.
static const DL_SYSCTL_SYSPLLConfig pll_cfg = {
    .inputFreq = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
    .rDivClk2x = 3, .rDivClk1 = 1, .rDivClk0 = 0,
    .enableCLK2x = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
    .enableCLK1 = DL_SYSCTL_SYSPLL_CLK1_ENABLE,
    .enableCLK0 = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
    .sysPLLMCLK = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
    .sysPLLRef = DL_SYSCTL_SYSPLL_REF_SYSOSC,
    .qDiv = 9,
    .pDiv = DL_SYSCTL_SYSPLL_PDIV_2,
};

static uint32_t fcc_count(DL_SYSCTL_FCC_CLOCK_SOURCE src) {
    uint32_t guard;
    DL_SYSCTL_setFCCPeriods(DL_SYSCTL_FCC_TRIG_CNT_01);
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE, DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK, src);
    DL_SYSCTL_startFCC();
    for (guard = 0; !DL_SYSCTL_isFCCDone() && guard < 70; guard++) delay_cycles(977);
    return DL_SYSCTL_readFCC();
}
static int pll_ratio_ok(void) {
    uint32_t pll = fcc_count(DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK1);
    uint32_t osc = fcc_count(DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    if (osc == 0) return 0;
    uint32_t ratio = (pll * 1000u) / osc;
    return ratio > 1246 && ratio < 1254;
}
static void clock_init(void) {
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *)&pll_cfg);
    int tries = 0;
    while (!pll_ratio_ok()) {
        if (++tries >= 20) { DL_SYSCTL_disableSYSPLL(); return; }   // stay on the 32 MHz oscillator
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();
        while ((DL_SYSCTL_getClockStatus() & DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD) == 0) {}
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
    
}


#ifdef MP3_EMU
static uint32_t st_now(void) { return *(volatile uint32_t *)0x40000F00u; }   /* the emulator's modelled cycle count */
#else
/* 32-bit count from the 24-bit SysTick plus an overflow counter bumped by the SysTick interrupt (every 2^24 cycles = 0.21 s).
 * Read with interrupts off; if the wrap has happened but the interrupt has not run yet (PENDSTSET), the counter is one behind. */
volatile uint32_t st_ovf;
void SysTick_Handler(void) { st_ovf++; }
static uint32_t st_now(void) {
    uint32_t pm = __get_PRIMASK(); __disable_irq();
    uint32_t o = st_ovf, el = 0xFFFFFFu - SysTick->VAL;
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) && el < 0x800000u) o++;   /* wrapped, interrupt not taken yet */
    if (!pm) __enable_irq();
    return (o << 24) | el;
}
#endif
/* Clock check and end-to-end time, read by the host: g_val[0..2] = cycles counted across delay_cycles(80M), (160M), then the whole bench;
 * g_tick is refreshed in the idle loop so the host can time the counter against its own clock after the run. */
volatile uint32_t g_val[4] = {0, 0, 0, 0};
volatile uint32_t g_tick;
int main(void)
{
#ifndef MP3_EMU
    clock_init();
    SysTick->LOAD = 0xFFFFFFu; SysTick->VAL = 0; SysTick->CTRL = 7;   /* processor clock, interrupt on wrap, enabled */
    { uint32_t t = st_now(); delay_cycles(80000000u); g_val[0] = st_now() - t; t = st_now(); delay_cycles(160000000u); g_val[1] = st_now() - t; }
#endif
    uint32_t t0 = st_now();
    mp3bench(st_now);
    g_val[2] = st_now() - t0; g_val[3] = 0x434B4F4Bu;
    for (;;) { g_tick = st_now(); }
}
