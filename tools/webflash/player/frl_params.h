/*
 * The shared parameter block (format 1), read by every bench image that has a Change tab. The browser bench rewrites
 * the board's last flash sector (PL_TABLE_ADDR, from board_leds.h) and resets the chip; the image reads its
 * settings here at start. Layout and rules: tools/webflash/params.py and site/flash/bench/README.md.
 *
 *   word 0 magic, word 1 format, word 2 count, word 3 image id, then one 32-bit value per parameter.
 *
 * frl_get(i) returns the value written for parameter i, clamped to its limits, or its default when the block is
 * missing, damaged, written for another image or shorter than i. frl_params_gen.h (made from params.py) holds the
 * image id, count, defaults and limits.
 */
#ifndef FRL_PARAMS_H
#define FRL_PARAMS_H
#include <stdint.h>
#include "frl_params_gen.h"

#if FRL_N_PARAMS > 0u
static uint32_t frl_get(uint32_t i)
{
    const volatile uint32_t *b = (const volatile uint32_t *) PL_TABLE_ADDR;
    uint32_t v = frl_def[i];
    if (b[0] == FRL_PARAMS_MAGIC && b[1] == FRL_PARAMS_FORMAT && b[3] == FRL_IMAGE_ID && i < b[2] && b[2] <= 64u) {
        v = b[4u + i];
        v = v < frl_lo[i] ? frl_lo[i] : (v > frl_hi[i] ? frl_hi[i] : v);
    }
    return v;
}
#endif
#endif
