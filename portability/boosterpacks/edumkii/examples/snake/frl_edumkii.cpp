/*
 * ports/arduboy platform for the Educational BoosterPack MKII on a DriverLib board
 * (implements ports/arduboy/include/frl/abstract.h with the portable EDUMKII core).
 *
 *   display  RGB565, 128 x 128, lines streamed with edumkii_blit (DMA bulk writes where the port has them)
 *   input    joystick X, Y from the ADC; S1 (and the stick press, when the board can read it) is A, S2 is B
 *   audio    non-blocking square wave on the buzzer pin from the SysTick interrupt
 *   clock    millis from the CPU cycle counter, delay from the port
 *
 * Also keeps a few counters in RAM (struct snk) for the debugger: frames, display time per frame.
 * Build: examples/snake/build_snake.sh. BSD-3-Clause.
 */
extern "C" {
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"
}
#include <frl/abstract.h>

extern void setup();
extern void loop();
extern "C" void __cxa_pure_virtual() { for (;;) {} }

static edumkii_t dev;

volatile struct {
    uint32_t magic;           /* 0xA0B0B001 once the game loop runs */
    uint32_t cpu_hz;
    uint32_t frames;          /* frames presented */
    uint32_t disp_cyc_last;   /* CPU cycles spent sending the last frame */
    uint32_t disp_cyc_min, disp_cyc_max;
    uint32_t lines_last;      /* line calls in the last frame */
    uint32_t frame_cyc_last;  /* cycles between the last two frames, whole loop included */
    uint32_t frame_cyc_min, frame_cyc_max;
    uint32_t tones;
    uint32_t press_a, press_b;/* edges seen on A and B */
    uint16_t joy_x, joy_y;
    uint8_t  buttons;
} snk;

/* ---- display ---- */
static uint32_t acc_cyc, acc_lines, last_flush_stamp;
static void d_lines(void *, uint16_t y0, uint16_t n, const void *pix)
{
    uint32_t t = edu_stamp();
    edumkii_blit(&dev, 0, y0, 128, n, (const uint16_t *)pix);
    acc_cyc += edu_cycles_since(t);
    acc_lines++;
}
/* Streaming path (Game Boy front end): one window per frame, rows queued with DMA while the next one is made. The time spent
 * in these calls is the time the CPU waited for the wire, and goes in the same counter as the blocking path. */
static void d_sbegin(void *) { edumkii_stream_begin(&dev, 0, 0, 128, 128); }
static void d_srow(void *, const void *row, uint16_t bytes)
{
    uint32_t t = edu_stamp();
    edumkii_stream_write(&dev, (const uint8_t *)row, bytes);
    acc_cyc += edu_cycles_since(t);
    acc_lines++;
}
static void d_send(void *)
{
    uint32_t t = edu_stamp();
    edumkii_stream_end(&dev);
    acc_cyc += edu_cycles_since(t);
}
static void d_flush(void *)
{
    uint32_t now = edu_stamp();
    snk.frames++;
    snk.disp_cyc_last = acc_cyc;
    if (snk.frames > 2) {
        if (acc_cyc < snk.disp_cyc_min) snk.disp_cyc_min = acc_cyc;
        if (acc_cyc > snk.disp_cyc_max) snk.disp_cyc_max = acc_cyc;
        uint32_t fc = now - last_flush_stamp;
        snk.frame_cyc_last = fc;
        if (fc < snk.frame_cyc_min) snk.frame_cyc_min = fc;
        if (fc > snk.frame_cyc_max) snk.frame_cyc_max = fc;
    }
    snk.lines_last = acc_lines;
    last_flush_stamp = now;
    acc_cyc = 0; acc_lines = 0;
}

/* ---- input ---- */
static uint8_t prev_a, prev_b;
static void i_read(void *, frl_input_raw_t *o)
{
    o->joy_x = edumkii_adc(&dev, EDUMKII_ADC_JOY_X);
    o->joy_y = edumkii_adc(&dev, EDUMKII_ADC_JOY_Y);
    int a = edumkii_button(&dev, EDUMKII_PIN_S1);
    int b = edumkii_button(&dev, EDUMKII_PIN_S2);
#ifdef EDU_SNAKE_AUTOSTART
    /* S1 shares the probe's serial line on some LaunchPad kits: press A once, two seconds after start-up */
    static uint32_t n; if (n < 400) n++;
    if (n > 120 && n < 160) a = 1;
#endif
#ifdef EDU_GB_SCRIPT
    /* Game Boy test script: tap S1 and S2 together (Start) every 60 reads for the first 2,400, to leave the intro */
    static uint32_t gn; if (gn < 100000) gn++;
    if (gn < 2400 && (gn % 60) < 6) { a = 1; b = 1; }
#endif
    o->buttons = (uint8_t)((a ? FRL_BTN_1 : 0) | (b ? FRL_BTN_2 : 0));
    if (a && !prev_a) snk.press_a++;
    if (b && !prev_b) snk.press_b++;
    prev_a = (uint8_t)a; prev_b = (uint8_t)b;
    snk.joy_x = o->joy_x; snk.joy_y = o->joy_y; snk.buttons = o->buttons;
}

/* ---- audio: SysTick toggles the buzzer pin, so tone() returns at once ---- */
static volatile uint32_t half_left;   /* half periods still to play; 0xFFFFFFFF = until the next call */
extern "C" void SysTick_Handler(void)
{
    if (half_left == 0) { SysTick->CTRL = 0; DL_GPIO_clearPins(E_BUZZER_PORT, E_BUZZER_BUZZER_PIN); return; }
    DL_GPIO_togglePins(E_BUZZER_PORT, E_BUZZER_BUZZER_PIN);
    if (half_left != 0xFFFFFFFFu) half_left--;
}
static void a_tone(void *, uint16_t hz, uint16_t ms)
{
    snk.tones++;
    SysTick->CTRL = 0;
    DL_GPIO_clearPins(E_BUZZER_PORT, E_BUZZER_BUZZER_PIN);
    if (hz == 0) { half_left = 0; return; }
    uint32_t load = edu_cpu_hz() / (2u * hz);
    if (load < 2u) load = 2u;
    if (load > 0xFFFFFFu) load = 0xFFFFFFu;
    half_left = ms ? (2u * (uint32_t)hz * ms) / 1000u : 0xFFFFFFFFu;
    if (half_left == 0) half_left = 1;
    SysTick->LOAD = load - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* ---- clock: milliseconds from the 32-bit cycle counter, folded in on every call ---- */
static uint64_t total_cyc; static uint32_t last_stamp;
static uint32_t c_millis(void *)
{
    uint32_t now = edu_stamp();
    total_cyc += (uint32_t)(now - last_stamp);
    last_stamp = now;
    return (uint32_t)(total_cyc / (edu_cpu_hz() / 1000u));
}
static void c_delay(void *, uint32_t ms) { dev.port.delay_ms(dev.port.ctx, ms); }
static uint32_t c_entropy(void *) { return edu_stamp() ^ ((uint32_t)edumkii_adc(&dev, EDUMKII_ADC_MIC) << 16); }

int main(void)
{
    SYSCFG_DL_init();
    edu_board_init();
    snk.cpu_hz = edu_cpu_hz();
    snk.disp_cyc_min = snk.frame_cyc_min = 0xFFFFFFFFu;
    edumkii_init(&dev, edu_port());
    edumkii_lcd_init(&dev);
    edumkii_backlight(&dev, 1);
    edumkii_joy_calibrate(&dev);
    last_stamp = edu_stamp();

    frl_platform_t pf = {};
    pf.display.fmt = FRL_FMT_RGB565;
    pf.display.fg = EDUMKII_WHITE; pf.display.bg = EDUMKII_BLACK;
    pf.display.write_lines = d_lines; pf.display.flush = d_flush;
    pf.display.stream_begin = d_sbegin; pf.display.stream_row = d_srow; pf.display.stream_end = d_send; pf.display.stream_be16 = 1;
    pf.input.read = i_read;
    pf.input.centre_x = dev.joy_x0; pf.input.centre_y = dev.joy_y0;
    pf.input.invert_x = 0;
    pf.input.invert_y = 0;            /* raw Y already rises as the stick is pushed up (by hand, 2026-10-23), as the layer wants */
    pf.audio.tone = a_tone;
    pf.millis = c_millis; pf.delay_ms = c_delay; pf.entropy = c_entropy;
    frl_set_platform(&pf);
#ifdef FRL_FIT_STRETCH
    frl_set_fit(FRL_FIT_2X_V);        /* fills the screen, every row doubled: pictures look twice as tall */
#else
    frl_set_fit(FRL_FIT_CENTER);      /* true shape, 1:1 in the middle, black bars above and below */
#endif
    setup();
    snk.magic = 0xA0B0B001u;
    for (;;) loop();
}
