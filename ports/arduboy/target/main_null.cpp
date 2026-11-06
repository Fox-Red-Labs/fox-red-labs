/*
 * Size-measurement entry point for a board build (Fox Red Labs, BSD-3-Clause).
 *
 * Registers a platform whose display swallows lines, whose stick rests at mid-scale and whose
 * clock counts loop() calls, then runs the game forever. It links the whole layer and the game, so
 * `size` reports what they cost. The board's real display, input and tone drivers (EDUMKII,
 * Sharp) replace the null ones next; their code is not in these numbers.
 *
 * Build with -DFRL_TARGET_1BPP for the 1-bit panel path (default is RGB565).
 */
#include <frl/abstract.h>

extern void setup();
extern void loop();

extern "C" void __cxa_pure_virtual() { for (;;) {} }

static volatile uint32_t g_sink;
static uint32_t g_ms;

static uint32_t n_millis(void *) { return g_ms; }
static void n_delay(void *, uint32_t ms) { g_ms += ms; }
static void n_read(void *, frl_input_raw_t *o) { o->joy_x = 2048; o->joy_y = 2048; o->buttons = 0; }
static void n_tone(void *, uint16_t hz, uint16_t ms) { g_sink = hz + ms; }
static void n_lines(void *, uint16_t y0, uint16_t n, const void *pix) { g_sink = (uint32_t)(size_t)pix + y0 + n; }

int main(void)
{
    frl_platform_t pf = {};
#ifdef FRL_TARGET_1BPP
    pf.display.fmt = FRL_FMT_1BPP;
#else
    pf.display.fmt = FRL_FMT_RGB565; pf.display.fg = 0xFFFF; pf.display.bg = 0;
#endif
    pf.display.write_lines = n_lines;
    pf.input.read = n_read;
    pf.audio.tone = n_tone;
    pf.millis = n_millis; pf.delay_ms = n_delay;
    frl_set_platform(&pf);
    setup();
    for (;;) { loop(); g_ms++; }
}
