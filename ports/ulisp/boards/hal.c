// Board layer for uLisp on the LP-MSPM0G5187 (default), LP-MSPM0G3519 (-DFRL_G3519) and LP-MSPM0G3507 (-DFRL_G3507):
// 80 MHz clock, back-channel UART with a receive interrupt, three LEDs, a millisecond and microsecond clock.
// Clock code is the same as ports/wasm3/boards/g3519/hal.c (itself written fresh from the DriverLib headers and the data sheets).
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "frl_hal.h"

#if defined(FRL_G3519) || defined(FRL_G3507)
  #define UART_INST UART0
  #define UART_IRQN UART0_INT_IRQn
  #define UART_ISR UART0_IRQHandler
  #define TX_MUX_FN IOMUX_PINCM21_PF_UART0_TX
  #define RX_MUX_FN IOMUX_PINCM22_PF_UART0_RX
  static GPIO_Regs *const led_port[3] = { GPIOB, GPIOB, GPIOB };
  static const uint32_t led_pin[3] = { DL_GPIO_PIN_27, DL_GPIO_PIN_26, DL_GPIO_PIN_22 };
  static const uint32_t led_mux[3] = { IOMUX_PINCM58, IOMUX_PINCM57, IOMUX_PINCM50 };
#else   // LP-MSPM0G5187
  #define UART_INST UC0
  #define UART_IRQN UC0_INT_IRQn
  #define UART_ISR UC0_IRQHandler
  #define TX_MUX_FN IOMUX_PINCM21_PF_UC0_TX
  #define RX_MUX_FN IOMUX_PINCM22_PF_UC0_RX
  static GPIO_Regs *const led_port[3] = { GPIOA, GPIOB, GPIOA };
  static const uint32_t led_pin[3] = { DL_GPIO_PIN_17, DL_GPIO_PIN_13, DL_GPIO_PIN_24 };
  static const uint32_t led_mux[3] = { IOMUX_PINCM39, IOMUX_PINCM30, IOMUX_PINCM54 };
#endif

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

// Millisecond count kept by SysTick; microseconds read from its down-counter.
static volatile uint32_t tick_ms;
void SysTick_Handler(void) { tick_ms++; }
uint32_t hal_ms(void) { return tick_ms; }
uint32_t hal_us(void) {
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (cpu_hz / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

// UART at 115200 8N1; receive bytes go into a ring buffer from the interrupt.
#define RXN 512
static volatile uint8_t rxbuf[RXN];
static volatile uint16_t rxh, rxt;
static void rx_drain(void) {
    while (!DL_UART_Main_isRXFIFOEmpty(UART_INST)) {
        uint8_t c = DL_UART_Main_receiveData(UART_INST);
        uint16_t n = (rxh + 1) % RXN;
        if (n != rxt) { rxbuf[rxh] = c; rxh = n; }
    }
}
void UART_ISR(void) {
    rx_drain();
    DL_UART_Main_clearInterruptStatus(UART_INST, DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
}
int hal_getc(void) {
    if (rxt == rxh) return -1;
    uint8_t c = rxbuf[rxt]; rxt = (rxt + 1) % RXN; return c;
}
static void uart_init(void) {
    DL_UART_Main_reset(UART_INST);
    DL_UART_Main_enablePower(UART_INST);
    delay_cycles(32);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM21, TX_MUX_FN);   // PA10, the LaunchPad back-channel
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM22, RX_MUX_FN);    // PA11
    static const DL_UART_Main_ClockConfig ck = { .clockSel = DL_UART_MAIN_CLOCK_BUSCLK, .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1 };
    static const DL_UART_Main_Config cfg = {
        .mode = DL_UART_MAIN_MODE_NORMAL, .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE, .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS, .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_setClockConfig(UART_INST, (DL_UART_Main_ClockConfig *)&ck);
    DL_UART_Main_init(UART_INST, (DL_UART_Main_Config *)&cfg);
    DL_UART_Main_configBaudRate(UART_INST, cpu_hz == 80000000 ? 40000000 : 32000000, 115200);
    DL_UART_Main_enableFIFOs(UART_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_enableInterrupt(UART_INST, DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Main_enable(UART_INST);
    NVIC_ClearPendingIRQ(UART_IRQN);
    NVIC_EnableIRQ(UART_IRQN);
}
void hal_putc(int c) {
    while (DL_UART_Main_isTXFIFOFull(UART_INST)) {}
    DL_UART_Main_transmitData(UART_INST, (uint8_t)c);
}

// LEDs: index 0..2 = Arduino pins 13..15.
int hal_pin_count(void) { return 3; }
void hal_pin_mode(int i, int mode) {
    if (mode == 1) { DL_GPIO_initDigitalOutput(led_mux[i]); DL_GPIO_enableOutput(led_port[i], led_pin[i]); }
    else {
        DL_GPIO_disableOutput(led_port[i], led_pin[i]);
        if (mode == 2) DL_GPIO_initDigitalInputFeatures(led_mux[i], DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        else if (mode == 3) DL_GPIO_initDigitalInputFeatures(led_mux[i], DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        else DL_GPIO_initDigitalInput(led_mux[i]);
    }
}
void hal_pin_write(int i, int v) { if (v) DL_GPIO_setPins(led_port[i], led_pin[i]); else DL_GPIO_clearPins(led_port[i], led_pin[i]); }
int hal_pin_read(int i) {
    if (led_port[i]->DOE31_0 & led_pin[i]) return (led_port[i]->DOUT31_0 & led_pin[i]) != 0;   // an output reads back what it drives
    return DL_GPIO_readPins(led_port[i], led_pin[i]) != 0;
}

void hal_init(void) {
    DL_GPIO_reset(GPIOA); DL_GPIO_reset(GPIOB);
    DL_GPIO_enablePower(GPIOA); DL_GPIO_enablePower(GPIOB);
    delay_cycles(32);
    clock_init();
    SysTick->LOAD = cpu_hz / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    uart_init();
#ifdef FRL_LED_OUT   // set by build.sh with AUTOEXEC: LED 1 is an output from reset, so blink.py's single read of the output-enable register sees it
    hal_pin_mode(0, 1);
    hal_pin_write(0, 0);
#endif
}
