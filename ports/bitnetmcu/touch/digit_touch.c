/*
 * Draw a digit with a finger, and the LP-MSPM0G3507 reads it: BitNetMCU's 64-wide CNN
 * (99.55% on MNIST) on the Waveshare 3.5 inch touch LCD.
 *
 * Portrait 320 x 480. A 288 x 288 drawing box; lift the finger for half a second and the board
 * shows its answer and how long it took. Touch the box again for a new digit, or CLEAR.
 *
 * The drawing is turned into the 16 x 16 input the model was trained on the way MNIST digits were
 * made: the digit's larger side scaled to 20 of 28 pixels, its center of mass put in the middle,
 * then shrunk to 16 x 16 and normalized like the training data (BitNetMCU test_inference.py).
 *
 * At power-up a self-test feeds the 10 test digits through the same drawing path and records how
 * many it reads right; results sit in `dt` in RAM for the debugger.
 * GPL-3.0, like BitNetMCU (third-party/bitnetmcu/LICENSE; copy in ../LICENSE).
 */
#include "ti_msp_dl_config.h"
#include "bench_timer.h"

#define W 320
#define H 480
#define TP_ADDR 0x38
static void delay_ms(uint32_t ms) { while (ms--) delay_cycles(CPUCLK_FREQ / 1000); }
#include "lcd_touch.h"
#include "font5x7.h"
#include "bitnet_run.c"

#include "prepare.h"

/* CLEAR button */
#define CBX0 196
#define CBY0 404
#define CBX1 307
#define CBY1 466

#define WHITE 0xFFFF
#define BLACK 0x0000
#define GREY  0x7BEF
#define DARK  0x2104
#define RED   0xB0A3            /* close to Fox Red #B42418 in 16-bit color */

volatile struct {
    uint32_t magic;             /* 0xD1617001 after the self-test */
    uint32_t selftest_right;    /* of 10 test digits, read right through the drawing path */
    uint32_t selftest_pred[10];
    uint32_t answers;           /* digits classified from touch */
    uint32_t last_pred;
    uint32_t last_cycles;       /* inference only */
    uint32_t last_prep_cycles;  /* drawing to 16 x 16 input */
    uint32_t tp_reads_ok, tp_errors, tp_touches; /* touch diagnostics */
    uint16_t tp_last_x, tp_last_y;
    uint32_t tp_id;             /* reg 0xA8, FocalTech 0x11 */
    int8_t last_input[256];
} dt;

static void text(int x, int y, const char *s, int k, uint16_t fg, uint16_t bg)
{
    for (; *s; s++, x += 6 * k) {
        int g = 0;
        while (font_chars[g] && font_chars[g] != *s) g++;
        lcd_fill(x, y, x + 6 * k - 1, y + 7 * k - 1, bg);
        if (!font_chars[g]) continue;
        for (int c = 0; c < 5; c++)
            for (int r = 0; r < 7; r++)
                if (font_cols[g][c] & (1 << r)) lcd_fill(x + c * k, y + r * k, x + c * k + k - 1, y + r * k + k - 1, fg);
    }
}

static void clear_box(void)
{
    for (unsigned i = 0; i < sizeof grid; i++) grid[i] = 0;
    lcd_fill(BX, BY, BX + BS - 1, BY + BS - 1, BLACK);
}

static void clear_result(void) { lcd_fill(0, 352, CBX0 - 4, H - 1, DARK); }

static void screen(void)
{
    lcd_fill(0, 0, W - 1, H - 1, DARK);
    text(16, 14, "DRAW A DIGIT", 3, WHITE, DARK);
    lcd_fill(BX - 2, BY - 2, BX + BS + 1, BY + BS + 1, GREY);
    clear_box();
    lcd_fill(CBX0, CBY0, CBX1, CBY1, RED);
    text(CBX0 + 26, CBY0 + 24, "CLEAR", 2, WHITE, RED);
    text(CBX0, 360, "BITNETMCU", 2, GREY, DARK);
    text(CBX0, 380, "MSPM0G3507", 2, GREY, DARK);
}

/* stamp a round brush at cell (cx, cy); draw only cells that were not set yet */
static void stamp(int cx, int cy)
{
    for (int dy = -BRUSH; dy <= BRUSH; dy++)
        for (int dx = -BRUSH; dx <= BRUSH; dx++) {
            int x = cx + dx, y = cy + dy;
            if (dx * dx + dy * dy > BRUSH * BRUSH || x < 0 || y < 0 || x >= N || y >= N || GET(x, y)) continue;
            SET(x, y);
            lcd_fill(BX + x * CELL, BY + y * CELL, BX + x * CELL + CELL - 1, BY + y * CELL + CELL - 1, WHITE);
        }
}

/* a line of stamps between two touch samples, so a fast stroke has no gaps */
static void stroke(int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0, dy = y1 - y0, n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (n == 0) { stamp(x0, y0); return; }
    for (int i = 0; i <= n; i++) stamp(x0 + dx * i / n, y0 + dy * i / n);
}

static void show(uint32_t pred, uint32_t cycles)
{
    char ms[8]; uint32_t t = (uint32_t) ((uint64_t) cycles * 1000 / CPUCLK_FREQ);
    int k = 0; if (t >= 100) ms[k++] = '0' + t / 100; if (t >= 10) ms[k++] = '0' + t / 10 % 10; ms[k++] = '0' + t % 10;
    ms[k++] = ' '; ms[k++] = 'M'; ms[k++] = 'S'; ms[k] = 0;
    char d[2] = {(char) ('0' + pred), 0};
    clear_result();
    text(24, 360, d, 14, WHITE, DARK);
    text(110, 372, "IS IT", 2, GREY, DARK);
    text(110, 420, ms, 2, WHITE, DARK);
}

static uint32_t classify(int draw)
{
    int8_t in[256];
    uint64_t t0 = bench_cycles();
    if (!prepare(in)) return 99;
    uint64_t t1 = bench_cycles();
    uint32_t p = BitMnistInference(in);
    uint64_t t2 = bench_cycles();
    dt.last_pred = p; dt.last_prep_cycles = (uint32_t) (t1 - t0); dt.last_cycles = (uint32_t) (t2 - t1);
    for (int k = 0; k < 256; k++) dt.last_input[k] = in[k];
    if (draw) show(p, dt.last_cycles);
    return p;
}

/* self-test: draw each test digit into the box (pixels above a threshold become ink) and read it back */
static void selftest(void)
{
    for (int t = 0; t < N_TEST; t++) {
        for (unsigned i = 0; i < sizeof grid; i++) grid[i] = 0;
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++)
                if (test_in[t][(y * 16 / N) * 16 + x * 16 / N] > 40) SET(x, y);
        dt.selftest_pred[t] = classify(0);
        dt.selftest_right += dt.selftest_pred[t] == *test_label[t];
    }
    for (unsigned i = 0; i < sizeof grid; i++) grid[i] = 0;
    dt.magic = 0xD1617001u;
}

int main(void)
{
    SYSCFG_DL_init();
    DL_SPI_setBitRateSerialClockDivider(LCD_SPI_INST, 1);
    /* software I2C on PB3 (SDA) and PB2 (SCL): the pins must be plain GPIO with pull-ups, not the I2C
       block (as display_test.c sets them before it reads touch); "low" = output on at 0, "high" = released */
    DL_GPIO_initDigitalOutput(GPIO_TP_I2C_IOMUX_SDA);
    DL_GPIO_initDigitalOutput(GPIO_TP_I2C_IOMUX_SCL);
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_TP_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_3 | DL_GPIO_PIN_2);
    bench_timer_start();
    selftest();
    DL_GPIO_setPins(PA_PORT, PA_LCD_BL_PIN);
    lcd_init();
    screen();
    { uint8_t v = 0; if (tp_read(0xA8, &v, 1) == 0) dt.tp_id = v; else dt.tp_id = 0xEE; }

    int drawn = 0, shown = 0, px = -1, py = -1; uint64_t last = 0;
    const uint64_t lift = (uint64_t) CPUCLK_FREQ / 2;          /* half a second without a finger */
    for (;;) {
        uint8_t p[5];
        int rc = tp_read(0x02, p, 5);
        if (rc == 0) dt.tp_reads_ok++; else dt.tp_errors++;
        int touch = rc == 0 && (p[0] & 0x0F) > 0;
        if (touch) {
            int x = ((p[1] & 0x0F) << 8) | p[2], y = ((p[3] & 0x0F) << 8) | p[4];
            dt.tp_touches++; dt.tp_last_x = x; dt.tp_last_y = y;
            if (x >= CBX0 && x <= CBX1 && y >= CBY0 && y <= CBY1) {
                clear_box(); clear_result(); drawn = shown = 0; px = -1;
            } else if (x >= BX && x < BX + BS && y >= BY && y < BY + BS) {
                if (shown) { clear_box(); clear_result(); shown = 0; px = -1; }
                int cx = (x - BX) / CELL, cy = (y - BY) / CELL;
                if (px >= 0 && bench_cycles() - last < (uint64_t) CPUCLK_FREQ / 20) stroke(px, py, cx, cy);
                else stamp(cx, cy);
                px = cx; py = cy; drawn = 1; last = bench_cycles();
            }
        } else if (drawn && !shown && bench_cycles() - last > lift) {
            classify(1); dt.answers++; shown = 1; drawn = 0; px = -1;
        }
        delay_ms(5);
    }
}
