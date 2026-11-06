/*
 * Fake-radio glue for the CC1407P host on any target: every SPI transfer completes at once, the radio
 * always answers "ready" and returns zeros, and every transmitted word is logged. Lets the driver run
 * with no radio so the bytes it WOULD put on the wire can be read back. Same file on the C28x and on a PC.
 */
#include "hal_bare.h"
#ifdef TRACE
#include <stdio.h>
#endif
extern void bench_done(void);
#define LOG_WORDS 256u
volatile uint16_t fake_log[LOG_WORDS];   /* low 8 bits of each transmitted unit, in order */
volatile uint16_t fake_log_n;            /* words logged (stops at LOG_WORDS) */
volatile uint16_t fake_xfers;            /* SPI transfers started */
volatile uint16_t fake_xfer_len[32];     /* length of the first 32 transfers, in driver units */
void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;
    if (fake_xfers < 32u) { fake_xfer_len[fake_xfers] = len; }
    fake_xfers++;
#ifdef TRACE
    if (fake_xfers < 12u) { fprintf(stderr, "xfer %u len %u tx %p\n", fake_xfers, len, (const void *)tx); }
#endif
    for (i = 0; i < len; i++) {
        if (tx && fake_log_n < LOG_WORDS) { fake_log[fake_log_n++] = (uint16_t)(tx[i] & 0xFFu); }
        if (rx) { rx[i] = 0; }
    }
    if (fake_xfers == 1u) { bench_done(); }   /* TRX_Host_open now waits for an answer a fake radio cannot give: stop here */
    hb_spi_done();
}
void hb_cs(bool a) { (void)a; }
bool hb_poci_high(void) { return false; }
void hb_reset(bool a) { (void)a; }
void hb_int_enable(bool on) { (void)on; }
void hb_delay_us(uint32_t us) { (void)us; }
void hb_timer_oneshot_us(uint32_t us) { (void)us; hb_timer_isr(); }
void hb_timer_stop(void) { }
