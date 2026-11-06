/* hb_* hooks for the LP-EM-CC2755P10 (TI Drivers, no RTOS). Pins per ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay:
   SCLK DIO3, POCI DIO4, PICO DIO5, CS DIO7 (GPIO), RESET DIO16, SPI_INT DIO28. Counters let the run be checked with no radio fitted. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_gpio.h)
#include DeviceFamily_constructPath(inc/hw_types.h)
#include "ti_drivers_config.h"
#include "hal_bare.h"

volatile struct { uint32_t spi_start, spi_done, cs_on, cs_off, rst_on, int_en, int_isr, tmr_start, tmr_isr, poci_rd, poci_hi, delay_calls, spi_fail; } hbc;
static SPI_Handle spi;
static ClockP_Struct clk; static ClockP_Handle clkh;
static uint8_t zeros[160];

static void spiCb(SPI_Handle h, SPI_Transaction *t) { (void)h; if (t->status != SPI_TRANSFER_COMPLETED) { hbc.spi_fail++; } hbc.spi_done++; hb_spi_done(); }
static void intCb(uint_least8_t idx) { (void)idx; hbc.int_isr++; hb_int_isr(); }
static void clkCb(uintptr_t a) { (void)a; hbc.tmr_isr++; hb_timer_isr(); }

void hb_p10_init(void)
{
    GPIO_init(); SPI_init();
    GPIO_setConfig(CONFIG_RADIO_CS, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH);
    GPIO_setConfig(CONFIG_RADIO_RESET, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH);
    GPIO_setConfig(CONFIG_RADIO_INT, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setCallback(CONFIG_RADIO_INT, intCb);
    SPI_Params p; SPI_Params_init(&p);
    p.transferMode = SPI_MODE_CALLBACK; p.transferCallbackFxn = spiCb; p.mode = SPI_CONTROLLER; p.bitRate = 12000000u; p.dataSize = 8;
    spi = SPI_open(CONFIG_SPI_0, &p);
    ClockP_Params cp; ClockP_Params_init(&cp); cp.period = 0; cp.startFlag = false; cp.arg = 0;
    clkh = ClockP_construct(&clk, clkCb, 1, &cp);
}
int hb_p10_spi_ok(void) { return spi != NULL; }

void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    static SPI_Transaction tr;
    hbc.spi_start++;
    tr.count = len; tr.txBuf = (void *)(tx ? tx : zeros); tr.rxBuf = rx;
    if (!SPI_transfer(spi, &tr)) { hbc.spi_fail++; hb_spi_done(); }
}
void hb_cs(bool a) { if (a) hbc.cs_on++; else hbc.cs_off++; GPIO_write(CONFIG_RADIO_CS, a ? 0 : 1); }
bool hb_poci_high(void) { hbc.poci_rd++; bool h = (HWREG(GPIO_BASE + GPIO_O_DIN31_0) >> 4) & 1u; if (h) hbc.poci_hi++; return h; }
void hb_reset(bool a) { if (a) hbc.rst_on++; GPIO_write(CONFIG_RADIO_RESET, a ? 0 : 1); }
void hb_int_enable(bool on) { if (on) { hbc.int_en++; GPIO_clearInt(CONFIG_RADIO_INT); GPIO_enableInt(CONFIG_RADIO_INT); } else { GPIO_disableInt(CONFIG_RADIO_INT); } }
void hb_delay_us(uint32_t us)
{
    hbc.delay_calls++;
    /* DWT cycle counter, CPU at 96 MHz */
    *(volatile uint32_t *)0xE000EDFC |= (1u << 24); *(volatile uint32_t *)0xE0001000 |= 1u;
    uint32_t s = *(volatile uint32_t *)0xE0001004, n = us * 96u;
    while ((*(volatile uint32_t *)0xE0001004 - s) < n) { }
}
void hb_timer_oneshot_us(uint32_t us)
{
    uint32_t tick = ClockP_getSystemTickPeriod(); uint32_t n = (us + tick - 1) / tick; if (n < 2) n = 2;
    hbc.tmr_start++; ClockP_stop(clkh); ClockP_setTimeout(clkh, n); ClockP_start(clkh);
}
void hb_timer_stop(void) { ClockP_stop(clkh); }
