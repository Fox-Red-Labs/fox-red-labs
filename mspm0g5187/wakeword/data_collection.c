/*
 * Fox Red Labs replacement for TI's data_collection.c (wakeword_detection_ai_npu): the microphone is the
 * LP-MSPM0G5187's own ICS-40720 into the PCM1809 audio ADC, read over I2S (PB10 WCLK, PB11 BCLK,
 * PB12 data) with the DMA setup of TI's tinyusb/device_uac_microphone_i2s; frl_report.c decimates
 * 47,619 Hz to 7,936.5 Hz and hands each sample to the example's buffer_manager. The replay build
 * skips the microphone and takes samples from the serial port instead.
 */
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "frl_report.h"
#include "data_collection.h"

volatile uint32_t samples_count = 0;

/* TI's clockConfig(), verbatim from the shipped data_collection.c: it re-trims SYSOSC (4 MHz, then base) and sets the PLL by hand */
#define HWREG(x)                            (*((volatile uint32_t *)(x)))

void clockConfig(void)
{
    HWREG(0x400B1404)  = 0x81071407;
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_4M);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);

    /*********************     SYSPLL Configurations      ***************************/
    SYSCTL->SOCLOCK.SYSPLLCFG0  = ( SYSCTL_SYSPLLCFG0_ENABLECLK0_ENABLE|SYSCTL_SYSPLLCFG0_ENABLECLK1_ENABLE |0x0 <<SYSCTL_SYSPLLCFG0_RDIVCLK1_OFS | 0x0 <<SYSCTL_SYSPLLCFG0_RDIVCLK0_OFS); // SYSPLLCLK0 / 1 enable and Rdiv setting , Actual R divide =2*(Rdiv0 +1)
    DL_Common_updateReg(&SYSCTL->SOCLOCK.SYSPLLCFG0 , SYSCTL_SYSPLLCFG0_SYSPLLREF_SYSOSC, SYSCTL_SYSPLLCFG0_SYSPLLREF_MASK ); // Enable sysosc as SYPLL ref source
    SYSCTL->SOCLOCK.SYSPLLCFG1  = (SYSCTL_SYSPLLCFG1_PDIV_REFDIV1| 0x4 << SYSCTL_SYSPLLCFG1_QDIV_OFS);      // Pdiv and qdiv settings fvco = (syspll _refclk * (qdiv+1))/(2^(pdiv))

    /*********************     TRim values for 32- 48 MHz Range  ***************************/
    SYSCTL->SOCLOCK.SYSPLLPARAM0    =   0x05060A;                   //    Load PARAM0 trim setting from TRM SYSPLL Parameter Lookup according to floopin freq
    SYSCTL->SOCLOCK.SYSPLLPARAM1    =   0x1000110;                  //    Load PARAM1 trim setting from TRM SYSPLL Parameter Lookup according to floopin freq
    HWREG(0x400B1450)               =   0x80FA0;                    //    PLL TRIM 0
    HWREG(0x400B1454)               =   0xA28;                      //    PLL TRIM 1

    DL_Common_updateReg(&SYSCTL->SOCLOCK.HSCLKEN,  SYSCTL_HSCLKEN_SYSPLLEN_ENABLE, SYSCTL_HSCLKEN_SYSPLLEN_MASK );         // Enable PLL
    // Need wait PLL Lock
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    while (!(DL_SYSCTL_getClockStatus() & DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD));

    SYSCTL->SOCLOCK.HSCLKCFG = SYSCTL_HSCLKCFG_HSCLKSEL_SYSPLL;   // To select either HFCLK or SYSPLL as High speed clock source
    SYSCTL->SOCLOCK.MCLKCFG = (SYSCTL_MCLKCFG_USEHSCLK_ENABLE| 0x210);  // To select either SYSOSC or HSCCLK as MCLK
}




void mic_init(void)
{
    SYSCFG_DL_init();
    clockConfig();
    frl_audio_start();
}
