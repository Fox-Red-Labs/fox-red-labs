/*
 * The seven things the CC1407P host driver needs from an LP-MSPM0L1306, in DriverLib.
 * Pins from radio.syscfg: SPI0 (PA6 clock, PA5 out, PA4 in), CS PA3, RESET_N PA7, SPI_INT PA2.
 * SPI transfers are polled and the completion is handed to the driver from PendSV, so the
 * driver sees it from an interrupt context the way TI's Zephyr version does.
 * The one-shot timer is SysTick. The ready-poll it serves is 30 us.
 */
#include "ti_msp_dl_config.h"
#include "hal_bare.h"

#define US_CYCLES (CPUCLK_FREQ / 1000000u)
/* SPI accounting for measurements: bytes clocked, transfers, and microseconds spent in transfers
 * (needs frl_now_us() from the app; without it only bytes and transfers are counted) */
volatile uint32_t hb_spi_bytes, hb_spi_us, hb_spi_xfers;
extern uint32_t frl_now_us(void) __attribute__((weak));
/* trace: one character per event on the UART, so a hang shows where it stopped */
#ifdef HB_TRACE
static void tc(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void th(uint8_t v) { tc("0123456789abcdef"[v >> 4]); tc("0123456789abcdef"[v & 15]); }
#else
#define tc(c) ((void)0)
#define th(v) ((void)0)
#endif

void hb_spi_start(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint32_t t0 = frl_now_us ? frl_now_us() : 0;
    tc('S'); th((uint8_t)len); tc('>'); for (uint16_t i = 0; i < len && i < 16; i++) { th(tx ? tx[i] : 0); }
    for (uint16_t i = 0; i < len; i++) {
        while (DL_SPI_isTXFIFOFull(SPI_0_INST)) { }
        DL_SPI_transmitData8(SPI_0_INST, tx ? tx[i] : 0u);
        while (DL_SPI_isRXFIFOEmpty(SPI_0_INST)) { }
        uint8_t b = DL_SPI_receiveData8(SPI_0_INST);
        if (rx) { rx[i] = b; }
    }
    while (DL_SPI_isBusy(SPI_0_INST)) { }
    hb_spi_bytes += len; hb_spi_xfers++; if (frl_now_us) hb_spi_us += frl_now_us() - t0;
    tc('<'); for (uint16_t i = 0; i < len && i < 16; i++) { th(rx ? rx[i] : 0); } tc(' ');
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}
void PendSV_Handler(void) { hb_spi_done(); }

void hb_cs(bool asserted)
{
    tc(asserted ? 'c' : 'C');
    if (asserted) { DL_GPIO_clearPins(RADIO_PORT, RADIO_CS_PIN); } else { DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN); }
}
bool hb_poci_high(void) { static uint8_t last = 2; bool h = (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_4) & DL_GPIO_PIN_4) != 0u; if (h != last) { tc(h ? 'P' : 'p'); last = h; } return (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_4) & DL_GPIO_PIN_4) != 0u; }
void hb_reset(bool asserted)
{
    tc(asserted ? 'r' : 'R');
    if (asserted) { DL_GPIO_clearPins(RADIO_PORT, RADIO_RST_PIN); } else { DL_GPIO_setPins(RADIO_PORT, RADIO_RST_PIN); }
}
void hb_int_enable(bool on)
{
    if (on) {
        DL_GPIO_clearInterruptStatus(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
        DL_GPIO_enableInterrupt(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
        NVIC_EnableIRQ(RADIO_INT_INT_IRQN);
    } else {
        DL_GPIO_disableInterrupt(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
    }
}
void GROUP1_IRQHandler(void)
{
    if (DL_GPIO_getEnabledInterruptStatus(RADIO_INT_PORT, RADIO_INT_IRQ_PIN) & RADIO_INT_IRQ_PIN) {
        DL_GPIO_clearInterruptStatus(RADIO_INT_PORT, RADIO_INT_IRQ_PIN);
        tc('I');
        hb_int_isr();
    }
}
void hb_delay_us(uint32_t us) { delay_cycles(us * US_CYCLES); }
/* After the one-shot, SysTick keeps ticking every 10 ms as a watchdog on the interrupt line
 * (HB_INT_POLL, debug): a low level on SPI_INT that produced no edge is reported as 'L' and
 * handed to the driver; with no interrupt at all for 500 ms, '?' forces a read so the trace
 * shows whether the radio has an answer waiting. */
static volatile uint8_t tick_mode, tick_n;
void hb_timer_oneshot_us(uint32_t us)
{
    uint32_t ticks = us * US_CYCLES; if (ticks < 2u) { ticks = 2u; } if (ticks > 0xFFFFFFu) { ticks = 0xFFFFFFu; }
    SysTick->CTRL = 0; SysTick->LOAD = ticks - 1u; SysTick->VAL = 0; tick_mode = 0; tick_n = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
void hb_timer_stop(void) { SysTick->CTRL = 0; }
void SysTick_Handler(void)
{
    if (tick_mode == 0) {
        tc('t'); hb_timer_isr();
#ifdef HB_INT_POLL
        tick_mode = 1; SysTick->LOAD = 10000u * US_CYCLES - 1u; SysTick->VAL = 0;
#else
        SysTick->CTRL = 0;
#endif
        return;
    }
    bool low = (DL_GPIO_readPins(RADIO_INT_PORT, RADIO_INT_IRQ_PIN) & RADIO_INT_IRQ_PIN) == 0u;
    if (low) { SysTick->CTRL = 0; tc('L'); hb_int_isr(); return; }
    if (++tick_n >= 50u) { SysTick->CTRL = 0; tc('?'); hb_int_isr(); }
}
