/*
 * The seven things the CC1407P host driver needs from an IWRL6432BOOST, on the MMWAVE-L-SDK drivers.
 * Wires (iwrl6432boost/cc1407p/README.md): McSPI A on J2 (CLK pad D10, MOSI B12, MISO C11, CS0 D11),
 * SPI_INT on pad J10 (GPIO5, the board's HOST_INTR line), RESET_N on pad K11 (GPIO4, the nERROR line).
 *
 * Everything runs from the main loop: hb_poll() is called by the app's wait loops and does what an
 * interrupt would (transfer-done, the 30 us ready timer, the SPI_INT edge). That keeps the driver
 * single-threaded and needs no RTOS. SPI transfers are polled and synchronous; the done callback is
 * delivered from hb_poll() after hb_spi_start() has returned, as the driver expects.
 *
 * Chip select is the McSPI's own CS0 (pad D11 has no GPIO function), held asserted with the channel's
 * FORCE bit between transfers. MISO has to be read as a level while CS is low (the radio pulls it low
 * when it is ready), so hb_poci_high() flips pad C11 to GPIO1 and hb_spi_start() flips it back.
 *
 * McSPI smart idle (TI errata): the SDK's MCSPI_open writes SYSCONFIG with SIDLEMODE = no idle and
 * AUTOIDLE = off; hb_spi_sysconfig() reads it back so the app can print it.
 */
#include <stdint.h>
#include <stdbool.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <drivers/mcspi.h>
#include <drivers/gpio.h>
#include <drivers/pinmux.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/cslr_soc.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "hal_bare.h"

#define PIN_RESET   GPIO_RADIO_RESET_PIN
#define PIN_INT     GPIO_RADIO_INT_PIN
#define PIN_POCI    1u                      /* pad C11 in mode 1 is GPIO1 */
#define SPI_BASE    CSL_MCU_MCSPI0_CFG_BASE
#define INT_RETRY_US 10000u                 /* a low SPI_INT that made no edge is handed over every 10 ms */

volatile uint32_t hb_spi_bytes, hb_spi_xfers, hb_spi_us, hb_int_calls;

static uint32_t gpio;                       /* GPIO block base */
static volatile bool spi_done_pending, int_on, tmr_on, poci_as_gpio;
static volatile uint64_t tmr_deadline;
static uint8_t int_prev = 1; static uint64_t int_low_since;
static Pinmux_PerCfg_t poci_gpio_cfg[] = { { PIN_PAD_AJ, (PIN_MODE(1) | PIN_PULL_DISABLE) }, { PINMUX_END, PINMUX_END } };
static Pinmux_PerCfg_t poci_spi_cfg[]  = { { PIN_PAD_AJ, (PIN_MODE(0) | PIN_PULL_DISABLE) }, { PINMUX_END, PINMUX_END } };

void hb_hw_init(void)
{
    gpio = (uint32_t)AddrTranslateP_getLocalAddr(GPIO_RADIO_RESET_BASE_ADDR);
    GPIO_setDirMode(gpio, PIN_RESET, GPIO_DIRECTION_OUTPUT);
    GPIO_pinWriteHigh(gpio, PIN_RESET);     /* RESET_N released */
    GPIO_setDirMode(gpio, PIN_INT, GPIO_DIRECTION_INPUT);
}
uint32_t hb_spi_sysconfig(void) { return HW_RD_REG32(SPI_BASE + CSL_APP_SPI_SYSCONFIG); }
uint32_t hb_now_us(void) { return (uint32_t)ClockP_getTimeUsec(); }

static void poci_to_spi(void)
{
    if (poci_as_gpio) { Pinmux_config(poci_spi_cfg, PINMUX_DOMAIN_ID_MAIN); poci_as_gpio = false; }
}

void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    MCSPI_Transaction t;
    uint32_t t0 = hb_now_us();
    poci_to_spi();
    MCSPI_Transaction_init(&t);
    t.channel = gConfigMcspi0ChCfg[0].chNum;
    t.dataSize = 8; t.csDisable = FALSE;    /* CS is held by hb_cs(), never released between transfers */
    t.count = len; t.txBuf = (void *)tx; t.rxBuf = rx; t.args = NULL;
    (void)MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &t);
    hb_spi_bytes += len; hb_spi_xfers++; hb_spi_us += hb_now_us() - t0;
    spi_done_pending = true;
}

void hb_cs(bool asserted)
{
    if (asserted) {
        CSL_REG32_FINS(SPI_BASE + APP_SPI_CHCTRL(0), APP_SPI_CH0CTRL_EN, CSL_APP_SPI_CH0CTRL_EN_ACT);
        CSL_REG32_FINS(SPI_BASE + APP_SPI_CHCONF(0), APP_SPI_CH0CONF_FORCE, CSL_APP_SPI_CH0CONF_FORCE_ASSERT);
    } else {
        poci_to_spi();
        CSL_REG32_FINS(SPI_BASE + APP_SPI_CHCONF(0), APP_SPI_CH0CONF_FORCE, CSL_APP_SPI_CH0CONF_FORCE_DEASSERT);
        CSL_REG32_FINS(SPI_BASE + APP_SPI_CHCTRL(0), APP_SPI_CH0CTRL_EN, CSL_APP_SPI_CH0CTRL_EN_NACT);
    }
}
bool hb_poci_high(void)
{
    if (!poci_as_gpio) { Pinmux_config(poci_gpio_cfg, PINMUX_DOMAIN_ID_MAIN); GPIO_setDirMode(gpio, PIN_POCI, GPIO_DIRECTION_INPUT); poci_as_gpio = true; }
    return GPIO_pinRead(gpio, PIN_POCI) != 0u;
}
void hb_reset(bool asserted)
{
    if (asserted) { GPIO_pinWriteLow(gpio, PIN_RESET); } else { GPIO_pinWriteHigh(gpio, PIN_RESET); }
}
void hb_int_enable(bool on)
{
    int_on = on; int_prev = (GPIO_pinRead(gpio, PIN_INT) != 0u); int_low_since = ClockP_getTimeUsec();
}
void hb_delay_us(uint32_t us)
{
    uint64_t end = ClockP_getTimeUsec() + us;
    while (ClockP_getTimeUsec() < end) { }
}
void hb_timer_oneshot_us(uint32_t us) { tmr_deadline = ClockP_getTimeUsec() + us; tmr_on = true; }
void hb_timer_stop(void) { tmr_on = false; }

/* Does what the interrupts would. Call it in every wait loop. */
void hb_poll(void)
{
    if (spi_done_pending) { spi_done_pending = false; hb_spi_done(); }
    if (tmr_on && ClockP_getTimeUsec() >= tmr_deadline) { tmr_on = false; hb_timer_isr(); }
    if (int_on) {
        uint8_t lvl = (GPIO_pinRead(gpio, PIN_INT) != 0u);
        uint64_t now = ClockP_getTimeUsec();
        if (lvl) { int_low_since = now; }
        else if (int_prev || (now - int_low_since) >= INT_RETRY_US) { int_low_since = now; hb_int_calls++; hb_int_isr(); }
        int_prev = lvl;
    }
}
