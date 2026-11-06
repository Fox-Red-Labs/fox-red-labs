/* Halloween show on the LP-AM13E230: board layer with a boot interlock. Pins: halloween.syscfg.
 *
 * Interlock (why): the BOOSTXL-AUDIO's DAC and amplifier lines are BoosterPack pins 7, 15 and 13. On the DRV8376EVM
 * the same pins are SCLK, SDI and DRVOFF (SLVUD20 tables 2-1 and 2-2), and driving pin 13 low, which turns the pack's
 * amplifier on, would enable the motor driver's gate drivers. So the image reads which board is stacked before it
 * drives any of them, and leaves them as inputs unless the Audio pack is found.
 *
 * How it tells: the DRV8376EVM puts a 143 k over 6.04 k divider with 100 nF on each of its four voltage-sense lines,
 * VSENVM, VSENC, VSENB and VSENA (SLVUD20 figure 4-1, R37 to R44), which land on J3 pins 23, 24, 25 and 26 (PA16,
 * PA18, PA28, PA17). With the chip's internal pull-up on, each reads low through the 6.04 k to ground, motor supply on
 * or off. The BOOSTXL-AUDIO uses none of J3 23 to 26 (SLAU670A tables 1 and 2: pins 5, 6, 7, 12, 13, 15; J5 options
 * 19 and 30; resistor alternates 26 and 31 are not fitted as shipped), so all four read high. Play mode needs all four
 * high on every one of 16 reads; any low, or any line that changes, means silent mode. Holding the wake button
 * (PB13) during power-up forces play mode for a pack the check misjudges. Nothing else can force it.
 * A bare board reads like the Audio pack (nothing on J3 23 to 26) and plays. Before any of the pack's lines is set
 * up, the guard also reads pins 5, 7, 13 and 15 as inputs with the pull-up and then the pull-down on, and prints
 * them: a bare header pin follows the pull, a pin a stacked board holds does not.
 * The EVM branch is reasoned from SLVUD20; the board here has no EVM on it. */
#include "device.h"
#include "ti_sdk_dl_config.h"
#include "board_audio.h"
#include "board_show.h"

int _system_pre_init(void) { SCB->CPACR |= (3UL << 0); __DSB(); __ISB(); return 1; } /* the TMU trap from am13e230/AGENTS.md */

void SysTick_Handler(void) { app_tick(); }
void board_tick_enable(void) { }
uint32_t board_cycles(void) { return DWT->CYCCNT; }
uint32_t board_cpu_hz(void) { return CPUCLK_FREQ_HZ; }

/* interlock pins: port A for the sense lines, port B for the button and the pack lines */
#define SENSE_N 4
static const uint32_t sense_cm[SENSE_N] = { IOMUX_PINCM_PA16, IOMUX_PINCM_PA18, IOMUX_PINCM_PA28, IOMUX_PINCM_PA17 };
static const uint32_t sense_pin[SENSE_N] = { DL_GPIO_PIN(16), DL_GPIO_PIN(18), DL_GPIO_PIN(28), DL_GPIO_PIN(17) };
static const char *const sense_name[SENSE_N] = { "J3.23 PA16", "J3.24 PA18", "J3.25 PA28", "J3.26 PA17" };
#define BTN_CM IOMUX_PINCM_PB13
#define BTN_PIN DL_GPIO_PIN(13)
static int mode = SHOW_MODE_SILENT;
static uint8_t s_hi[SENSE_N], s_lo[SENSE_N], btn_lo, btn_hi;
static uint32_t pins_after;  /* PB5, PB10, PB18 output-enable bits read back after the decision */
/* pins 5, 7, 13, 15 = PA3, PB5, PB18, PB10 */
static const uint32_t bare_cm[4] = { IOMUX_PINCM_PA3, IOMUX_PINCM_PB5, IOMUX_PINCM_PB18, IOMUX_PINCM_PB10 };
static GPIO_Regs *const bare_port[4] = { GPIO0, GPIO1, GPIO1, GPIO1 };
static const uint32_t bare_pin[4] = { DL_GPIO_PIN(3), DL_GPIO_PIN(5), DL_GPIO_PIN(18), DL_GPIO_PIN(10) };
static uint8_t bare_up, bare_down;   /* bit i: pin i of {5, 7, 13, 15} read high */
#define BTNB_CM IOMUX_PINCM_PA6
#define BTNB_PIN DL_GPIO_PIN(6)
volatile uint32_t g_btn_force;       /* test hook: a debugger write of bit 0 (A) or bit 1 (B) holds that button */

static void bare_pins(void)
{
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 4; i++)
            DL_GPIO_initDigitalInputFeatures(bare_cm[i], DL_GPIO_INVERSION_DISABLE,
                pass ? DL_GPIO_RESISTOR_PULL_DOWN : DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE);
        DEVICE_DELAY_MS(5);
        uint8_t m = 0;
        for (int i = 0; i < 4; i++) if (DL_GPIO_readPins(bare_port[i], bare_pin[i])) m |= (uint8_t)(1u << i);
        if (pass) bare_down = m; else bare_up = m;
    }
    for (int i = 0; i < 4; i++) DL_GPIO_initDigitalInput(bare_cm[i]);   /* plain inputs again, no pull */
}

static void interlock(void)
{
    bare_pins();
    for (int i = 0; i < SENSE_N; i++)
        DL_GPIO_initDigitalInputFeatures(sense_cm[i], DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(BTN_CM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_WAKEUP_DISABLE);
    DEVICE_DELAY_MS(20);  /* the EVM's 100 nF settles in under 1 ms through 6.04 k */
    for (int n = 0; n < 16; n++) {
        uint32_t a = DL_GPIO_readPins(GPIO0, DL_GPIO_PIN(16) | DL_GPIO_PIN(17) | DL_GPIO_PIN(18) | DL_GPIO_PIN(28));
        for (int i = 0; i < SENSE_N; i++) { if (a & sense_pin[i]) s_hi[i]++; else s_lo[i]++; }
        if (DL_GPIO_readPins(GPIO1, BTN_PIN)) btn_hi++; else btn_lo++;
        DEVICE_DELAY_MS(1);
    }
    int audio = 1;
    for (int i = 0; i < SENSE_N; i++) if (s_hi[i] != 16) audio = 0;
    int forced = (btn_lo == 16);
    mode = (audio || forced) ? SHOW_MODE_PLAY : SHOW_MODE_SILENT;
    /* back to plain inputs, no pulls: nothing is left pulling on the EVM's sense lines */
    for (int i = 0; i < SENSE_N; i++) DL_GPIO_initDigitalInput(sense_cm[i]);
    if (mode == SHOW_MODE_PLAY) {
        DL_GPIO_clearPins(GPIO1, DL_GPIO_PIN(5) | DL_GPIO_PIN(10));
        DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB5);
        DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB10);
        DL_GPIO_enableOutput(GPIO1, DL_GPIO_PIN(5) | DL_GPIO_PIN(10));
        DL_GPIO_clearPins(GPIO0, DL_GPIO_PIN(3));                      /* microphone power, pin 5: off for now */
        DL_GPIO_initDigitalOutput(IOMUX_PINCM_PA3);
        DL_GPIO_enableOutput(GPIO0, DL_GPIO_PIN(3));
    }
    DL_GPIO_initDigitalInputFeatures(BTNB_CM, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, DL_GPIO_WAKEUP_DISABLE);
    pins_after = GPIO1->DOE31_0 & (DL_GPIO_PIN(5) | DL_GPIO_PIN(10) | DL_GPIO_PIN(18));
}

int board_show_mode(void) { return mode; }

void board_show_reason(void (*puts_)(const char *))
{
    static char line[24];
    puts_("INTERLOCK sense (16 reads with pull-up, high count):");
    for (int i = 0; i < SENSE_N; i++) {
        int k = 0; const char *p = sense_name[i];
        line[k++] = ' '; while (*p) line[k++] = *p++; line[k++] = '=';
        if (s_hi[i] >= 10) line[k++] = (char)('0' + s_hi[i] / 10);
        line[k++] = (char)('0' + s_hi[i] % 10); line[k] = 0;
        puts_(line);
    }
    puts_(btn_lo == 16 ? " wake_button=held" : " wake_button=up");
    puts_(DL_GPIO_readPins(GPIO0, BTNB_PIN) ? " s5=down" : " s5=up");
    static char bp[40] = " bare_pins(5,7,13,15) up=0000 down=0000";
    for (int i = 0; i < 4; i++) { bp[25 + i] = (char)('0' + ((bare_up >> i) & 1u)); bp[35 + i] = (char)('0' + ((bare_down >> i) & 1u)); }
    puts_(bp);
    puts_(" pack_lines_out=");
    puts_(pins_after & DL_GPIO_PIN(5) ? "7" : "-"); puts_(pins_after & DL_GPIO_PIN(10) ? "15" : "-"); puts_(pins_after & DL_GPIO_PIN(18) ? "13" : "-");
    puts_(" buttons: A = S2 (wake), B = S5 (needs J11)");
    puts_("\r\nREASON ");
    int any_lo = 0, mixed = 0;
    for (int i = 0; i < SENSE_N; i++) { if (s_lo[i]) any_lo = 1; if (s_lo[i] && s_hi[i]) mixed = 1; }
    if (btn_lo == 16) puts_("wake button held at power-up: play forced");
    else if (mode == SHOW_MODE_PLAY) puts_("J3 23 to 26 all high: no DRV8376EVM sense dividers, Audio pack assumed: play");
    else if (mixed) puts_("a sense line changed while read: unclear, silent mode");
    else if (any_lo) puts_("DRV8376EVM sense dividers found on J3 23 to 26: silent mode, pins 7, 13, 15 left as inputs");
    puts_("\r\n");
}

void board_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_ADC_powerUp(MIC_ADC_INST);
    DEVICE_DELAY_US(5000);
    interlock();
}
void board_sync(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SYNC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SYNC_PIN); }
void board_sclk(int l) { if (mode != SHOW_MODE_PLAY) return; if (l) DL_GPIO_setPins(GPIO1, DL_GPIO_PIN(5)); else DL_GPIO_clearPins(GPIO1, DL_GPIO_PIN(5)); }
void board_din(int l)  { if (mode != SHOW_MODE_PLAY) return; if (l) DL_GPIO_setPins(GPIO1, DL_GPIO_PIN(10)); else DL_GPIO_clearPins(GPIO1, DL_GPIO_PIN(10)); }
void board_amp(int m)
{
    if (mode != SHOW_MODE_PLAY) return;  /* never drive pin 13 unless the Audio pack was found */
    if (m == 0) { DL_GPIO_disableOutput(GPIO1, DL_GPIO_PIN(18)); return; }
    if (m == 1) DL_GPIO_clearPins(GPIO1, DL_GPIO_PIN(18)); else DL_GPIO_setPins(GPIO1, DL_GPIO_PIN(18));
    DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB18);
    DL_GPIO_enableOutput(GPIO1, DL_GPIO_PIN(18));
}
void board_mic_power(int on)
{
    if (mode != SHOW_MODE_PLAY) return;
    if (on) DL_GPIO_setPins(GPIO0, DL_GPIO_PIN(3)); else DL_GPIO_clearPins(GPIO0, DL_GPIO_PIN(3));
}
void board_adc_start(void) { DL_ADC_forceSequencer(MIC_ADC_INST, MIC_ADC_SEQ1); }
uint16_t board_adc_read(void) { return DL_ADC_readResult(MIC_ADC_RESULT_INST, MIC_ADC_SOC0); }

/* buttons: A is S2 (wake, PB13), B is S5 (bootloader invoke, PA6, reaches the chip only with J11 fitted); both
 * S2 is active low (internal pull-up), S5 active high (the bootloader-invoke line idles low; internal pull-down,
 * so an open J11 reads as not pressed; seen 2026-10-25: with a pull-up the open line read as held). They are read
 * only while the show runs; the bootloader looks at S5 only during a reset. */
void board_buttons(int *a, int *b)
{
    *a = !DL_GPIO_readPins(GPIO1, BTN_PIN) || (g_btn_force & 1u);
    *b = (DL_GPIO_readPins(GPIO0, BTNB_PIN) != 0) || (g_btn_force & 2u);
}

/* a seed for shuffle: the low bits of 64 microphone ADC reads and the cycle counter */
uint32_t board_seed(void)
{
    uint32_t h = 2166136261u ^ DWT->CYCCNT;
    for (int i = 0; i < 64; i++) {
        board_adc_start(); DEVICE_DELAY_US(20);
        h = (h ^ (board_adc_read() & 0xFu)) * 16777619u;
    }
    return h ^ DWT->CYCCNT;
}

void board_leds(int red, int green)  /* both active low */
{
    if (red) DL_GPIO_clearPins(LEDS_RED_PORT, LEDS_RED_PIN); else DL_GPIO_setPins(LEDS_RED_PORT, LEDS_RED_PIN);
    if (green) DL_GPIO_clearPins(LEDS_GREEN_PORT, LEDS_GREEN_PIN); else DL_GPIO_setPins(LEDS_GREEN_PORT, LEDS_GREEN_PIN);
}

void board_rgb(const uint8_t rgb[3]) { (void)rgb; }   /* no RGB LED on this board */
uint32_t board_rgb_duty(void) { return 0; }

void board_ring_send(const uint8_t *buf, uint32_t n)
{
    DL_DMA_setSrcAddr(DMA0, RING_DMA_CHANNEL_ID, (uint32_t)buf);
    DL_DMA_setDestAddr(DMA0, RING_DMA_CHANNEL_ID, (uint32_t)&RING_SPI_INST->spi->TXDATA);
    DL_DMA_setTransferSize(DMA0, RING_DMA_CHANNEL_ID, (uint16_t)n);
    DL_DMA_enableChannel(DMA0, RING_DMA_CHANNEL_ID);
}
static void hex8(void (*puts_)(const char *), uint32_t v)
{
    static char b[9]; for (int i = 0; i < 8; i++) b[i] = "0123456789abcdef"[(v >> (28 - 4 * i)) & 15u]; b[8] = 0; puts_(b);
}
void board_ring_info(void (*puts_)(const char *))
{
    puts_("RING UC0 SPI CLKDIV=0x"); hex8(puts_, UC0_SPI->CLKDIV); puts_(" CLKSEL=0x"); hex8(puts_, UC0_SPI->CLKSEL);
    puts_(" CTL0=0x"); hex8(puts_, UC0_SPI->CTL0); puts_(" CLKCTL=0x"); hex8(puts_, UC0_SPI->CLKCTL);
    puts_(" IFLS=0x"); hex8(puts_, UC0_SPI->IFLS); puts_(" scr="); 
    static char d[4]; uint32_t scr = DL_SPI_getBitRateSerialClockDivider(RING_SPI_INST); d[0] = (char)('0' + scr / 10u); d[1] = (char)('0' + scr % 10u); d[2] = 0; puts_(d);
    puts_(" PA26_PINCM=0x"); hex8(puts_, IOMUX->SECCFG.PINCM[IOMUX_PINCM_PA26]); puts_("\r\n");
}
int board_ring_busy(void) { return DL_DMA_isChannelEnabled(DMA0, RING_DMA_CHANNEL_ID) || DL_SPI_isBusy(RING_SPI_INST); }

void board_uart_put(uint8_t c) { while (DL_UART_isTXFIFOFull(UART_0_INST)) { } DL_UART_transmitData(UART_0_INST, c); }
int board_uart_get(void) { return DL_UART_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_receiveData(UART_0_INST); }
void board_delay_ms(uint32_t ms) { DEVICE_DELAY_MS(ms); }

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    app_main();
    return 0;
}
