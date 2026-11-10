/*
 * Board layer for uLisp on the LAUNCHXL-F2800137 (TMS320F2800137, C28x 120 MHz). Implements ports/ulisp/shim/frl_hal.h over C2000Ware driverlib:
 * SCI-A on GPIO28 (RX) and GPIO29 (TX), which the onboard XDS110 carries to the Mac as a serial port, at 115200;
 * CPU Timer 0 at 1 kHz for milliseconds and its down-counter for microseconds; LED1 on GPIO20 and LED2 on GPIO22 (Arduino pins 13 and 14).
 */
#include <string.h>
#include "driverlib.h"
#include "device.h"
#include "frl_hal.h"

static volatile uint32_t tick_ms;
static uint16_t led_out[2];
static const uint16_t led_gpio[2] = { 20, 22 };

__interrupt void timer0_isr(void) { tick_ms++; Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1); }
uint32_t hal_cpu_hz(void) { return DEVICE_SYSCLK_FREQ; }
uint32_t hal_ms(void) { return tick_ms; }
uint32_t hal_us(void)
{
    uint32_t ms, cnt;
    do { ms = tick_ms; cnt = CPUTimer_getTimerCount(CPUTIMER0_BASE); } while (ms != tick_ms);
    return ms * 1000UL + (DEVICE_SYSCLK_FREQ / 1000UL - cnt) / (DEVICE_SYSCLK_FREQ / 1000000UL);
}
void hal_delay_ms(uint32_t ms) { uint32_t s = tick_ms; while (tick_ms - s < ms) {} }

void hal_putc(int c)
{
    while (!(HWREGH(SCIA_BASE + SCI_O_CTL2) & SCI_CTL2_TXRDY)) {}
    SCI_writeCharNonBlocking(SCIA_BASE, (uint16_t)c);
}
int hal_getc(void)
{
    if (SCI_getRxStatus(SCIA_BASE) & SCI_RXSTATUS_ERROR) SCI_performSoftwareReset(SCIA_BASE);
    return SCI_isDataAvailableNonFIFO(SCIA_BASE) ? (int)(SCI_readCharNonBlocking(SCIA_BASE) & 0xFF) : -1;
}

int hal_pin_count(void) { return 2; }
void hal_pin_mode(int i, int mode)
{
    if (i < 0 || i > 1) return;
    led_out[i] = (mode == 1);
    GPIO_setDirectionMode(led_gpio[i], mode == 1 ? GPIO_DIR_MODE_OUT : GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(led_gpio[i], mode == 2 ? GPIO_PIN_TYPE_PULLUP : GPIO_PIN_TYPE_STD);
}
void hal_pin_write(int i, int v) { if (i >= 0 && i <= 1 && led_out[i]) { if (v) GPIO_writePin(led_gpio[i], 1); else GPIO_writePin(led_gpio[i], 0); } }
int hal_pin_read(int i) { return (i >= 0 && i <= 1) ? (int)GPIO_readPin(led_gpio[i]) : 0; }

extern uint16_t RamfuncsLoadStart, RamfuncsLoadSize, RamfuncsRunStart;

void hal_init(void)
{
    /* flash build: copy the .TI.ramfunc code (Flash_initModule and friends) to RAM before Device_init calls it */
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);
    Device_init();
    Device_initGPIO();
    GPIO_setPinConfig(GPIO_28_SCIA_RX);
    GPIO_setPinConfig(GPIO_29_SCIA_TX);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, 115200, (SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE | SCI_CONFIG_PAR_NONE));
    SCI_disableFIFO(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);
    SCI_resetChannels(SCIA_BASE);
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Interrupt_register(INT_TIMER0, &timer0_isr);
    CPUTimer_setPeriod(CPUTIMER0_BASE, DEVICE_SYSCLK_FREQ / 1000UL);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_setEmulationMode(CPUTIMER0_BASE, CPUTIMER_EMULATIONMODE_RUNFREE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_enable(INT_TIMER0);
    CPUTimer_startTimer(CPUTIMER0_BASE);
    EINT;
    ERTM;
}

int strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = *(const unsigned char *)a, y = *(const unsigned char *)b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y || x == 0) return x - y;
    }
}
