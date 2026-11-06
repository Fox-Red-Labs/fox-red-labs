/*
 * Game Boy on the EDUMKII: Peanut-GB (ports/peanut-gb) behind the same platform hooks as the Arduboy games
 * (examples/snake/frl_edumkii.cpp supplies main, screen, stick, buttons and clock). This file is setup() and loop().
 * The ROM is linked into flash from rom_data.c (made by build.sh from ports/peanut-gb/roms/, git-ignored, never committed).
 * Controls: stick is the D-pad, S1 is A, S2 is B, S1 and S2 together are Start. BSD-3-Clause.
 *
 * Pacing: when the board is faster than the console (160 MHz build), each frame waits until 16.742 ms of game time have
 * passed (59.73 frames a second), so the game plays at its own speed. -DEDU_GB_UNPACED (build.sh: GB_UNPACED=1) runs flat
 * out, which is how the board's top speed is read from the frame counters.
 */
#include <stdint.h>
#include <stddef.h>
#include <frl/abstract.h>
extern "C" {
#include "pgb_frontend.h"
extern const uint8_t frl_rom_data[];
extern const uint32_t frl_rom_len;
}
static uint8_t cart_ram[8192];
volatile int gb_init_result = 99;
volatile uint32_t gb_late_frames;      /* frames that finished after their due time (the board could not keep up) */
void setup() { gb_init_result = pgb_init(frl_rom_data, frl_rom_len, cart_ram, sizeof cart_ram); }

static inline uint32_t now_ms() { const frl_platform_t *p = frl_platform(); return p->millis(p->clock_ctx); }

void loop()
{
    if (gb_init_result != 0) return;
    pgb_run_frame();
    pgb_present();
#ifndef EDU_GB_UNPACED
    static uint32_t t0, n; static bool started;
    if (!started) { t0 = now_ms(); n = 0; started = true; }
    n++;
    uint32_t due = (uint32_t)(((uint64_t)n * 16742u) / 1000u);
    uint32_t el = now_ms() - t0;
    if (el > due + 100u) { t0 = now_ms(); n = 0; gb_late_frames++; }     /* far behind: start counting again, do not race to catch up */
    else while ((uint32_t)(now_ms() - t0) < due) { }
#endif
}
