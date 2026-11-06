/*
 * Wire check for the CC1407P on the G5187: which J20 pin really is the radio's RESET_N and which its SPI_INT.
 * For each candidate pin (PB10, PB11): hold it low 20 ms as a reset, release, wait 50 ms, then do one raw
 * 10-byte exchange and print what came back. A real reset gives a fresh radio (no stale error message).
 * Also prints the idle level of both pins with a pull-down on the Mac side.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
static void putc_(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void th(uint8_t v) { putc_("0123456789abcdef"[v >> 4]); putc_("0123456789abcdef"[v & 15]); }
static void ms(uint32_t m) { delay_cycles(m * (CPUCLK_FREQ / 1000)); }
static int lvl(uint32_t pin) { return (DL_GPIO_readPins(GPIOB, pin) & pin) ? 1 : 0; }
static void as_input(uint32_t iomux, uint32_t pin, DL_GPIO_RESISTOR r) {
    DL_GPIO_disableOutput(GPIOB, pin);
    DL_GPIO_initDigitalInputFeatures(iomux, DL_GPIO_INVERSION_DISABLE, r, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}
static void as_low(uint32_t iomux, uint32_t pin) { DL_GPIO_initDigitalOutput(iomux); DL_GPIO_clearPins(GPIOB, pin); DL_GPIO_enableOutput(GPIOB, pin); }
static void exchange(void) {
    uint8_t rx[10];
    DL_GPIO_clearPins(RADIO_PORT, RADIO_CS_PIN);
    uint32_t t = 0; while (lvl(DL_GPIO_PIN_7) && t < 10000) { delay_cycles(CPUCLK_FREQ / 1000000); t++; }
    puts_(t >= 10000 ? " poci=stays-high" : " poci=low");
    for (int i = 0; i < 10; i++) {
        while (DL_SPI_isTXFIFOFull(SPI_0_INST)) { }
        DL_SPI_transmitData8(SPI_0_INST, 0);
        while (DL_SPI_isRXFIFOEmpty(SPI_0_INST)) { }
        rx[i] = DL_SPI_receiveData8(SPI_0_INST);
    }
    while (DL_SPI_isBusy(SPI_0_INST)) { }
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN);
    puts_(" rx="); for (int i = 0; i < 10; i++) th(rx[i]);
}
static void idle_levels(const char *when) {
    as_input(RADIO_RST_IOMUX, RADIO_RST_PIN, DL_GPIO_RESISTOR_PULL_DOWN);
    as_input(RADIO_INT_IRQ_IOMUX, RADIO_INT_IRQ_PIN, DL_GPIO_RESISTOR_PULL_DOWN);
    ms(5);
    puts_(when); puts_(" idle with pull-down: PB10="); putc_('0' + lvl(RADIO_RST_PIN)); puts_(" PB11="); putc_('0' + lvl(RADIO_INT_IRQ_PIN));
    as_input(RADIO_RST_IOMUX, RADIO_RST_PIN, DL_GPIO_RESISTOR_PULL_UP);
    as_input(RADIO_INT_IRQ_IOMUX, RADIO_INT_IRQ_PIN, DL_GPIO_RESISTOR_PULL_UP);
    ms(5);
    puts_(" | pull-up: PB10="); putc_('0' + lvl(RADIO_RST_PIN)); puts_(" PB11="); putc_('0' + lvl(RADIO_INT_IRQ_PIN)); puts_("\r\n");
}
#define PB12_IOMUX (IOMUX_PINCM29)
#define PB13_IOMUX (IOMUX_PINCM30)
static void hold_test(const char *name, uint32_t iomux, uint32_t pin) {
    puts_("WHILE "); puts_(name); puts_(" held low: "); as_low(iomux, pin); ms(200); exchange(); as_input(iomux, pin, DL_GPIO_RESISTOR_PULL_UP); puts_("\r\n");
    ms(100);
}
int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_disableInterrupt(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN);
    ms(200);
    for (int round = 1; round <= 3; round++) {
        puts_("\r\n=== diag round "); putc_('0' + round); puts_(" ===\r\n");
        idle_levels("start");
        puts_("no reset:      "); exchange(); puts_("\r\n");
        puts_("WHILE PB10 held low: "); as_low(RADIO_RST_IOMUX, RADIO_RST_PIN); ms(200); exchange(); as_input(RADIO_RST_IOMUX, RADIO_RST_PIN, DL_GPIO_RESISTOR_PULL_UP); puts_("\r\n");
        ms(100); puts_("after release:       "); exchange(); puts_("\r\n");
        idle_levels("after PB10");
        puts_("WHILE PB11 held low: "); as_low(RADIO_INT_IRQ_IOMUX, RADIO_INT_IRQ_PIN); ms(200); exchange(); as_input(RADIO_INT_IRQ_IOMUX, RADIO_INT_IRQ_PIN, DL_GPIO_RESISTOR_PULL_UP); puts_("\r\n");
        ms(100); puts_("after release:       "); exchange(); puts_("\r\n");
        idle_levels("after PB11");
        DL_GPIO_enablePower(GPIOB);
        hold_test("PB12", PB12_IOMUX, DL_GPIO_PIN_12);
        hold_test("PB13", PB13_IOMUX, DL_GPIO_PIN_13);
        ms(1500);
    }
    for (;;) { }
}
