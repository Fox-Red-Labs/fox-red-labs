/*
 * Port of the Educational BoosterPack core to TI DriverLib: one file for the MSPM0 and
 * MSPM33 boards. Pins, ADC channels and the SPI/UART instances come from the generated
 * ti_msp_dl_config.h and board_config.h (tools/gen_board.py reads boards/<board>/pins.json).
 * BSD-3-Clause.
 */
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"

typedef struct { GPIO_Regs *port; uint32_t pin; uint32_t iomux; } pinref_t;
#define PINREF(n) { E_##n##_PORT, E_##n##_##n##_PIN, E_##n##_##n##_IOMUX }

static const pinref_t pins[EDUMKII_PIN_COUNT] = {
#ifdef EDU_HAS_LCD_CS
    [EDUMKII_PIN_LCD_CS] = PINREF(LCD_CS),
#endif
#ifdef EDU_HAS_LCD_DC
    [EDUMKII_PIN_LCD_DC] = PINREF(LCD_DC),
#endif
#ifdef EDU_HAS_LCD_RST
    [EDUMKII_PIN_LCD_RST] = PINREF(LCD_RST),
#endif
#ifdef EDU_HAS_BL_RED
    [EDUMKII_PIN_BL_RED] = PINREF(BL_RED),
#endif
#ifdef EDU_HAS_LED_GREEN
    [EDUMKII_PIN_LED_GREEN] = PINREF(LED_GREEN),
#endif
#ifdef EDU_HAS_LED_BLUE
    [EDUMKII_PIN_LED_BLUE] = PINREF(LED_BLUE),
#endif
#ifdef EDU_HAS_BUZZER
    [EDUMKII_PIN_BUZZER] = PINREF(BUZZER),
#endif
#ifdef EDU_HAS_S1
    [EDUMKII_PIN_S1] = PINREF(S1),
#endif
#ifdef EDU_HAS_S2
    [EDUMKII_PIN_S2] = PINREF(S2),
#endif
#ifdef EDU_HAS_JOY_SEL
    [EDUMKII_PIN_JOY_SEL] = PINREF(JOY_SEL),
#endif
};

static const pinref_t scl = PINREF(I2C_SCL), sda = PINREF(I2C_SDA);

static void p_delay_us(void *c, uint32_t us)
{
    (void)c;
    while (us > 1000u) { delay_cycles(1000u * (CPUCLK_FREQ / 1000000u)); us -= 1000u; }
    delay_cycles(us * (CPUCLK_FREQ / 1000000u));
}
static void p_delay_ms(void *c, uint32_t ms) { while (ms--) p_delay_us(c, 1000u); }

static void p_gpio_write(void *c, edumkii_pin_t p, int level)
{
    (void)c;
    if (!pins[p].port) return;
    if (level) DL_GPIO_setPins(pins[p].port, pins[p].pin);
    else DL_GPIO_clearPins(pins[p].port, pins[p].pin);
}
#ifdef EDU_HAS_JOY_SEL_ADC
static uint16_t adc12_read(ADC12_Regs *a, uint32_t chan);
#endif
static int p_gpio_read(void *c, edumkii_pin_t p)
{
    (void)c;
#ifdef EDU_HAS_JOY_SEL_ADC
    if (p == EDUMKII_PIN_JOY_SEL) {          /* this LaunchPad has no GPIO on the pin: read it as analog */
        static const struct { ADC12_Regs *inst; uint32_t chan; } js = EDU_JOYSEL_ADC;
        return adc12_read(js.inst, js.chan) < 2048u ? 0 : 1;
    }
#endif
#ifdef EDU_HAS_JOY_SEL_RAW
    if (p == EDUMKII_PIN_JOY_SEL) {          /* the pad is analog (ADC1_7) most of the time: make it digital for one read */
        static const struct { GPIO_Regs *port; uint32_t pin; uint32_t iomux; } js = EDU_JOYSEL_RAW;
        DL_GPIO_initDigitalInputFeatures(js.iomux, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
            DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
        p_delay_us(0, 500);                      /* the BoosterPack has 10 nF on the line, 10 kOhm pull-up: tau 0.1 ms */
        int v = DL_GPIO_readPins(js.port, js.pin) ? 1 : 0;
        DL_GPIO_initPeripheralAnalogFunction(js.iomux);
        return v;
    }
#endif
    if (!pins[p].port) return 1;                 /* a pin this LaunchPad lacks reads released */
    return DL_GPIO_readPins(pins[p].port, pins[p].pin) ? 1 : 0;
}

/* ---- SPI ---- */
#ifdef EDU_HAS_LCD
static void p_spi_write(void *c, const uint8_t *d, uint32_t n)
{
    (void)c;
    for (uint32_t i = 0; i < n; i++) DL_SPI_transmitDataBlocking8(SPI_LCD_INST, d[i]);
    while (DL_SPI_isBusy(SPI_LCD_INST)) { }
}
#endif

#ifdef EDU_SPI_DMA
static volatile uint32_t dma_done;
void SPI_LCD_INST_IRQHandler(void)
{
    if (DL_SPI_getPendingInterrupt(SPI_LCD_INST) == DL_SPI_IIDX_DMA_DONE_TX) dma_done = 1;
}
static volatile int use_dma = 1;
void edu_use_dma(int on) { use_dma = on; }
void edu_spi_rate(uint32_t scr)
{
    DL_SPI_disable(SPI_LCD_INST);
    DL_SPI_setBitRateSerialClockDivider(SPI_LCD_INST, scr);
    DL_SPI_enable(SPI_LCD_INST);
}
static volatile uint32_t dma_pending;     /* a DMA send was started and its done flag not yet collected */
static void p_spi_wait(void *c)
{
    (void)c;
    if (dma_pending) { while (!dma_done) { } dma_pending = 0; }
    while (DL_SPI_isBusy(SPI_LCD_INST)) { }
}
static void p_spi_async(void *c, const uint8_t *d, uint32_t n)
{
    if (!use_dma) { p_spi_write(c, d, n); return; }
    p_spi_wait(c);
    DL_SPI_flushTXFIFO(SPI_LCD_INST);
    DL_DMA_setSrcAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)d);
    DL_DMA_setDestAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)(&SPI_LCD_INST->spi->TXDATA));
    DL_DMA_setTransferSize(DMA0, DMA0_CH1_CHAN_ID, n);
    dma_done = 0;
    dma_pending = 1;
    DL_DMA_enableChannel(DMA0, DMA0_CH1_CHAN_ID);
}
static void p_spi_bulk(void *c, const uint8_t *d, uint32_t n)
{
    if (!use_dma) { p_spi_write(c, d, n); return; }
    p_spi_wait(c);
    DL_SPI_flushTXFIFO(SPI_LCD_INST);
    DL_DMA_setSrcAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)d);
    DL_DMA_setDestAddr(DMA0, DMA0_CH1_CHAN_ID, (uint32_t)(&SPI_LCD_INST->spi->TXDATA));
    DL_DMA_setTransferSize(DMA0, DMA0_CH1_CHAN_ID, n);
    dma_done = 0;
    DL_DMA_enableChannel(DMA0, DMA0_CH1_CHAN_ID);
    while (!dma_done) { }
    while (DL_SPI_isBusy(SPI_LCD_INST)) { }
}
#endif

/* ---- ADC ---- */
#ifdef EDU_ADC12
static uint16_t adc12_read(ADC12_Regs *a, uint32_t chan)
{
    DL_ADC12_disableConversions(a);
    DL_ADC12_configConversionMem(a, DL_ADC12_MEM_IDX_0, chan, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_clearInterruptStatus(a, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(a);
    DL_ADC12_startConversion(a);
    for (uint32_t t0 = 0; t0 < 200000u; t0++)
        if (DL_ADC12_getRawInterruptStatus(a, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) break;
    uint16_t v = DL_ADC12_getMemResult(a, DL_ADC12_MEM_IDX_0);
    DL_ADC12_stopConversion(a);
    return v;
}
static uint16_t p_adc(void *c, edumkii_adc_t ch)
{
    static const struct { ADC12_Regs *inst; uint32_t chan; } t[EDUMKII_ADC_COUNT] = { EDU_ADC_TABLE };
    (void)c;
    return t[ch].inst ? adc12_read(t[ch].inst, t[ch].chan) : 0;
}
#endif
#ifdef EDU_HSADC
static uint16_t p_adc(void *c, edumkii_adc_t ch)
{
    static const uint8_t soc[EDUMKII_ADC_COUNT] = EDU_HSADC_SOC_TABLE;
    (void)c;
    DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
    p_delay_us(0, 12);                           /* six conversions after the force */
    return (uint16_t)DL_HSADC_getResult(HSADC1_RESULT_INST, (DL_HSADC_SOC_NUMBER)soc[ch]);
}
#endif

#ifdef EDU_HSADC_SEL_SOC
uint16_t edu_sel_analog(void)
{
    DL_HSADC_triggerSequencerSoftwareForce(HSADC1_INST, DL_HSADC_SEQ_NUMBER1);
    p_delay_us(0, 12);
    return (uint16_t)DL_HSADC_getResult(HSADC1_RESULT_INST, (DL_HSADC_SOC_NUMBER)EDU_HSADC_SEL_SOC);
}
#endif

/* ---- I2C by software, open drain by switching the output enable (the pins' pull-ups and the
 *      BoosterPack's own 10 kOhm resistors hold the lines high) ---- */
#define HALF_US 25u                                  /* 20 kHz */
static void lo(const pinref_t *p)  { DL_GPIO_enableOutput(p->port, p->pin); }
static void rel(const pinref_t *p) { DL_GPIO_disableOutput(p->port, p->pin); }
static int  rd(const pinref_t *p)  { return DL_GPIO_readPins(p->port, p->pin) != 0; }
static void hd(void) { p_delay_us(0, HALF_US); }
static void i2c_start(void) { rel(&sda); rel(&scl); hd(); lo(&sda); hd(); lo(&scl); hd(); }
static void i2c_stop(void)  { lo(&sda); hd(); rel(&scl); hd(); rel(&sda); hd(); }
static int  i2c_wbyte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) rel(&sda); else lo(&sda);
        hd(); rel(&scl); hd(); lo(&scl); hd();
    }
    rel(&sda); hd(); rel(&scl); hd();
    int ack = !rd(&sda);
    lo(&scl); hd();
    return ack;
}
static uint8_t i2c_rbyte(int ack)
{
    uint8_t v = 0;
    rel(&sda);
    for (int i = 0; i < 8; i++) {
        hd(); rel(&scl); hd();
        v = (uint8_t)((v << 1) | rd(&sda));
        lo(&scl); hd();
    }
    if (ack) lo(&sda); else rel(&sda);
    hd(); rel(&scl); hd(); lo(&scl); hd(); rel(&sda);
    return v;
}
static int p_i2c_write(void *c, uint8_t a, const uint8_t *d, uint32_t n)
{
    (void)c;
    i2c_start();
    if (!i2c_wbyte((uint8_t)(a << 1))) { i2c_stop(); return 1; }
    for (uint32_t i = 0; i < n; i++) if (!i2c_wbyte(d[i])) { i2c_stop(); return 1; }
    i2c_stop();
    return 0;
}
static int p_i2c_read(void *c, uint8_t a, uint8_t *d, uint32_t n)
{
    (void)c;
    i2c_start();
    if (!i2c_wbyte((uint8_t)((a << 1) | 1))) { i2c_stop(); return 1; }
    for (uint32_t i = 0; i < n; i++) d[i] = i2c_rbyte(i + 1 < n);
    i2c_stop();
    return 0;
}

static const edumkii_port_t port = {
    0, p_gpio_write, p_gpio_read,
#ifdef EDU_HAS_LCD
    p_spi_write,
#else
    0,
#endif
#ifdef EDU_SPI_DMA
    p_spi_bulk,
#else
    0,
#endif
    p_adc, 0 /* pwm_set: the buzzer is driven in software */, p_i2c_write, p_i2c_read, p_delay_us, p_delay_ms,
#ifdef EDU_SPI_DMA
    p_spi_async, p_spi_wait
#else
    0, 0
#endif
};

const edumkii_port_t *edu_port(void) { return &port; }

void edu_board_init(void)
{
    /* SysConfig left the I2C pins as outputs with the input stage off: turn the input stage and the
     * pull-up on, drive nothing, and let the output enable act as the open-drain switch. */
    DL_GPIO_initDigitalInputFeatures(scl.iomux, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(sda.iomux, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(scl.port, scl.pin);
    DL_GPIO_clearPins(sda.port, sda.pin);
    rel(&scl); rel(&sda);
#ifdef EDU_SPI_DMA
    NVIC_EnableIRQ(SPI_LCD_INST_INT_IRQN);
#endif
#if __CORTEX_M >= 3
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#else
    SysTick->LOAD = 0x00FFFFFFu;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
#endif
}

uint32_t edu_stamp(void)
{
#if __CORTEX_M >= 3
    return DWT->CYCCNT;
#else
    return SysTick->VAL;
#endif
}
uint32_t edu_cycles_since(uint32_t s)
{
#if __CORTEX_M >= 3
    return DWT->CYCCNT - s;
#else
    return (s - SysTick->VAL) & 0x00FFFFFFu;
#endif
}
uint32_t edu_cpu_hz(void) { return CPUCLK_FREQ; }

void edu_print(const char *s)
{
    while (*s) DL_UART_Main_transmitDataBlocking(UART_CONSOLE_INST, (uint8_t)*s++);
}
