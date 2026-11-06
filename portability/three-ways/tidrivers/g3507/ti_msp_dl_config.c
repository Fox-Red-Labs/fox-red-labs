/* Three ways, TI Drivers: power, clocks and the internal reference, in DriverLib.
 * TI Drivers do not power peripherals or the VREF on MSPM0; TI's examples do it here.
 * Clock lines are TI's adc_singlechannel example's; the VREF lines are what SysConfig generates
 * for the DriverLib way (2.5 V, bus clock). */
#include "ti_msp_dl_config.h"

static const DL_VREF_ClockConfig vrefClock = {
    .clockSel = DL_VREF_CLOCK_BUSCLK, .divideRatio = DL_VREF_CLOCK_DIVIDE_1};
static const DL_VREF_Config vrefConfig = {.vrefEnable = DL_VREF_ENABLE_ENABLE,
    .bufConfig = DL_VREF_BUFCONFIG_OUTPUT_2_5V, .shModeEnable = DL_VREF_SHMODE_DISABLE,
    .holdCycleCount = DL_VREF_HOLD_MIN, .shCycleCount = DL_VREF_SH_MIN};

void SYSCFG_DL_init(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_UART_reset(UART0);
    DL_ADC12_reset(ADC0);
    DL_VREF_reset(VREF);
    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_UART_enablePower(UART0);
    DL_ADC12_enablePower(ADC0);
    DL_VREF_enablePower(VREF);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1);
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    DL_VREF_setClockConfig(VREF, (DL_VREF_ClockConfig *) &vrefClock);
    DL_VREF_configReference(VREF, (DL_VREF_Config *) &vrefConfig);
    while (DL_VREF_getStatus(VREF) == DL_VREF_CTL1_READY_NOTRDY) {
    }
}
