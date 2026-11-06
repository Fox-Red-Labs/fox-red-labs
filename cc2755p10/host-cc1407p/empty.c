/* Bare CC1407P host on the LP-EM-CC2755P10: run the transmit sequence once and report the HAL counters on the backchannel UART at 115200. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "hal_bare.h"
extern void hb_p10_init(void); extern int hb_p10_spi_ok(void); extern void frl_cc1407p_tx_once(void);
extern volatile uint32_t frl_stage;
extern volatile struct { uint32_t spi_start, spi_done, cs_on, cs_off, rst_on, int_en, int_isr, tmr_start, tmr_isr, poci_rd, poci_hi, delay_calls, spi_fail; } hbc;
void *mainThread(void *arg0)
{
    char b[200]; UART2_Params up; UART2_Params_init(&up); up.baudRate = 115200;
    UART2_Handle u = UART2_open(CONFIG_UART2_0, &up);
    hb_p10_init();
    snprintf(b, sizeof b, "X45 start spi_open=%d\r\n", hb_p10_spi_ok()); UART2_write(u, b, strlen(b), NULL);
    frl_cc1407p_tx_once();
    snprintf(b, sizeof b, "X45 tx_once returned\r\n"); UART2_write(u, b, strlen(b), NULL);
    for (int t = 1; ; t++) {
        sleep(1);
        snprintf(b, sizeof b, "X45 t=%d rst=%u cs=%u/%u spi=%u/%u fail=%u poci=%u hi=%u int_en=%u int=%u tmr=%u/%u dly=%u\r\n", t,
            (unsigned)hbc.rst_on, (unsigned)hbc.cs_on, (unsigned)hbc.cs_off, (unsigned)hbc.spi_start, (unsigned)hbc.spi_done, (unsigned)hbc.spi_fail,
            (unsigned)hbc.poci_rd, (unsigned)hbc.poci_hi, (unsigned)hbc.int_en, (unsigned)hbc.int_isr, (unsigned)hbc.tmr_start, (unsigned)hbc.tmr_isr, (unsigned)hbc.delay_calls);
        UART2_write(u, b, strlen(b), NULL);
        if (t >= 6) { for (;;) sleep(1); }
    }
}
