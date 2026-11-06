/* Stand-ins for the vendor glue so the image links for size measurement. On a real board each is a few DriverLib calls. */
#include "hal_bare.h"
void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len) { (void)tx; (void)rx; (void)len; hb_spi_done(); }
void hb_cs(bool a) { (void)a; }
bool hb_poci_high(void) { return false; }
void hb_reset(bool a) { (void)a; }
void hb_int_enable(bool on) { (void)on; }
void hb_delay_us(uint32_t us) { (void)us; }
void hb_timer_oneshot_us(uint32_t us) { (void)us; hb_timer_isr(); }
void hb_timer_stop(void) { }
