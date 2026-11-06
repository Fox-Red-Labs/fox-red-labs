/* Halloween show on the LP-MSPM33C321A: board layer. Pins and clock: boards/mspm33c321a/halloween.syscfg.
 * The RGB LED is dimmed by software PWM from the 8 kHz sample tick: 32 levels, 250 Hz; 32 is fully on.
 * Buttons: A is S2 (PC17), B is S1 (PA18, the bootloader-invoke line, read only while the show runs), plain inputs
 * with no internal pull (the board's own resistors set them): each button's idle level is read at boot and the other level counts as pressed, so the
 * show does not depend on which way the board wires them (do not hold a button during power-up). */
#include "ti_msp_dl_config.h"
#include "board_audio.h"
#include "board_show.h"

static volatile uint8_t duty[3];       /* 0..32 for blue, red, green */
static uint8_t pwm_n;
volatile uint32_t g_btn_force;
static uint32_t a_idle, b_idle;

void SysTick_Handler(void)
{
    app_tick();
    /* software PWM, one step per sample: 32 steps, 250 Hz; the LEDs are active high on this board */
    uint8_t n = pwm_n = (uint8_t)((pwm_n + 1u) & 31u);
    if (n < duty[0]) DL_GPIO_setPins(RGB_BLUE_PORT, RGB_BLUE_PIN); else DL_GPIO_clearPins(RGB_BLUE_PORT, RGB_BLUE_PIN);
    if (n < duty[1]) DL_GPIO_setPins(RGB_RED_PORT, RGB_RED_PIN); else DL_GPIO_clearPins(RGB_RED_PORT, RGB_RED_PIN);
    if (n < duty[2]) DL_GPIO_setPins(RGB_GREEN_PORT, RGB_GREEN_PIN); else DL_GPIO_clearPins(RGB_GREEN_PORT, RGB_GREEN_PIN);
}
void board_tick_enable(void) { }
uint32_t board_cycles(void) { return DWT->CYCCNT; }
uint32_t board_cpu_hz(void) { return CPUCLK_FREQ; }

void board_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    delay_cycles(160000);
    a_idle = DL_GPIO_readPins(BTN_A_PORT, BTN_A_PIN);
    b_idle = DL_GPIO_readPins(BTN_B_PORT, BTN_B_PIN);
}
int board_show_mode(void) { return SHOW_MODE_PLAY; }
void board_show_reason(void (*puts_)(const char *))
{
    puts_("BOARD LP-MSPM33C321A at 160 MHz; buttons idle S2(PC17)=");
    puts_(a_idle ? "high" : "low"); puts_(" S1(PA18)="); puts_(b_idle ? "high" : "low");
    puts_("; RGB LED PWM 32 levels at 250 Hz; button A = S2, B = S1\r\n");
}
void board_buttons(int *a, int *b)
{
    *a = (DL_GPIO_readPins(BTN_A_PORT, BTN_A_PIN) != a_idle) || (g_btn_force & 1u);
    *b = (DL_GPIO_readPins(BTN_B_PORT, BTN_B_PIN) != b_idle) || (g_btn_force & 2u);
}
uint32_t board_seed(void)
{
    uint32_t h = 2166136261u ^ DWT->CYCCNT;
    for (int i = 0; i < 64; i++) { board_adc_start(); delay_cycles(3200); h = (h ^ (board_adc_read() & 0xFu)) * 16777619u; }
    return h ^ DWT->CYCCNT;
}
void board_rgb(const uint8_t rgb[3])
{
    /* a gentle curve, halfway between linear and squared, so dim themes still show and 255 is fully on (32 of 32) */
    for (int c = 0; c < 3; c++) {
        uint32_t v = rgb[c], d = (v + v * v / 255u) * 32u / 510u;
        duty[c == 0 ? 1 : c == 1 ? 2 : 0] = (uint8_t)d;
    }
}
uint32_t board_rgb_duty(void) { return (uint32_t)duty[1] << 16 | (uint32_t)duty[2] << 8 | duty[0]; }
void board_leds(int red, int green) { (void)red; (void)green; }
void board_ring_send(const uint8_t *buf, uint32_t n) { (void)buf; (void)n; }
int board_ring_busy(void) { return 0; }
void board_ring_info(void (*puts_)(const char *)) { (void)puts_; }

void board_sync(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SYNC_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SYNC_PIN); }
void board_sclk(int l) { if (l) DL_GPIO_setPins(DAC_PORT, DAC_SCLK_PIN); else DL_GPIO_clearPins(DAC_PORT, DAC_SCLK_PIN); }
void board_din(int l)  { if (l) DL_GPIO_setPins(DAC_PORT, DAC_DIN_PIN);  else DL_GPIO_clearPins(DAC_PORT, DAC_DIN_PIN); }
void board_amp(int mode)
{
    if (mode == 0) { DL_GPIO_disableOutput(DAC_PORT, DAC_AMP_PIN); return; }
    if (mode == 1) DL_GPIO_clearPins(DAC_PORT, DAC_AMP_PIN); else DL_GPIO_setPins(DAC_PORT, DAC_AMP_PIN);
    DL_GPIO_enableOutput(DAC_PORT, DAC_AMP_PIN);
}
/* BoosterPack pin 5 is not wired to the chip on this LaunchPad: the isolation resistor R21 between the header and PA21
 * is not fitted, and PA21 itself is the VREF- pin, tied to ground through R10 at the REF6033 (SLAU969 schematic). So
 * PA21 stays an input, and the pack's microphone gets its power from pin 8 (PB30) instead, through one jumper wire
 * on the pack's top header from pin 8 to pin 5. Without the wire the microphone is silent and nothing is heard. */
void board_mic_power(int on)
{
    DL_GPIO_disableOutput(MICPWR_PORT, MICPWR_ON_PIN);
    DL_GPIO_initDigitalInput(MICPWR_ON_IOMUX);
    if (on) DL_GPIO_setPins(MICALT_PORT, MICALT_PWR_PIN); else DL_GPIO_clearPins(MICALT_PORT, MICALT_PWR_PIN);
}

int board_has_mic(void) { return 1; }   /* with the pin 8 to pin 5 wire; without it the listener simply hears silence */
void board_adc_start(void) { DL_HSADC_triggerSequencerSoftwareForce(HSADC0_INST, DL_HSADC_SEQ_NUMBER1); }
uint16_t board_adc_read(void) { return (uint16_t)DL_HSADC_getResult(HSADC0_RESULT_INST, DL_HSADC_SOC_NUMBER0); }
void board_uart_put(uint8_t c) { DL_UART_Main_transmitDataBlocking(UART_0_INST, c); }
int board_uart_get(void) { return DL_UART_Main_isRXFIFOEmpty(UART_0_INST) ? -1 : (int)DL_UART_Main_receiveData(UART_0_INST); }
void board_delay_ms(uint32_t ms) { while (ms--) delay_cycles(CPUCLK_FREQ / 1000u); }

int main(void)
{
    SYSCFG_DL_init();
    app_main();
    return 0;
}
