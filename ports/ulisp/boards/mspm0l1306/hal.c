// Board layer for uLisp on the LP-MSPM0L1306: 32 MHz oscillator (reset default), UART0 on PA8 (TX) and PA9 (RX) at 115200
// (the back-channel with J16 and J17 on XDS), LED1 on PA0 (on when the pin is low), millisecond clock from SysTick.
// Same hal_* functions as ports/ulisp/boards/hal.c; no receive ring buffer (4 kB RAM): the interrupt keeps the last byte only.
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include "frl_hal.h"

static const uint32_t cpu_hz = 32000000;
uint32_t hal_cpu_hz(void) { return cpu_hz; }
static volatile uint32_t tick_ms;
void SysTick_Handler(void) { tick_ms++; }
uint32_t hal_ms(void) { return tick_ms; }
uint32_t hal_us(void) {
    uint32_t ms, val;
    do { ms = tick_ms; val = SysTick->VAL; } while (ms != tick_ms);
    return ms * 1000u + (SysTick->LOAD - val) / (cpu_hz / 1000000u);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

#define RXN 64
static volatile uint8_t rxbuf[RXN];
static volatile uint8_t rxh, rxt;
void UART0_IRQHandler(void) {
    while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
        uint8_t c = DL_UART_Main_receiveData(UART0);
        uint8_t n = (rxh + 1) % RXN;
        if (n != rxt) { rxbuf[rxh] = c; rxh = n; }
    }
    DL_UART_Main_clearInterruptStatus(UART0, DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
}
int hal_getc(void) {
    if (rxt == rxh) return -1;
    uint8_t c = rxbuf[rxt]; rxt = (rxt + 1) % RXN; return c;
}
static void uart_init(void) {
    DL_UART_Main_reset(UART0);
    DL_UART_Main_enablePower(UART0);
    delay_cycles(32);
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM9, IOMUX_PINCM9_PF_UART0_TX);
    DL_GPIO_initPeripheralInputFunction(IOMUX_PINCM10, IOMUX_PINCM10_PF_UART0_RX);
    static const DL_UART_Main_ClockConfig ck = { .clockSel = DL_UART_MAIN_CLOCK_BUSCLK, .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1 };
    static const DL_UART_Main_Config cfg = {
        .mode = DL_UART_MAIN_MODE_NORMAL, .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE, .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS, .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_setClockConfig(UART0, (DL_UART_Main_ClockConfig *)&ck);
    DL_UART_Main_init(UART0, (DL_UART_Main_Config *)&cfg);
    DL_UART_Main_configBaudRate(UART0, 32000000, 115200);
    DL_UART_Main_enableFIFOs(UART0);
    DL_UART_Main_setRXFIFOThreshold(UART0, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_enableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Main_enable(UART0);
    NVIC_ClearPendingIRQ(UART0_INT_IRQn);
    NVIC_EnableIRQ(UART0_INT_IRQn);
}
void hal_putc(int c) {
    while (DL_UART_Main_isTXFIFOFull(UART0)) {}
    DL_UART_Main_transmitData(UART0, (uint8_t)c);
}

// One pin: index 0 = LED1 on PA0.
int hal_pin_count(void) { return 1; }
void hal_pin_mode(int i, int mode) {
    (void)i;
    if (mode == 1) { DL_GPIO_initDigitalOutput(IOMUX_PINCM1); DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_0); }
    else { DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_0); DL_GPIO_initDigitalInput(IOMUX_PINCM1); }
}
void hal_pin_write(int i, int v) { (void)i; if (v) DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_0); else DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_0); }
int hal_pin_read(int i) { (void)i; return (GPIOA->DOUT31_0 & DL_GPIO_PIN_0) != 0; }

void hal_init(void) {
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);
    delay_cycles(32);
    SysTick->LOAD = cpu_hz / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    uart_init();
}
