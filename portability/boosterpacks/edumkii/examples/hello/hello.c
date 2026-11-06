/*
 * Educational BoosterPack MKII hello example, one source for every board.
 *
 * What it does: beeps the buzzer, then (where the screen is wired) draws a test pattern and a
 * title, times one full-screen fill, and shows live readouts. Every 100 ms it reads the joystick
 * and buttons, the accelerometer, the microphone level and the light sensor, lights the RGB LED by
 * the stick direction and buttons, beeps on a press, and once a second prints one line on the
 * console (UART TX, 115200). The same values sit in the RAM struct `hello` for a debugger.
 * BSD-3-Clause.
 */
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"

volatile struct {
    uint32_t magic;            /* 0xED0C0001 once running */
    uint32_t loops;
    uint32_t cpu_hz;
    int32_t  lux_status;       /* 0 ok, negative: error code of edumkii_lux_init */
    uint32_t lux_centi;        /* hundredths of a lux */
    uint32_t lcd_status;       /* 1 if the screen path ran */
    uint32_t fill_cycles;      /* one full 128 x 128 fill */
    uint32_t spi_hz;
    uint16_t joy_x, joy_y, joy_x0, joy_y0;
    uint16_t acc_x, acc_y, acc_z, mic_pp;
    uint32_t s1, s2, sel;      /* 1 = pressed */
    uint32_t s1_seen, s2_seen, sel_seen;   /* edges seen since start */
    uint32_t dir_seen;         /* OR of EDUMKII_DIR_* bits seen */
    uint32_t sel_analog;       /* M33 only: the press pin read as ADC1_7, 12-bit counts */
} hello;

static void put_int(int32_t v)
{
    char b[12]; int n = 0;
    uint32_t u = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    do { b[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) b[n++] = '-';
    char o[13]; int k = 0;
    while (n) o[k++] = b[--n];
    o[k] = 0;
    edu_print(o);
}

#ifdef EDU_HAS_LCD
static void test_pattern(edumkii_t *d)
{
    static const uint16_t bars[8] = { EDUMKII_WHITE, EDUMKII_YELLOW, EDUMKII_CYAN, EDUMKII_GREEN,
                                      EDUMKII_MAGENTA, EDUMKII_RED, EDUMKII_BLUE, EDUMKII_BLACK };
    for (int i = 0; i < 8; i++) edumkii_rect(d, i * 16, 0, 16, 40, bars[i]);
    for (int x = 0; x < 128; x++) edumkii_rect(d, x, 40, 1, 12, EDUMKII_RGB(x * 2, x * 2, x * 2));
    edumkii_frame(d, 0, 0, 128, 128, EDUMKII_WHITE);
    edumkii_line(d, 0, 52, 127, 127, EDUMKII_GREY);
    edumkii_text(d, 6, 58, "EDUMKII hello", EDUMKII_WHITE, EDUMKII_BLACK, 1);
    edumkii_text(d, 6, 68, EDU_BOARD_NAME, EDUMKII_YELLOW, EDUMKII_BLACK, 1);
}

static void show(edumkii_t *d, int dx, int dy, int dir, uint32_t lux)
{
    const uint16_t fg = EDUMKII_WHITE, bg = EDUMKII_BLACK;
    int x;
    edumkii_text(d, 6, 80, "JOY", fg, bg, 1);
    x = edumkii_number(d, 30, 80, dx, 5, EDUMKII_CYAN, bg, 1);
    edumkii_number(d, x + 6, 80, dy, 5, EDUMKII_CYAN, bg, 1);
    edumkii_text(d, 6, 90, "DIR", fg, bg, 1);
    edumkii_text(d, 30, 90, (dir & EDUMKII_DIR_LEFT) ? "L" : "-", EDUMKII_GREEN, bg, 1);
    edumkii_text(d, 38, 90, (dir & EDUMKII_DIR_RIGHT) ? "R" : "-", EDUMKII_GREEN, bg, 1);
    edumkii_text(d, 46, 90, (dir & EDUMKII_DIR_UP) ? "U" : "-", EDUMKII_GREEN, bg, 1);
    edumkii_text(d, 54, 90, (dir & EDUMKII_DIR_DOWN) ? "D" : "-", EDUMKII_GREEN, bg, 1);
    edumkii_text(d, 6, 100, "BTN", fg, bg, 1);
    edumkii_text(d, 30, 100, hello.s1 ? "S1" : "--", EDUMKII_RED, bg, 1);
    edumkii_text(d, 48, 100, hello.s2 ? "S2" : "--", EDUMKII_GREEN, bg, 1);
    edumkii_text(d, 66, 100, hello.sel ? "SEL" : "---", EDUMKII_BLUE, bg, 1);
    edumkii_text(d, 6, 110, "LUX", fg, bg, 1);
    edumkii_number(d, 30, 110, (int32_t)(lux / 100u), 5, EDUMKII_YELLOW, bg, 1);
    edumkii_text(d, 72, 110, "MIC", fg, bg, 1);
    edumkii_number(d, 96, 110, hello.mic_pp, 4, EDUMKII_YELLOW, bg, 1);
}
#endif

int main(void)
{
    static edumkii_t d;                 /* static: the screen scratch buffer stays out of the stack */
    SYSCFG_DL_init();
    edu_board_init();
    hello.cpu_hz = edu_cpu_hz();
    edumkii_init(&d, edu_port());
    edu_print("\r\nEDUMKII hello on " EDU_BOARD_NAME "\r\n");

    edumkii_tone(&d, 2000, 120);

#ifdef EDU_HAS_LCD
    if (edumkii_lcd_init(&d) == EDUMKII_OK) {
        edumkii_backlight(&d, 1);
        uint32_t t0 = edu_stamp();
        edumkii_fill(&d, EDUMKII_BLUE);
        hello.fill_cycles = edu_cycles_since(t0);
        test_pattern(&d);
        hello.lcd_status = 1;
        edu_print("screen: fill cycles ");
        put_int((int32_t)hello.fill_cycles);
        edu_print("\r\n");
    }
#endif

    hello.lux_status = edumkii_lux_init(&d);
    edu_print("light sensor: ");
    edu_print(hello.lux_status == 0 ? "ok\r\n" : "no answer\r\n");

    edumkii_joy_calibrate(&d);
    hello.joy_x0 = d.joy_x0; hello.joy_y0 = d.joy_y0;
    hello.magic = 0xED0C0001u;

    uint32_t prev_s1 = 0, prev_s2 = 0, prev_sel = 0;
    for (;;) {
        int dx = 0, dy = 0;
        edumkii_joy_read(&d, &dx, &dy);
        int dir = edumkii_joy_dir(&d, 600);
        hello.dir_seen |= (uint32_t)dir;
        hello.joy_x = (uint16_t)(dx + hello.joy_x0); hello.joy_y = (uint16_t)(dy + hello.joy_y0);
        uint16_t ax, ay, az;
        edumkii_accel_read(&d, &ax, &ay, &az);
        hello.acc_x = ax; hello.acc_y = ay; hello.acc_z = az;
        hello.mic_pp = edumkii_mic_level(&d, 32);
        hello.s1 = (uint32_t)edumkii_button(&d, EDUMKII_PIN_S1);
        hello.s2 = (uint32_t)edumkii_button(&d, EDUMKII_PIN_S2);
        hello.sel = (uint32_t)edumkii_button(&d, EDUMKII_PIN_JOY_SEL);
#ifdef EDU_HSADC_SEL_SOC
        hello.sel_analog = edu_sel_analog();
#endif
        if (hello.s1 && !prev_s1) { hello.s1_seen++; edumkii_tone(&d, 1000, 40); }
        if (hello.s2 && !prev_s2) { hello.s2_seen++; edumkii_tone(&d, 1500, 40); }
        if (hello.sel && !prev_sel) { hello.sel_seen++; edumkii_tone(&d, 2500, 40); }
        prev_s1 = hello.s1; prev_s2 = hello.s2; prev_sel = hello.sel;

        /* LED: buttons pick a color, otherwise the stick does, otherwise a slow rotation */
        int r = hello.s1 || (dir & EDUMKII_DIR_LEFT), g = hello.s2 || (dir & EDUMKII_DIR_RIGHT);
        int b = hello.sel || (dir & (EDUMKII_DIR_UP | EDUMKII_DIR_DOWN));
        if (!r && !g && !b) { int ph = (int)((hello.loops / 10u) % 3u); r = ph == 0; g = ph == 1; b = ph == 2; }
        edumkii_led(&d, r, g, b);

        uint32_t lux = 0;
        if (hello.lux_status == 0 && edumkii_lux_read(&d, &lux) == EDUMKII_OK) hello.lux_centi = lux;
#ifdef EDU_HAS_LCD
        if (hello.lcd_status) show(&d, dx, dy, dir, hello.lux_centi);
#endif
        if (hello.loops % 10u == 0u) {
            edu_print("joy "); put_int(hello.joy_x); edu_print(" "); put_int(hello.joy_y);
            edu_print(" d "); put_int(dx); edu_print(" "); put_int(dy);
            edu_print(" acc "); put_int(ax); edu_print(" "); put_int(ay); edu_print(" "); put_int(az);
            edu_print(" mic "); put_int(hello.mic_pp);
            edu_print(" lux "); put_int((int32_t)(hello.lux_centi / 100u));
            edu_print(" btn "); put_int((int32_t)(hello.s1 + 2 * hello.s2 + 4 * hello.sel));
#ifdef EDU_HSADC_SEL_SOC
            edu_print(" selA "); put_int((int32_t)hello.sel_analog);
#endif
            edu_print("\r\n");
        }
        hello.loops++;
        d.port.delay_ms(d.port.ctx, 60);
    }
}
