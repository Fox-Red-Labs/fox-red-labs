/* The six things a bare host must provide for the CC1407P driver. Vendor glue: a few DriverLib calls each. */
#ifndef HAL_BARE_H
#define HAL_BARE_H
#include <stdint.h>
#include <stdbool.h>
/* Start a full-duplex SPI transfer of len bytes (tx may be NULL: send zeros), call hb_spi_done() from the end-of-transfer interrupt. */
void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len);
void hb_cs(bool asserted);          /* chip select is a GPIO driven by the driver */
bool hb_poci_high(void);            /* MISO read as a GPIO: low means the TRX is ready */
void hb_reset(bool asserted);       /* RESET_N line */
void hb_int_enable(bool on);        /* SPI_INT falling edge calls hb_int_isr() */
void hb_delay_us(uint32_t us);
void hb_timer_oneshot_us(uint32_t us);  /* calls hb_timer_isr() once */
void hb_timer_stop(void);
/* Called by the glue; implemented in hal_bare.c */
void hb_spi_done(void);
void hb_int_isr(void);
void hb_timer_isr(void);
#endif
