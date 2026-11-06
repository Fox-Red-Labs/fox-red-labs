/* Minesweeper, 9 x 9 with 10 mines. Tap digs, long press plants or lifts a flag, tap on a number whose flags match it digs
 * the rest of its neighbours. The first dig is always safe. Written fresh from the game's public rules.
 * One byte per cell: bits 0-3 neighbour mine count, 4 mine, 5 revealed, 6 flagged. */
#include "game.h"
#include "gfx.h"
#include "hal.h"

#define N 9
#define MINES 10
#define CS 34                 /* cell size, 34 x 34 = 1156 pixels fits the scratch buffer */
#define OX 7
#define OY 96
#define F_MINE 0x10
#define F_REV  0x20
#define F_FLAG 0x40

typedef struct {
    uint8_t c[N * N];
    uint32_t rng, t_start_ms, shown_secs;
    uint8_t placed, over, won, flags, revealed, boom;
    int8_t shown_flags;
} mines_t;
static mines_t M;

static const button_t B_NEW  = {12, 424, 140, 44, "NEW"};
static const button_t B_MENU = {168, 424, 140, 44, "MENU"};

static uint32_t rnd(void) { M.rng ^= M.rng << 13; M.rng ^= M.rng >> 17; M.rng ^= M.rng << 5; return M.rng; }

/* ---------- rules ---------- */
static void place(int safe)
{
    int n = 0;
    while (n < MINES) {
        int i = (int)(rnd() % (N * N));
        if (i == safe || (M.c[i] & F_MINE)) continue;
        M.c[i] |= F_MINE; n++;
    }
    for (int i = 0; i < N * N; i++) {
        if (M.c[i] & F_MINE) continue;
        int r = i / N, c = i % N, k = 0;
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
            int rr = r + dr, cc = c + dc;
            if ((dr || dc) && rr >= 0 && rr < N && cc >= 0 && cc < N && (M.c[rr * N + cc] & F_MINE)) k++;
        }
        M.c[i] = (uint8_t)((M.c[i] & 0xF0) | k);
    }
    M.placed = 1;
}

static void draw_cell(int i);
static int g_draw;     /* 1: draw cells as they change (set by event handlers); 0 for host tests */

/* Flood reveal from cell i, iterative with a small queue. Returns 1 if a mine was dug. */
static int reveal(int i)
{
    uint8_t q[N * N]; int qh = 0, qt = 0;
    if (M.c[i] & (F_REV | F_FLAG)) return 0;
    if (M.c[i] & F_MINE) { M.c[i] |= F_REV; M.boom = (uint8_t)i; return 1; }
    M.c[i] |= F_REV; M.revealed++; q[qt++] = (uint8_t)i;
    while (qh < qt) {
        int j = q[qh++]; if (g_draw) draw_cell(j);
        if ((M.c[j] & 0x0F) != 0) continue;
        int r = j / N, c = j % N;
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
            int rr = r + dr, cc = c + dc, k = rr * N + cc;
            if (rr < 0 || rr >= N || cc < 0 || cc >= N) continue;
            if (M.c[k] & (F_REV | F_FLAG | F_MINE)) continue;
            M.c[k] |= F_REV; M.revealed++; q[qt++] = (uint8_t)k;
        }
    }
    return 0;
}

static int flags_around(int i)
{
    int r = i / N, c = i % N, k = 0;
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        int rr = r + dr, cc = c + dc;
        if ((dr || dc) && rr >= 0 && rr < N && cc >= 0 && cc < N && (M.c[rr * N + cc] & F_FLAG)) k++;
    }
    return k;
}

/* ---------- drawing ---------- */
static const uint16_t NUMCOL[9] = {0, RGB(0, 0, 255), RGB(0, 128, 0), RGB(255, 0, 0), RGB(0, 0, 128),
                                   RGB(128, 0, 0), RGB(0, 128, 128), RGB(0, 0, 0), RGB(100, 100, 100)};

static void draw_cell(int i)
{
    uint16_t *b = gfx_buf; uint8_t v = M.c[i];
    int rev = v & F_REV, mine = v & F_MINE, flag = v & F_FLAG;
    uint16_t face = RGB(192, 192, 192), hi = RGB(255, 255, 255), lo = RGB(110, 110, 110);
    int lost = M.over && !M.won;
    if (lost && mine && !flag) rev = 1;                  /* show the mines after a loss */
    if (rev) face = (lost && i == M.boom) ? RGB(255, 60, 40) : RGB(205, 205, 205);
    for (int y = 0; y < CS; y++) for (int x = 0; x < CS; x++) {
        uint16_t p = face;
        if (rev) { if (x == 0 || y == 0) p = lo; }
        else {
            if (x < 3 || y < 3) p = hi;
            if (x >= CS - 3 || y >= CS - 3) p = lo;
            if ((x < 3 && y >= CS - 3 - (3 - x)) || (y < 3 && x >= CS - 3 - (3 - y))) p = face;
        }
        b[y * CS + x] = p;
    }
    if (rev && mine) {                                    /* mine: disc with a cross of spikes */
        for (int y = 0; y < CS; y++) for (int x = 0; x < CS; x++) {
            int dx = x - CS / 2, dy = y - CS / 2, d2 = dx * dx + dy * dy;
            int spike = (dx == 0 || dy == 0) ? 12 : (dx == dy || dx == -dy) ? 10 : 0;
            if (d2 <= 7 * 7 || (spike && d2 <= spike * spike)) b[y * CS + x] = RGB(0, 0, 0);
            if (dx >= -4 && dx <= -2 && dy >= -4 && dy <= -2) b[y * CS + x] = RGB(255, 255, 255);
        }
    } else if (rev && (v & 0x0F)) {
        gfx_buf_glyph(b, CS, CS, (CS - 15) / 2, (CS - 21) / 2, (char)('0' + (v & 0x0F)), 3, NUMCOL[v & 0x0F]);
    } else if (!rev && flag) {                            /* flag: red pennant on a black pole and base */
        for (int y = 6; y <= 18; y++) {                   /* pennant: a triangle pointing left of the pole */
            int d = y > 12 ? y - 12 : 12 - y, xl = 8 + d * 3 / 2;
            for (int x = xl; x <= 16; x++) b[y * CS + x] = RGB(230, 20, 20);
        }
        for (int y = 6; y <= 25; y++) { b[y * CS + 17] = RGB(0, 0, 0); b[y * CS + 18] = RGB(0, 0, 0); }
        for (int y = 22; y <= 27; y++) for (int x = (y < 24 ? 13 : 10); x <= (y < 24 ? 22 : 25); x++) b[y * CS + x] = RGB(0, 0, 0);
        if (lost && !mine) {                              /* wrong flag: red cross */
            for (int k = 4; k < CS - 4; k++) { b[k * CS + k] = RGB(255, 0, 0); b[k * CS + (CS - 1 - k)] = RGB(255, 0, 0); }
        }
    }
    gfx_blit(OX + (i % N) * CS, OY + (i / N) * CS, CS, CS, b);
}

static void draw_counter(int x, int v)
{
    char s[8]; int neg = v < 0; if (neg) v = -v;
    char t[8]; gfx_utoa((uint32_t)v, t);
    int k = 0; if (neg) s[k++] = '-';
    int n = 0; while (t[n]) n++;
    for (int pad = n; pad < 2; pad++) s[k++] = '0';
    for (int j = 0; j < n; j++) s[k++] = t[j];
    s[k] = 0;
    gfx_fill(x, 8, 84, 52, RGB(30, 30, 30));
    gfx_text_center(x, 20, 84, s, 4, RGB(255, 60, 40), RGB(30, 30, 30));
}

static void draw_status(void)
{
    const char *m = M.won ? "YOU WIN!" : M.over ? "BOOM" : "TAP DIG  HOLD FLAG";
    uint16_t ink = M.won ? RGB(0, 150, 60) : M.over ? RGB(230, 40, 30) : COL_INK;
    gfx_text_center(0, 68, LCD_W, m, 2, ink, COL_BG);
}

static void draw_header(void)
{
    draw_counter(10, MINES - M.flags); M.shown_flags = (int8_t)M.flags;
    char s[8]; gfx_utoa(M.shown_secs > 999 ? 999 : M.shown_secs, s);
    gfx_fill(226, 8, 84, 52, RGB(30, 30, 30));
    gfx_text_center(226, 20, 84, s, 4, RGB(255, 60, 40), RGB(30, 30, 30));
    gfx_text_center(100, 22, 120, "MINES", 3, COL_INK, COL_BG);
}

static void draw_all(void)
{
    g_draw = 1;
    gfx_fill(0, 0, LCD_W, LCD_H, COL_BG);
    draw_header(); draw_status();
    for (int i = 0; i < N * N; i++) draw_cell(i);
    button_draw(&B_NEW, RGB(143, 122, 102), RGB(249, 246, 242));
    button_draw(&B_MENU, RGB(187, 173, 160), RGB(249, 246, 242));
}

/* ---------- game interface ---------- */
static void start(uint32_t seed)
{
    for (unsigned i = 0; i < sizeof M; i++) ((uint8_t *)&M)[i] = 0;
    M.rng = seed ? seed : 88172645u;
    draw_all();
}

static void finish(int won)
{
    M.over = 1; M.won = (uint8_t)won;
    if (won) for (int i = 0; i < N * N; i++) if ((M.c[i] & F_MINE) && !(M.c[i] & F_FLAG)) M.c[i] |= F_FLAG;
    M.flags = 0; for (int i = 0; i < N * N; i++) if (M.c[i] & F_FLAG) M.flags++;
    g_draw = 1;
    for (int i = 0; i < N * N; i++) if ((M.c[i] & (F_MINE | F_FLAG))) draw_cell(i);
    draw_header(); draw_status();
}

static int event(const gest_evt_t *e)
{
    if (e->kind == G_TAP) {
        if (button_hit(&B_MENU, e->x, e->y)) return 1;
        if (button_hit(&B_NEW, e->x, e->y)) { start(M.rng ^ hal_us()); return 0; }
    }
    if (M.over) return 0;
    int cx = (e->x - OX) / CS, cy = (e->y - OY) / CS;
    if (e->x < OX || e->y < OY || cx >= N || cy >= N) return 0;
    int i = cy * N + cx;
    g_draw = 1;
    if (e->kind == G_TAP) {
        if (M.c[i] & F_FLAG) return 0;
        if (!M.placed) { place(i); M.t_start_ms = hal_ms(); }
        if (M.c[i] & F_REV) {                         /* chord: dig around a number whose flags match it */
            if ((M.c[i] & 0x0F) && flags_around(i) == (M.c[i] & 0x0F)) {
                int r = i / N, c = i % N, hit = 0;
                for (int dr = -1; dr <= 1 && !hit; dr++) for (int dc = -1; dc <= 1 && !hit; dc++) {
                    int rr = r + dr, cc = c + dc;
                    if ((dr || dc) && rr >= 0 && rr < N && cc >= 0 && cc < N) hit = reveal(rr * N + cc);
                }
                if (hit) { finish(0); return 0; }
            } else return 0;
        } else if (reveal(i)) { finish(0); return 0; }
        if (M.revealed == N * N - MINES) finish(1);
    } else if (e->kind == G_LONG) {
        if (M.c[i] & F_REV) return 0;
        M.c[i] ^= F_FLAG; M.flags = (uint8_t)(M.flags + ((M.c[i] & F_FLAG) ? 1 : -1));
        draw_cell(i);
        draw_counter(10, MINES - M.flags);
    }
    return 0;
}

static void tick(uint32_t now)
{
    if (!M.placed || M.over) return;
    uint32_t secs = (now - M.t_start_ms) / 1000;
    if (secs != M.shown_secs) {
        M.shown_secs = secs;
        char s[8]; gfx_utoa(secs > 999 ? 999 : secs, s);
        gfx_text_center(226, 20, 84, s, 4, RGB(255, 60, 40), RGB(30, 30, 30));
    }
}

const game_t game_mines = { "MINES", start, draw_all, event, tick };
