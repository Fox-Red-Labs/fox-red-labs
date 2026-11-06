/* 2048: swipe to slide the tiles; equal tiles merge. Written fresh from the game's public rules.
 * Tiles hold exponents (1 = 2, 2 = 4 ...). Only tiles whose value changed are redrawn after a move. */
#include "game.h"
#include "gfx.h"
#include "hal.h"

#define T 70            /* tile size */
#define GAP 4
#define BX 12           /* board origin */
#define BY 108
#define BSZ (4 * T + 5 * GAP)

typedef struct {
    uint8_t  t[16], shown[16];
    uint32_t score, best, shown_score, shown_best, rng;
    uint8_t  over, won, msg;     /* msg: 0 none, 1 win, 2 over (what the status line shows) */
} g2048_t;
static g2048_t G;

static const button_t B_NEW  = {12, 424, 140, 44, "NEW"};
static const button_t B_MENU = {168, 424, 140, 44, "MENU"};

/* ---------- rules (no drawing in this block) ---------- */
static uint32_t rnd(void) { G.rng ^= G.rng << 13; G.rng ^= G.rng >> 17; G.rng ^= G.rng << 5; return G.rng; }

static int spawn(void)
{
    uint8_t empty[16]; int n = 0;
    for (int i = 0; i < 16; i++) if (!G.t[i]) empty[n++] = (uint8_t)i;
    if (!n) return 0;
    G.t[empty[rnd() % n]] = (rnd() % 10) ? 1 : 2;
    return 1;
}

static int idx(int dir, int line, int k)       /* dir 0 left, 1 right, 2 up, 3 down; k = 0 is the edge tiles move toward */
{
    switch (dir) {
    case 0: return line * 4 + k;
    case 1: return line * 4 + 3 - k;
    case 2: return k * 4 + line;
    default: return (3 - k) * 4 + line;
    }
}

static int can_move(void)
{
    for (int i = 0; i < 16; i++) {
        if (!G.t[i]) return 1;
        if ((i & 3) < 3 && G.t[i] == G.t[i + 1]) return 1;
        if (i < 12 && G.t[i] == G.t[i + 4]) return 1;
    }
    return 0;
}

/* Returns 1 if anything moved. Adds merged values to the score. */
static int slide(int dir)
{
    int moved = 0;
    for (int line = 0; line < 4; line++) {
        uint8_t out[4] = {0, 0, 0, 0}; int o = 0, last_merged = 0;
        for (int k = 0; k < 4; k++) {
            uint8_t v = G.t[idx(dir, line, k)];
            if (!v) continue;
            if (o && out[o - 1] == v && !last_merged) { out[o - 1]++; G.score += 1u << out[o - 1]; last_merged = 1; if (out[o - 1] == 11) G.won = 1; }
            else { out[o++] = v; last_merged = 0; }
        }
        for (int k = 0; k < 4; k++) {
            int i = idx(dir, line, k);
            if (G.t[i] != out[k]) { G.t[i] = out[k]; moved = 1; }
        }
    }
    return moved;
}

/* ---------- drawing ---------- */
static uint16_t tile_color(int v)
{
    static const uint16_t c[12] = {
        RGB(205, 193, 180), RGB(238, 228, 218), RGB(237, 224, 200), RGB(242, 177, 121), RGB(245, 149, 99),
        RGB(246, 124, 95), RGB(246, 94, 59), RGB(237, 207, 114), RGB(237, 204, 97), RGB(237, 200, 80),
        RGB(237, 197, 63), RGB(237, 194, 46) };
    return v < 12 ? c[v] : RGB(60, 58, 50);
}

static void draw_tile(int i)
{
    int x = BX + GAP + (i & 3) * (T + GAP), y = BY + GAP + (i >> 2) * (T + GAP), v = G.t[i];
    uint16_t bg = tile_color(v);
    gfx_fill(x, y, T, T, bg);
    if (v) {
        char s[8]; gfx_utoa(1u << v, s);
        int digits = 0; while (s[digits]) digits++;
        int scale = digits == 1 ? 5 : digits == 2 ? 4 : digits == 3 ? 3 : 2;
        uint16_t ink = v <= 2 ? COL_INK : RGB(249, 246, 242);
        int tw = gfx_text_w(s, scale);
        gfx_text(x + (T - tw) / 2, y + (T - 7 * scale) / 2, s, scale, ink, bg);
    }
    G.shown[i] = (uint8_t)v;
}

static void draw_box(int x, const char *label, uint32_t v)
{
    uint16_t bg = RGB(187, 173, 160);
    gfx_fill(x, 8, 76, 54, bg);
    gfx_text_center(x, 14, 76, label, 2, RGB(238, 228, 218), bg);
    char s[12]; gfx_utoa(v, s);
    int n = 0; while (s[n]) n++;
    gfx_text_center(x, 34, 76, s, n <= 4 ? 3 : 2, RGB(255, 255, 255), bg);
}

static void draw_status(void)
{
    const char *m = G.over ? "GAME OVER - TAP NEW" : G.won ? "2048! KEEP GOING" : "SWIPE TO MOVE TILES";
    uint16_t ink = G.over ? RGB(246, 94, 59) : COL_INK;
    gfx_text_center(0, 76, LCD_W, m, 2, ink, COL_BG);
    G.msg = G.over ? 2 : G.won ? 1 : 0;
}

static void draw_scores(int all)
{
    if (all || G.shown_score != G.score) { draw_box(150, "SCORE", G.score); G.shown_score = G.score; }
    if (all || G.shown_best != G.best)   { draw_box(234, "BEST", G.best);   G.shown_best = G.best; }
}

static void draw_all(void)
{
    gfx_fill(0, 0, LCD_W, LCD_H, COL_BG);
    gfx_text(12, 22, "2048", 5, COL_INK, COL_BG);
    draw_scores(1);
    draw_status();
    gfx_fill(BX, BY, BSZ, BSZ, RGB(187, 173, 160));
    for (int i = 0; i < 16; i++) draw_tile(i);
    button_draw(&B_NEW, RGB(143, 122, 102), RGB(249, 246, 242));
    button_draw(&B_MENU, RGB(187, 173, 160), RGB(249, 246, 242));
}

static void draw_changes(void)
{
    for (int i = 0; i < 16; i++) if (G.shown[i] != G.t[i]) draw_tile(i);
    draw_scores(0);
    if ((G.over ? 2 : G.won ? 1 : 0) != G.msg) draw_status();
}

/* ---------- game interface ---------- */
static void start(uint32_t seed)
{
    uint32_t best = G.best;
    for (unsigned i = 0; i < sizeof G; i++) ((uint8_t *)&G)[i] = 0;
    G.best = best;
    G.rng = seed ? seed : 2463534242u;
    spawn(); spawn();
    draw_all();
}

static int event(const gest_evt_t *e)
{
    int dir = -1;
    switch (e->kind) {
    case G_SWIPE_L: dir = 0; break;
    case G_SWIPE_R: dir = 1; break;
    case G_SWIPE_U: dir = 2; break;
    case G_SWIPE_D: dir = 3; break;
    case G_TAP:
        if (button_hit(&B_MENU, e->x, e->y)) return 1;
        if (button_hit(&B_NEW, e->x, e->y)) start(G.rng ^ hal_us());
        return 0;
    default: return 0;
    }
    if (G.over) return 0;
    if (slide(dir)) {
        spawn();
        if (G.score > G.best) G.best = G.score;
        if (!can_move()) G.over = 1;
        draw_changes();
    }
    return 0;
}

static void tick(uint32_t now) { (void)now; }

const game_t game_2048 = { "2048", start, draw_all, event, tick };
