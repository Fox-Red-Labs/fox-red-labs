// Clock, console UART, tick counter and pin helpers for the MSPM0G3519 and MSPM0G3507.
// Written from the MSPM0 DriverLib headers and the data sheet; no TI example code is copied.
#include <string.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/mpstate.h"
#include "py/stream.h"
#include "shared/runtime/interrupt_char.h"
#include "mphalport.h"

#ifndef FRL_UART_BAUD
#define FRL_UART_BAUD 115200
#endif
#ifndef FRL_RX_RING
#define FRL_RX_RING 128
#endif

// ---- interrupt masking -------------------------------------------------------------------

uint32_t frl_irq_disable(void) {
    uint32_t primask;
    __asm volatile ("mrs %0, primask\n cpsid i" : "=r" (primask) :: "memory");
    return primask;
}

void frl_irq_restore(uint32_t state) {
    __asm volatile ("msr primask, %0" :: "r" (state) : "memory");
}

// ---- clocks ------------------------------------------------------------------------------
// The device starts on the 32 MHz internal oscillator. The PLL path takes it to 80 MHz and
// keeps the bus clock at 40 MHz (ULPCLK divided by 2), which is the data sheet maximum.
// Define FRL_NO_PLL to stay at 32 MHz.

static uint32_t cpu_hz = 32000000;
uint32_t frl_cpu_hz(void) {
    return cpu_hz;
}

#ifndef FRL_NO_PLL
static const DL_SYSCTL_SYSPLLConfig pll_cfg = {
    .inputFreq = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
    .rDivClk2x = 3,
    .rDivClk1 = 1,
    .rDivClk0 = 0,
    .enableCLK2x = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
    .enableCLK1 = DL_SYSCTL_SYSPLL_CLK1_ENABLE,
    .enableCLK0 = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
    .sysPLLMCLK = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
    .sysPLLRef = DL_SYSCTL_SYSPLL_REF_SYSOSC,
    .qDiv = 9,
    .pDiv = DL_SYSCTL_SYSPLL_PDIV_2,
};

// Errata SYSPLL_ERR_01: after every PLL enable, check the PLL against the oscillator with the
// frequency counter and retry until the ratio is right. A 1.25 ratio of CLK1 to SYSOSC is
// what this configuration produces; the check below
// measures CLK1 (40 MHz) against the 32 MHz oscillator: ratio 1.25 with 0.3 percent tolerance.
static int pll_ratio_ok(void) {
    uint32_t pll, osc, guard;
    DL_SYSCTL_setFCCPeriods(DL_SYSCTL_FCC_TRIG_CNT_01);
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE, DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK1);
    DL_SYSCTL_startFCC();
    for (guard = 0; !DL_SYSCTL_isFCCDone() && guard < 70; guard++) {
        delay_cycles(977);
    }
    pll = DL_SYSCTL_readFCC();
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE, DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    DL_SYSCTL_startFCC();
    for (guard = 0; !DL_SYSCTL_isFCCDone() && guard < 70; guard++) {
        delay_cycles(977);
    }
    osc = DL_SYSCTL_readFCC();
    if (osc == 0) {
        return 0;
    }
    uint32_t ratio = (pll * 1000u) / osc;
    return ratio > 1246 && ratio < 1254;
}
#endif

static void clock_init(void) {
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    #ifndef FRL_NO_PLL
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *)&pll_cfg);
    int tries = 0;
    while (!pll_ratio_ok()) {
        if (++tries >= 20) {
            // The PLL never locked cleanly: stay on the 32 MHz oscillator.
            DL_SYSCTL_disableSYSPLL();
            return;
        }
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();
        while ((DL_SYSCTL_getClockStatus() & DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD) == 0) {
        }
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
    cpu_hz = 80000000;
    #endif
}

// ---- tick counter: SysTick every millisecond ---------------------------------------------

static volatile uint32_t tick_ms;

void SysTick_Handler(void) {
    tick_ms++;
}

static void systick_init(void) {
    SysTick->LOAD = cpu_hz / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

mp_uint_t mp_hal_ticks_ms(void) {
    return tick_ms;
}

mp_uint_t mp_hal_ticks_us(void) {
    uint32_t ms, val;
    do {
        ms = tick_ms;
        val = SysTick->VAL;
    } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (cpu_hz / 1000000u);
}

mp_uint_t mp_hal_ticks_cpu(void) {
    return (tick_ms * (SysTick->LOAD + 1)) + (SysTick->LOAD - SysTick->VAL);
}

void mp_hal_delay_ms(mp_uint_t ms) {
    mp_uint_t start = tick_ms;
    while (tick_ms - start < ms) {
        MICROPY_EVENT_POLL_HOOK
    }
}

void mp_hal_delay_us(mp_uint_t us) {
    mp_uint_t start = mp_hal_ticks_us();
    while (mp_hal_ticks_us() - start < us) {
    }
}

uint64_t mp_hal_time_ns(void) {
    return (uint64_t)tick_ms * 1000000ull;
}

// ---- console UART0 on PA10 (TX) and PA11 (RX), the LaunchPad backchannel ----------------

static volatile uint8_t rx_buf[FRL_RX_RING];
static volatile uint16_t rx_head, rx_tail;

void UART0_IRQHandler(void) {
    while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
        uint8_t c = DL_UART_Main_receiveData(UART0);
        if (c == (uint8_t)mp_interrupt_char) {
            mp_sched_keyboard_interrupt();
            continue;
        }
        uint16_t next = (rx_head + 1) % FRL_RX_RING;
        if (next != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next;
        }
    }
    DL_UART_Main_clearInterruptStatus(UART0, DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
}

static void uart_init(void) {
    DL_UART_Main_reset(UART0);
    DL_UART_Main_enablePower(UART0);
    delay_cycles(32);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM21, IOMUX_PINCM21_PF_UART0_TX);
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM22, IOMUX_PINCM22_PF_UART0_RX);
    static const DL_UART_Main_ClockConfig ck = {
        .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
    };
    static const DL_UART_Main_Config cfg = {
        .mode = DL_UART_MAIN_MODE_NORMAL,
        .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_setClockConfig(UART0, (DL_UART_Main_ClockConfig *)&ck);
    DL_UART_Main_init(UART0, (DL_UART_Main_Config *)&cfg);
    // Bus clock is half the CPU clock when the PLL is on, and equal to it when it is not.
    DL_UART_Main_configBaudRate(UART0, cpu_hz == 80000000 ? 40000000 : 32000000, FRL_UART_BAUD);
    DL_UART_Main_enableFIFOs(UART0);
    DL_UART_Main_setRXFIFOThreshold(UART0, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_enableInterrupt(UART0, DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Main_enable(UART0);
    NVIC_EnableIRQ(UART0_INT_IRQn);
}

int mp_hal_stdin_rx_chr(void) {
    for (;;) {
        if (rx_head != rx_tail) {
            uint8_t c = rx_buf[rx_tail];
            rx_tail = (rx_tail + 1) % FRL_RX_RING;
            return c;
        }
        MICROPY_EVENT_POLL_HOOK
    }
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        while (DL_UART_Main_isTXFIFOFull(UART0)) {
        }
        DL_UART_Main_transmitData(UART0, (uint8_t)str[i]);
    }
    return len;
}

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    uintptr_t ret = 0;
    if ((poll_flags & MP_STREAM_POLL_RD) && rx_head != rx_tail) {
        ret |= MP_STREAM_POLL_RD;
    }
    if (poll_flags & MP_STREAM_POLL_WR) {
        ret |= MP_STREAM_POLL_WR;
    }
    return ret;
}

// ---- board init --------------------------------------------------------------------------

void frl_board_init(void) {
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    #ifdef FRL_G3519
    DL_GPIO_reset(GPIOC);
    #endif
    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    #ifdef FRL_G3519
    DL_GPIO_enablePower(GPIOC);
    #endif
    delay_cycles(32);
    clock_init();
    systick_init();
    uart_init();
}

// ---- pins --------------------------------------------------------------------------------

static GPIO_Regs *port_regs(mp_hal_pin_obj_t p) {
    switch (p->port) {
        case 0: return GPIOA;
        case 1: return GPIOB;
        default:
            #ifdef FRL_G3519
            return GPIOC;
            #else
            return GPIOB;
            #endif
    }
}

static uint32_t pin_mask(mp_hal_pin_obj_t p) {
    return 1u << p->num;
}

void mp_hal_pin_input(mp_hal_pin_obj_t pin) {
    DL_GPIO_disableOutput(port_regs(pin), pin_mask(pin));
    DL_GPIO_initDigitalInputFeatures(pin->iomux, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

void mp_hal_pin_output(mp_hal_pin_obj_t pin) {
    DL_GPIO_initDigitalOutput(pin->iomux);
    DL_GPIO_enableOutput(port_regs(pin), pin_mask(pin));
}

void mp_hal_pin_open_drain(mp_hal_pin_obj_t pin) {
    // Input buffer on with the internal pull-up; the output is switched on only to pull low.
    DL_GPIO_clearPins(port_regs(pin), pin_mask(pin));
    DL_GPIO_disableOutput(port_regs(pin), pin_mask(pin));
    DL_GPIO_initDigitalInputFeatures(pin->iomux, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

void mp_hal_pin_od_low(mp_hal_pin_obj_t pin) {
    DL_GPIO_clearPins(port_regs(pin), pin_mask(pin));
    DL_GPIO_enableOutput(port_regs(pin), pin_mask(pin));
}

void mp_hal_pin_od_high(mp_hal_pin_obj_t pin) {
    DL_GPIO_disableOutput(port_regs(pin), pin_mask(pin));
}

int mp_hal_pin_read(mp_hal_pin_obj_t pin) {
    // An output pin has its input buffer off, so DIN reads 0: report the level being driven instead.
    if (port_regs(pin)->DOE31_0 & pin_mask(pin)) {
        return DL_GPIO_readPins(port_regs(pin), pin_mask(pin)) != 0 || (port_regs(pin)->DOUT31_0 & pin_mask(pin)) != 0;
    }
    return DL_GPIO_readPins(port_regs(pin), pin_mask(pin)) != 0;
}

void mp_hal_pin_write(mp_hal_pin_obj_t pin, int v) {
    if (v) {
        DL_GPIO_setPins(port_regs(pin), pin_mask(pin));
    } else {
        DL_GPIO_clearPins(port_regs(pin), pin_mask(pin));
    }
}
