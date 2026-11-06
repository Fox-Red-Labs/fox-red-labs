/* Port of the Sharp128 core to MSPM0 DriverLib (CPU M0+). Shared by every MSPM0 board in
 * boards/: the per-board part is the SysConfig file, which names the pins and the SPI
 * instance the same way on each board:
 *   GPIO groups GP_PWR, GP_DISP, GP_LCD_CS, GP_SD_CS (outputs) and GP_SD_DET (input), each
 *   with one pin named PIN_<same>, and one SPI instance named SHARP_SPI (clock, data out, data in).
 * SPI is mode 0, most significant bit first, 8-bit frames; chip selects are plain GPIO. */
#include "ti_msp_dl_config.h"
#include "sharp_lcd.h"
#ifdef SHARP_PORT_HAS_SD
#include "sharp_sd.h"
#endif

/* SysConfig names: group GP_<X> gives GP_<X>_PORT, its pin PIN_<X> gives GP_<X>_PIN_<X>_PIN. */
#define OUT_SET(x, l) do { if (l) DL_GPIO_setPins(GP_##x##_PORT, GP_##x##_PIN_##x##_PIN); else DL_GPIO_clearPins(GP_##x##_PORT, GP_##x##_PIN_##x##_PIN); } while (0)

/* Millisecond tick. SysTick on most parts; the MSPM0C1104's SysTick does nothing (mspm0c1104/FAQ.md), so
 * that part counts with TIMG14 (16 bits, CPU clock, one interrupt a millisecond). */
static volatile uint32_t ms_ticks;
#ifdef SHARP_TIMER_TIMG14
#define TICK_LOAD (CPUCLK_FREQ / 1000u - 1u)
#define TICK_VAL() (TIMG14->COUNTERREGS.CTR)
void TIMG14_IRQHandler(void)
{
    if (TIMG14->CPU_INT.IIDX == DL_TIMERG_IIDX_ZERO) ms_ticks++;
}
static void tick_start(void)
{
    static const DL_TimerG_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_1, .prescale = 0};
    static const DL_TimerG_TimerConfig tim = {
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .period = TICK_LOAD, .startTimer = DL_TIMER_STOP};
    DL_TimerG_reset(TIMG14);
    DL_TimerG_enablePower(TIMG14);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_TimerG_setClockConfig(TIMG14, (DL_TimerG_ClockConfig *) &clk);
    DL_TimerG_initTimerMode(TIMG14, (DL_TimerG_TimerConfig *) &tim);
    DL_TimerG_enableInterrupt(TIMG14, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_SetPriority(TIMG14_INT_IRQn, 3);
    NVIC_EnableIRQ(TIMG14_INT_IRQn);
    DL_TimerG_startCounter(TIMG14);
}
#else
#define TICK_LOAD (SysTick->LOAD)
#define TICK_VAL() (SysTick->VAL)
void SysTick_Handler(void) { ms_ticks++; }
static void tick_start(void)
{
    SysTick->LOAD = CPUCLK_FREQ / 1000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
#endif

static uint32_t spi_clk_hz;     /* the SPI module's input clock, found from the 1 MHz setup */

void sharp_port_init(void)
{
    SYSCFG_DL_init();
    /* SysConfig set the divider for 1 MHz: work back to the input clock. */
    spi_clk_hz = 2u * (DL_SPI_getBitRateSerialClockDivider(SHARP_SPI_INST) + 1u) * 1000000u;
    tick_start();
}

uint32_t sharp_port_millis(void) { return ms_ticks; }

/* Microseconds from the millisecond count and the tick timer's down-counter. */
uint32_t sharp_port_micros(void)
{
    uint32_t m1, m2, v;
    do { m1 = ms_ticks; v = TICK_VAL(); m2 = ms_ticks; } while (m1 != m2);
    return m1 * 1000u + (TICK_LOAD - v) / (CPUCLK_FREQ / 1000000u);
}

void sharp_port_delay_us(uint32_t us)
{
    uint32_t t0 = sharp_port_micros();
    while ((uint32_t)(sharp_port_micros() - t0) < us) { }
}

void sharp_port_lcd_cs(int l)    { OUT_SET(LCD_CS, l); }
void sharp_port_lcd_power(int l) { OUT_SET(PWR, l); }
void sharp_port_lcd_disp(int l)  { OUT_SET(DISP, l); }

uint32_t sharp_port_spi_hz(void)
{
    return spi_clk_hz / (2u * (DL_SPI_getBitRateSerialClockDivider(SHARP_SPI_INST) + 1u));
}

void sharp_port_spi_set_hz(uint32_t hz)
{
    uint32_t div = spi_clk_hz / (2u * hz);
    if (div == 0) div = 1;
    DL_SPI_disable(SHARP_SPI_INST);
    DL_SPI_setBitRateSerialClockDivider(SHARP_SPI_INST, div - 1u);
    DL_SPI_enable(SHARP_SPI_INST);
}

/* Up to four bytes in flight keeps the clock running between bytes. Every byte sent is
 * also received, so when the last byte is back the shifter is idle. */
void sharp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t n)
{
    uint32_t sent = 0, got = 0;
    while (got < n) {
        while (sent < n && (sent - got) < 4u && !DL_SPI_isTXFIFOFull(SHARP_SPI_INST)) {
            DL_SPI_transmitData8(SHARP_SPI_INST, tx ? tx[sent] : 0xFF);
            sent++;
        }
        while (got < sent && !DL_SPI_isRXFIFOEmpty(SHARP_SPI_INST)) {
            uint8_t b = DL_SPI_receiveData8(SHARP_SPI_INST);
            if (rx) rx[got] = b;
            got++;
        }
    }
}

#ifdef SHARP_PORT_HAS_SD
void sharp_port_sd_cs(int l) { OUT_SET(SD_CS, l); }
#ifdef SHARP_NO_SD_DET
int sharp_port_sd_present(void) { return 1; }      /* no detect wire: try the card */
#else
int sharp_port_sd_present(void) { return DL_GPIO_readPins(GP_SD_DET_PORT, GP_SD_DET_PIN_SD_DET_PIN) ? 0 : 1; }
#endif
#endif

/* Stack high-water mark: paint the unused stack at start, count the untouched words later.
 * __stack and __STACK_END come from the linker (TI Arm Clang). */
extern uint32_t __stack, __STACK_END;
void sharp_port_stack_paint(void)
{
    uint32_t *p = &__stack, *sp = (uint32_t *)__builtin_frame_address(0);
    while (p < sp - 8) *p++ = 0xA5A5A5A5u;
}
uint32_t sharp_port_stack_free(void)
{
    uint32_t *p = &__stack;
    while (p < &__STACK_END && *p == 0xA5A5A5A5u) p++;
    return (uint32_t)((uint8_t *)p - (uint8_t *)&__stack);   /* bytes never used */
}
