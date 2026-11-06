/* Launcher, main loop and the measurements. Games never touch hardware: they get gestures and draw through gfx.h. */
#include "game.h"
#include "gfx.h"
#include "hal.h"

#ifndef POLL_MS
#define POLL_MS 10               /* touch poll period */
#endif
#ifndef BOOT_GAME
#define BOOT_GAME 0              /* 0 launcher, 1 start in 2048, 2 start in minesweeper */
#endif
#ifndef BENCH
#define BENCH 1                  /* run the scripted measurement once at boot, report it, then play */
#endif
#ifndef BUILD_NAME
#define BUILD_NAME "touch-games"
#endif

/* Everything measured, in RAM at a fixed symbol so a debugger can read it as well as the log. Times in microseconds. */
typedef struct {
    uint32_t magic;               /* 0x7C6A0000 + stage reached */
    uint32_t full_menu_us, full_2048_us, full_mines_us;       /* clear screen and draw everything */
    uint32_t mv2048_n, mv2048_avg_us, mv2048_max_us;          /* one swipe: slide, spawn, redraw the changed tiles */
    uint32_t dig_n, dig_avg_us, dig_max_us;                   /* all dig taps over several boards (each tap that drew anything) */
    uint32_t dig1_n, dig1_us;                                 /* taps that redrew exactly one cell: count, mean time */
    uint32_t first_n, first_avg_us, first_max_us;             /* the first dig of a board (it may open a region) */
    uint32_t fill_full_us;                                    /* one solid fill of the whole screen: the display path alone */
    uint32_t mv_px_avg;                                       /* mean pixels sent by one 2048 swipe */
    uint32_t mv10_avg_us, mv10_px_avg;                        /* the same for the first 10 swipes that drew (a young board) */
    uint32_t flag_us;                                         /* one long press: toggle a flag */
    uint32_t touch_read_us;                                   /* one I2C read of the touch chip, mean of 16 */
    uint32_t touch_fail;                                      /* touch reads that failed */
    uint32_t polls, touches;                                  /* poll count, polls that saw a finger */
    uint32_t lat_last_us, lat_max_us, lat_n;                  /* real touches: sample read to pixels done */
    uint32_t stack_used_b;                                    /* deepest stack use seen after the measurement and during play (0 if the hal cannot tell) */
} tg_stats_t;
volatile tg_stats_t tg_stats;

static void put_u(const char *k, uint32_t v)
{
    char s[12]; hal_log(k); gfx_utoa(v, s); hal_log(s);
}
static void log_stats(void)
{
    hal_log("\r\nBENCH " BUILD_NAME);
    put_u(" full_menu_us=", tg_stats.full_menu_us); put_u(" full_2048_us=", tg_stats.full_2048_us);
    put_u(" full_mines_us=", tg_stats.full_mines_us);
    put_u("\r\nBENCH mv2048_n=", tg_stats.mv2048_n); put_u(" avg_us=", tg_stats.mv2048_avg_us); put_u(" max_us=", tg_stats.mv2048_max_us);
    put_u("\r\nBENCH dig_n=", tg_stats.dig_n); put_u(" avg_us=", tg_stats.dig_avg_us); put_u(" max_us=", tg_stats.dig_max_us);
    put_u(" one_cell_n=", tg_stats.dig1_n); put_u(" one_cell_us=", tg_stats.dig1_us);
    put_u("\r\nBENCH first_dig_n=", tg_stats.first_n); put_u(" avg_us=", tg_stats.first_avg_us); put_u(" max_us=", tg_stats.first_max_us);
    put_u(" flag_us=", tg_stats.flag_us); put_u(" fill_full_us=", tg_stats.fill_full_us); put_u(" mv_px_avg=", tg_stats.mv_px_avg);
    put_u("\r\nBENCH first10_swipes_avg_us=", tg_stats.mv10_avg_us); put_u(" px=", tg_stats.mv10_px_avg);
    put_u("\r\nBENCH stack_used_b=", tg_stats.stack_used_b); put_u(" touch_read_us=", tg_stats.touch_read_us); put_u(" touch_fail=", tg_stats.touch_fail);
    hal_log("\r\n");
}

/* ---------- launcher ---------- */
static const button_t B_2048  = {40, 150, 240, 90, "2048"};
static const button_t B_MINES = {40, 270, 240, 90, "MINES"};
static const game_t *cur;       /* 0 = launcher */

static void menu_draw(void)
{
    gfx_fill(0, 0, LCD_W, LCD_H, COL_BG);
    gfx_text_center(0, 40, LCD_W, "TOUCH GAMES", 4, COL_INK, COL_BG);
    gfx_text_center(0, 88, LCD_W, "PICK ONE", 2, RGB(187, 173, 160), COL_BG);
    button_draw(&B_2048, RGB(237, 194, 46), RGB(255, 255, 255));
    button_draw(&B_MINES, RGB(143, 122, 102), RGB(255, 255, 255));
    gfx_text_center(0, 400, LCD_W, "SWIPE  TAP  HOLD", 2, RGB(187, 173, 160), COL_BG);
}

static void go(const game_t *g, uint32_t seed)
{
    cur = g;
    if (g) g->start(seed); else menu_draw();
}

static void dispatch(const gest_evt_t *e)
{
    if (!cur) {
        if (e->kind != G_TAP) return;
        if (button_hit(&B_2048, e->x, e->y)) go(&game_2048, hal_us());
        else if (button_hit(&B_MINES, e->x, e->y)) go(&game_mines, hal_us());
    } else if (cur->event(e)) go(0, 0);
}

/* ---------- the scripted measurement: gestures injected into the same dispatch path a finger uses ---------- */
static uint32_t last_px;
static uint32_t timed_event(gest_kind_t k, int x, int y)
{
    gest_evt_t e = { k, (int16_t)x, (int16_t)y, 0 };
    uint32_t p0 = gfx_px_sent, t0 = hal_us(); dispatch(&e);
    uint32_t d = hal_us() - t0; last_px = gfx_px_sent - p0; return d;
}

static void bench(void)
{
    uint32_t t;
    t = hal_us(); gfx_fill(0, 0, LCD_W, LCD_H, 0x0000); tg_stats.fill_full_us = hal_us() - t;
    t = hal_us(); go(0, 0); tg_stats.full_menu_us = hal_us() - t;
    tg_stats.magic = 0x7C6A0001;
    /* 2048: full redraw, then 60 swipes cycling left, up, right, down on a fixed seed (the game may end early) */
    t = hal_us(); go(&game_2048, 12345); tg_stats.full_2048_us = hal_us() - t;
    static const gest_kind_t seq[4] = { G_SWIPE_L, G_SWIPE_U, G_SWIPE_R, G_SWIPE_D };
    uint32_t sum = 0, mx = 0, n = 0, px = 0, s10 = 0, p10 = 0;
    for (int i = 0; i < 60; i++) {
        uint32_t d = timed_event(seq[i & 3], 160, 240);
        if (last_px) { sum += d; px += last_px; n++; if (d > mx) mx = d; if (n <= 10) { s10 += d; p10 += last_px; } }     /* a swipe that moved nothing draws nothing: left out */
    }
    tg_stats.mv2048_n = n; tg_stats.mv2048_avg_us = n ? sum / n : 0; tg_stats.mv2048_max_us = mx; tg_stats.mv_px_avg = n ? px / n : 0;
    tg_stats.mv10_avg_us = s10 / 10; tg_stats.mv10_px_avg = p10 / 10;
    tg_stats.magic = 0x7C6A0002;
    /* Minesweeper: full redraw, then eight boards: first dig in the middle, then 24 taps on a fixed list of cells */
    t = hal_us(); go(&game_mines, 777); tg_stats.full_mines_us = hal_us() - t;
    uint32_t dsum = 0, dn = 0, dmx = 0, s1 = 0, n1 = 0, fsum = 0, fn = 0, fmx = 0;
    for (uint32_t seed = 1; seed <= 8; seed++) {
        go(&game_mines, seed * 2654435761u);
        for (int k = 0; k < 25; k++) {
            int cell = k == 0 ? 40 : (k * 37 + (int)seed * 11) % 81;
            int c = cell % 9, r = cell / 9;
            uint32_t d = timed_event(G_TAP, 7 + c * 34 + 17, 96 + r * 34 + 17);
            if (!last_px) continue;
            dsum += d; dn++; if (d > dmx) dmx = d;
            if (last_px == 34 * 34) { s1 += d; n1++; }
            if (k == 0) { fsum += d; fn++; if (d > fmx) fmx = d; }
        }
    }
    tg_stats.dig_n = dn; tg_stats.dig_avg_us = dn ? dsum / dn : 0; tg_stats.dig_max_us = dmx;
    tg_stats.dig1_n = n1; tg_stats.dig1_us = n1 ? s1 / n1 : 0;
    tg_stats.first_n = fn; tg_stats.first_avg_us = fn ? fsum / fn : 0; tg_stats.first_max_us = fmx;
    go(&game_mines, 778);                                        /* fresh board: every cell closed, so a long press plants a flag */
    tg_stats.flag_us = timed_event(G_LONG, 7 + 17, 96 + 17);
    /* touch chip read time, mean of 16 */
    uint32_t tt = hal_us(); int x, y;
    for (int i = 0; i < 16; i++) if (hal_touch(&x, &y) < 0) tg_stats.touch_fail++;
    tg_stats.touch_read_us = (hal_us() - tt) / 16;
    tg_stats.magic = 0x7C6A0003;
    tg_stats.stack_used_b = hal_stack_used();
    log_stats();
}

int main_app(void)
{
    hal_init();
    hal_log("\r\n" BUILD_NAME ": starting\r\n");
#if BENCH
    bench();
#endif
    tg_stats.magic = 0x7C6A0010;
    go(BOOT_GAME == 1 ? &game_2048 : BOOT_GAME == 2 ? &game_mines : 0, hal_us());
    gesture_t g; gesture_init(&g);
    uint32_t next = hal_ms();
    for (;;) {
        uint32_t now = hal_ms();
        if (cur) cur->tick(now);
        if ((int32_t)(now - next) >= 0) {
            next = now + POLL_MS;
            int x = 0, y = 0, d = hal_touch(&x, &y);
            uint32_t t_sample = hal_us();
            tg_stats.polls++;
            if (d < 0) { tg_stats.touch_fail++; d = 0; x = g.x0; y = g.y0; if (g.down) d = 1; }   /* keep the last state on a failed read */
            if (d > 0) tg_stats.touches++;
            gest_evt_t e;
            if (gesture_feed(&g, d, x, y, now, &e)) {
                dispatch(&e);
                uint32_t lat = hal_us() - t_sample;
                tg_stats.lat_last_us = lat; tg_stats.lat_n++;
                if (lat > tg_stats.lat_max_us) tg_stats.lat_max_us = lat;
            }
        }
        if (!(tg_stats.polls & 255)) tg_stats.stack_used_b = hal_stack_used();
        hal_idle();
    }
}
