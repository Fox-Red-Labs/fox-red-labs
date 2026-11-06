/* Host tests: gesture recognizer, 2048 and minesweeper behaviour, and pictures of the screens.
 *   ./host/run_host.sh   builds with the host C compiler, runs the checks, writes proof/ pictures */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/game.h"
#include "../src/gfx.h"

extern uint16_t host_fb[];
extern long host_px_sent;
extern int host_tx, host_ty, host_tdown;
void host_save_ppm(const char *path);

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static gest_kind_t feed_path(const int (*p)[3], int n, gest_evt_t *last)
{
    gesture_t g; gesture_init(&g); gest_kind_t k = G_NONE; gest_evt_t e;
    for (int i = 0; i < n; i++) if (gesture_feed(&g, p[i][0] != 0 ? 1 : 0, p[i][1], p[i][2], (uint32_t)i * 10, &e)) { k = e.kind; if (last) *last = e; }
    return k;
}

static void test_gesture(void)
{
    gest_evt_t e;
    { int p[][3] = {{1,100,100},{1,102,101},{1,101,100},{0,0,0}}; CHECK(feed_path(p, 4, &e) == G_TAP); CHECK(e.x == 100 && e.y == 100); }
    { int p[][3] = {{1,100,100},{1,100,100},{0,0,0}}; CHECK(feed_path(p, 3, 0) == G_TAP); }
    { int p[][3] = {{1,100,100},{1,130,102},{1,150,104},{1,160,104},{0,0,0}}; CHECK(feed_path(p, 5, 0) == G_SWIPE_R); }
    { int p[][3] = {{1,200,100},{1,170,102},{1,150,104},{0,0,0}}; CHECK(feed_path(p, 4, 0) == G_SWIPE_L); }
    { int p[][3] = {{1,100,300},{1,102,270},{1,103,250},{0,0,0}}; CHECK(feed_path(p, 4, 0) == G_SWIPE_U); }
    { int p[][3] = {{1,100,100},{1,101,130},{1,103,160},{0,0,0}}; CHECK(feed_path(p, 4, 0) == G_SWIPE_D); }
    { int p[][3] = {{1,100,100},{1,120,130},{1,130,100},{0,0,0}}; CHECK(feed_path(p, 4, 0) == G_NONE); }     /* wandering drag: neither */
    { int p[60][3]; for (int i = 0; i < 60; i++) { p[i][0] = 1; p[i][1] = 50; p[i][2] = 60; } p[59][0] = 0;
      gesture_t g; gesture_init(&g); int longs = 0, taps = 0; gest_evt_t ev;
      for (int i = 0; i < 60; i++) if (gesture_feed(&g, p[i][0], 50, 60, (uint32_t)i * 10, &ev)) { if (ev.kind == G_LONG) longs++; if (ev.kind == G_TAP) taps++; }
      CHECK(longs == 1 && taps == 0); }                                                                       /* a long hold fires once, the lift gives no tap */
    { int p[][3] = {{1,100,100},{1,140,102},{1,180,102},{1,230,104},{0,0,0}}; gesture_t g; gesture_init(&g); int sw = 0; gest_evt_t ev;
      for (int i = 0; i < 5; i++) if (gesture_feed(&g, p[i][0], p[i][1], p[i][2], (uint32_t)i * 10, &ev)) sw++;
      CHECK(sw == 1); }                                                                                       /* a long swipe fires once */
}

/* read the 2048 tile exponents back from the screen is not possible; check the rules through the public path instead */
static void swipe(const game_t *g, gest_kind_t k) { gest_evt_t e = { k, 160, 240, 0 }; g->event(&e); }

static int count_pixels(uint16_t c) { int n = 0; for (int i = 0; i < LCD_W * LCD_H; i++) n += host_fb[i] == c; return n; }

static void test_2048(void)
{
    game_2048.start(42);
    /* two tiles start; swiping the four ways repeatedly must never crash, and screen pixels stay in range */
    long before = host_px_sent;
    for (int i = 0; i < 400; i++) { swipe(&game_2048, (gest_kind_t)(G_SWIPE_L + (i % 4))); }
    CHECK(host_px_sent > before);                    /* tiles were redrawn */
    CHECK(count_pixels(RGB(250, 248, 239)) > 1000);   /* the page background is still there */
    /* a changed-tile redraw is far less than a full screen */
    game_2048.start(7);
    long b = host_px_sent; swipe(&game_2048, G_SWIPE_L);
    long moved_px = host_px_sent - b;
    printf("2048: one move sent %ld pixels (a full screen is %d)\n", moved_px, LCD_W * LCD_H);
    CHECK(moved_px < LCD_W * LCD_H / 2);
    /* the boot measurement's script (seed 12345, left up right down, 60 swipes): pixels per swipe that drew anything */
    game_2048.start(12345);
    long px = 0; int n = 0;
    for (int i = 0; i < 60; i++) { long q = host_px_sent; swipe(&game_2048, (gest_kind_t)(G_SWIPE_L + ((i & 3) == 0 ? 0 : (i & 3) == 1 ? 2 : (i & 3) == 2 ? 1 : 3))); if (host_px_sent > q) { px += host_px_sent - q; n++; } }
    printf("2048: the measurement script draws %d of 60 swipes, %ld pixels each on average\n", n, n ? px / n : 0);
}

static void tap(const game_t *g, int x, int y) { gest_evt_t e = { G_TAP, (int16_t)x, (int16_t)y, 0 }; g->event(&e); }
static void hold(const game_t *g, int x, int y) { gest_evt_t e = { G_LONG, (int16_t)x, (int16_t)y, 0 }; g->event(&e); }
static void cell_xy(int c, int r, int *x, int *y) { *x = 7 + c * 34 + 17; *y = 96 + r * 34 + 17; }

static void test_mines(void)
{
    /* the first dig is never a mine, over many seeds */
    for (uint32_t s = 1; s <= 200; s++) {
        game_mines.start(s * 2654435761u);
        int x, y; cell_xy(4, 4, &x, &y); tap(&game_mines, x, y);
        /* center pixel of the dug cell is not the mine-black spike colour at its middle */
        CHECK(host_fb[(y) * LCD_W + x] != RGB(255, 60, 40));
    }
    game_mines.start(5);
    int x, y; cell_xy(0, 0, &x, &y); hold(&game_mines, x, y);
    CHECK(count_pixels(RGB(230, 20, 20)) > 20);       /* a red flag was drawn */
    hold(&game_mines, x, y);
    CHECK(count_pixels(RGB(230, 20, 20)) == 0);       /* lifting it removes it */
}

static int ppm_png(const char *name)
{
    char p[160], cmd[400]; snprintf(p, sizeof p, "proof/%s.ppm", name);
    host_save_ppm(p);
    snprintf(cmd, sizeof cmd, "python3 -c \"from PIL import Image; Image.open('%s').save('proof/%s.png'); import os; os.remove('%s')\"", p, name, p);
    return system(cmd);
}

static void shots(void)
{
    system("mkdir -p proof");
    gest_evt_t e;
    game_2048.start(99);
    for (int i = 0; i < 90; i++) swipe(&game_2048, (gest_kind_t)(G_SWIPE_L + ((i * 7 + i / 3) % 4)));
    ppm_png("2048");
    game_mines.start(31);
    int x, y; cell_xy(4, 4, &x, &y); tap(&game_mines, x, y);
    cell_xy(0, 8, &x, &y); hold(&game_mines, x, y);
    cell_xy(8, 0, &x, &y); hold(&game_mines, x, y);
    ppm_png("mines");
    (void)e;
}

int main(void)
{
    extern void hal_init(void); hal_init();
    test_gesture(); test_2048(); test_mines(); shots();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
