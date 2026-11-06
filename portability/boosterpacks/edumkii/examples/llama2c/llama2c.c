/*
 * Language model on the EDUMKII screen (LP-MSPM33C321A at 160 MHz): TinyStories 260K int8 (ports/llama2c) writes a story,
 * each token's text drawn as it is produced with word wrap and scrolling, a header line with tokens per second.
 * A new story starts when S2 is pressed or 30 s after the last one began; the prompt and seed change every story.
 * Results sit in the RAM struct `ls` for the probe. BSD-3-Clause.
 */
#include <string.h>
#include "ti_msp_dl_config.h"
#include "board_config.h"
#include "edumkii.h"
#include "edu_board.h"
#include "llama2q.h"

extern const unsigned char ll_model_blob[], ll_tok_blob[];

#define COLS 21              /* 128 / 6 */
#define ROWS 15              /* text rows below the header, 8 px each */
#define STORY_MS 30000u

volatile struct {
    uint32_t magic;          /* 0x4C534331 once running */
    uint32_t stories;        /* finished stories */
    uint32_t tokens;         /* tokens of the last finished story */
    uint32_t gen_cycles;     /* whole ll_generate call, display included */
    uint32_t disp_cycles;    /* cycles spent drawing inside it */
    uint32_t tps_x10;        /* tokens per second with the display on, tenths */
    uint32_t tps_nodisp_x10; /* the same without the drawing time */
    uint32_t scrolls;
    uint32_t s2_presses;
    uint32_t cpu_hz;
} ls;

static edumkii_t d;
static char lines[ROWS][COLS + 1];
static int nl;                 /* index of the line being written */
static char word[COLS + 1];
static int wl;
static uint32_t disp_acc;
static int s2_latched, s2_prev;
static int tok_in_story;
static uint32_t story_t0;

static const char *prompts[] = { "Once upon a time", "One day", "There was a little", "The big red", "Tom and Sue", "In the park" };
#define NPROMPT (int)(sizeof prompts / sizeof prompts[0])

static void draw_line(int i)
{
    char b[COLS + 1]; int n = (int)strlen(lines[i]);
    memcpy(b, lines[i], (size_t)n);
    for (int k = n; k < COLS; k++) b[k] = ' ';
    b[COLS] = 0;
    edumkii_text(&d, 1, 10 + i * 8, b, EDUMKII_WHITE, EDUMKII_BLACK, 1);
}
static void draw_all(void) { for (int i = 0; i < ROWS; i++) draw_line(i); }

static void header(uint32_t tps_x10, int story)
{
    int x = edumkii_text(&d, 1, 1, "TinyStories ", EDUMKII_YELLOW, EDUMKII_BLACK, 1);
    x = edumkii_number(&d, x, 1, (int32_t)(tps_x10 / 10u), 2, EDUMKII_CYAN, EDUMKII_BLACK, 1);
    x = edumkii_text(&d, x, 1, ".", EDUMKII_CYAN, EDUMKII_BLACK, 1);
    x = edumkii_number(&d, x, 1, (int32_t)(tps_x10 % 10u), 1, EDUMKII_CYAN, EDUMKII_BLACK, 1);
    x = edumkii_text(&d, x, 1, " t/s #", EDUMKII_YELLOW, EDUMKII_BLACK, 1);
    edumkii_number(&d, x, 1, story, 2, EDUMKII_YELLOW, EDUMKII_BLACK, 1);
}

static void new_line(void)
{
    if (nl + 1 < ROWS) { nl++; lines[nl][0] = 0; return; }
    memmove(lines[0], lines[1], (size_t)(ROWS - 1) * (COLS + 1));
    lines[ROWS - 1][0] = 0; ls.scrolls++;
}
static void commit_word(int add_space)
{
    int n = (int)strlen(lines[nl]);
    if (wl) {
        if (n + wl > COLS) { new_line(); n = 0; }
        memcpy(lines[nl] + n, word, (size_t)wl); n += wl; lines[nl][n] = 0; wl = 0;
    }
    if (add_space && n < COLS && n > 0) { lines[nl][n] = ' '; lines[nl][n + 1] = 0; }
}

static void emit(const char *p)
{
    if (edumkii_button(&d, EDUMKII_PIN_S2)) s2_latched = 1;
    uint32_t t0 = edu_stamp();
    int dirty_all = 0, before = nl;
    uint32_t sc0 = ls.scrolls;
    for (; *p; p++) {
        char c = *p;
        if (c == ' ' || c == '\n') { commit_word(c == ' '); if (c == '\n') new_line(); }
        else if (c >= 33 && c < 127) { if (wl < COLS) word[wl++] = c; }
    }
    /* show the pending word at the end of the current line, wrapping early when it does not fit */
    {
        int n = (int)strlen(lines[nl]);
        if (wl && n + wl > COLS) { new_line(); n = 0; }
        if (nl != before || ls.scrolls != sc0) dirty_all = 1;
        char save[COLS + 1]; strcpy(save, lines[nl]);
        memcpy(lines[nl] + n, word, (size_t)wl); lines[nl][n + wl] = 0;
        if (dirty_all) draw_all(); else draw_line(nl);
        strcpy(lines[nl], save);
    }
    tok_in_story++;
    if ((tok_in_story & 7) == 0) {
        uint32_t el = edu_cycles_since(story_t0);
        header(el ? (uint32_t)((uint64_t)tok_in_story * ls.cpu_hz * 10u / el) : 0, (int)ls.stories + 1);
    }
    disp_acc += edu_cycles_since(t0);
}

int main(void)
{
    SYSCFG_DL_init();
    edu_board_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    ls.cpu_hz = edu_cpu_hz();
    edumkii_init(&d, edu_port());
    edumkii_lcd_init(&d);
    edumkii_backlight(&d, 1);
    int rc = ll_init(ll_model_blob, ll_tok_blob);
    ls.magic = 0x4C534331u;
    if (rc) { edumkii_text(&d, 1, 20, "model init failed", EDUMKII_RED, EDUMKII_BLACK, 1); for (;;) { } }
    uint64_t seed = 1234;
    for (;;) {
        edumkii_fill(&d, EDUMKII_BLACK);
        for (int i = 0; i < ROWS; i++) lines[i][0] = 0;
        nl = 0; wl = 0; tok_in_story = 0; disp_acc = 0; s2_latched = 0;
        header(0, (int)ls.stories + 1);
        uint32_t t0 = edu_stamp(); story_t0 = t0;
        uint32_t ms0 = 0; (void)ms0;
        int n = ll_generate(prompts[ls.stories % NPROMPT], LL_STEPS, 1.0f, 0.9f, seed, emit);
        uint32_t gen = edu_cycles_since(t0);
        commit_word(0); draw_all();
        ls.tokens = (uint32_t)n; ls.gen_cycles = gen; ls.disp_cycles = disp_acc;
        ls.tps_x10 = (uint32_t)((uint64_t)n * ls.cpu_hz * 10u / gen);
        ls.tps_nodisp_x10 = (uint32_t)((uint64_t)n * ls.cpu_hz * 10u / (gen - disp_acc));
        header(ls.tps_x10, (int)ls.stories + 1);
        ls.stories++;
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        /* hold the text until S2 or 30 s after the story began */
        uint32_t held_ms = (uint32_t)((uint64_t)gen * 1000u / ls.cpu_hz);   /* the 32-bit cycle counter wraps in 27 s, so count milliseconds */
        while (!s2_latched && held_ms < STORY_MS) {
            int s = edumkii_button(&d, EDUMKII_PIN_S2);
            if (s && !s2_prev) { s2_latched = 1; ls.s2_presses++; }
            s2_prev = s;
            d.port.delay_ms(d.port.ctx, 20); held_ms += 20;
        }
    }
}
