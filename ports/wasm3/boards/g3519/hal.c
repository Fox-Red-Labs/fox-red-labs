// Board layer for the LP-MSPM0G3519 (and, with -DFRL_G3507, the LP-MSPM0G3507): clock, console UART, LED, microsecond timer.
// Written fresh from the MSPM0 DriverLib headers and the data sheets, following the clock and UART code in ports/micropython/mphalport.c.
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "../../common/frl_hal.h"

#define LED_PORT GPIOB
#define LED_PIN  DL_GPIO_PIN_27          // green LED on the LaunchPad, PB27
#define LED_IOMUX IOMUX_PINCM58

static uint32_t cpu_hz = 32000000;
uint32_t hal_cpu_hz(void) { return cpu_hz; }

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
    cpu_hz = 80000000;
}

// Microsecond timer: SysTick counts down from a 1 ms reload; the millisecond count is kept in the interrupt.
static volatile uint32_t tick_ms;
void SysTick_Handler(void) { tick_ms++; }
uint32_t hal_us(void) {
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (cpu_hz / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

static void uart_init(void) {
    DL_UART_Main_reset(UART0);
    DL_UART_Main_enablePower(UART0);
    delay_cycles(32);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM21, IOMUX_PINCM21_PF_UART0_TX);   // PA10, the LaunchPad back-channel
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM22, IOMUX_PINCM22_PF_UART0_RX);    // PA11
    static const DL_UART_Main_ClockConfig ck = { .clockSel = DL_UART_MAIN_CLOCK_BUSCLK, .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1 };
    static const DL_UART_Main_Config cfg = {
        .mode = DL_UART_MAIN_MODE_NORMAL, .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE, .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS, .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_setClockConfig(UART0, (DL_UART_Main_ClockConfig *)&ck);
    DL_UART_Main_init(UART0, (DL_UART_Main_Config *)&cfg);
    DL_UART_Main_configBaudRate(UART0, cpu_hz == 80000000 ? 40000000 : 32000000, 115200);
    DL_UART_Main_enableFIFOs(UART0);
    DL_UART_Main_enable(UART0);
}
void hal_putc(int c) {
    while (DL_UART_Main_isTXFIFOFull(UART0)) {}
    DL_UART_Main_transmitData(UART0, (uint8_t)c);
}
void hal_led(int on) { if (on) DL_GPIO_setPins(LED_PORT, LED_PIN); else DL_GPIO_clearPins(LED_PORT, LED_PIN); }

void hal_init(void) {
    DL_GPIO_reset(GPIOA); DL_GPIO_reset(GPIOB);
    DL_GPIO_enablePower(GPIOA); DL_GPIO_enablePower(GPIOB);
    delay_cycles(32);
    clock_init();
    SysTick->LOAD = cpu_hz / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    uart_init();
    DL_GPIO_initDigitalOutput(LED_IOMUX);
    DL_GPIO_clearPins(LED_PORT, LED_PIN);
    DL_GPIO_enableOutput(LED_PORT, LED_PIN);
}
