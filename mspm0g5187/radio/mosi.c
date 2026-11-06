/* Is the G5187's PB8 (PICO) reaching radio pin 15? Reset the radio, then send one 10-byte transaction
 * with PB8 forced high as a GPIO (the radio sees 0xFF bytes), read the radio's reply, then the same with
 * PB8 forced low (0x00 bytes). A connected line gives two different replies; a loose one gives the same. */
#include "ti_msp_dl_config.h"
#include <stdint.h>
static void putc_(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void th(uint8_t v) { putc_("0123456789abcdef"[v >> 4]); putc_("0123456789abcdef"[v & 15]); }
static void ms(uint32_t m) { delay_cycles(m * (CPUCLK_FREQ / 1000)); }
static void xfer(int n) {
    DL_GPIO_clearPins(RADIO_PORT, RADIO_CS_PIN);
    uint32_t t = 0; while ((DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_7) & DL_GPIO_PIN_7) && t < 10000) { delay_cycles(80); t++; }
    puts_(" rx=");
    for (int i = 0; i < n; i++) {
        while (DL_SPI_isTXFIFOFull(SPI_0_INST)) { }
        DL_SPI_transmitData8(SPI_0_INST, 0);
        while (DL_SPI_isRXFIFOEmpty(SPI_0_INST)) { }
        th(DL_SPI_receiveData8(SPI_0_INST));
    }
    while (DL_SPI_isBusy(SPI_0_INST)) { }
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN);
}
static void reset_radio(void) { DL_GPIO_clearPins(RADIO_PORT, RADIO_RST_PIN); ms(20); DL_GPIO_setPins(RADIO_PORT, RADIO_RST_PIN); ms(100); }
static void pico_gpio(int high) {
    DL_GPIO_initDigitalOutput(GPIO_SPI_0_IOMUX_PICO);
    if (high) DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_8); else DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_8);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_8);
}
int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_disableInterrupt(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN);
    for (int round = 1; round <= 3; round++) {
        puts_("\r\n=== mosi round "); putc_('0' + round); puts_(" ===\r\n");
        for (int lvl = 1; lvl >= 0; lvl--) {
            reset_radio();
            puts_(lvl ? "PB8 high: first" : "PB8 low:  first"); pico_gpio(lvl); xfer(10);
            ms(20); puts_(" | INT="); putc_((DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_11) & DL_GPIO_PIN_11) ? '1' : '0');
            puts_(" | next"); xfer(14); puts_("\r\n");
        }
        ms(1000);
    }
    for (;;) { }
}
