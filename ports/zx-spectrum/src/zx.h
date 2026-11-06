/* ZX Spectrum 48K machine around the chips Z80 core (third-party/chips/z80.h): memory map, ULA port, 50 Hz interrupt, screen decode,
 * keyboard matrix. Portable C, no hardware. Written fresh from the public memory-map and ULA descriptions. Not modelled: contended
 * memory, floating bus, the beeper (output bits are ignored, so the machine is silent), tape. */
#ifndef ZX_H
#define ZX_H
#include <stdint.h>
#include "z80.h"

#define ZX_TSTATES_PER_FRAME 69888u      /* 312 lines of 224 T-states: 50.08 frames per second at 3.5 MHz */
#define ZX_CLOCK_HZ 3500000u

typedef struct {
    uint8_t ram[49152];              /* 0x4000 to 0xFFFF; first, so its address is the struct's on every target (the board check reads it) */
    z80_t cpu;
    uint64_t pins;
    const uint8_t *rom;              /* 16 kB at 0x0000 */
    uint8_t keys[8];                 /* half-row bits, 1 = pressed */
    uint8_t border;                  /* 0 to 7 */
    uint32_t frames;
    uint32_t tstates;                /* total ticks run */
} zx_t;

void zx_init(zx_t *z, const uint8_t *rom16k);
void zx_run_frame(zx_t *z);                                   /* ZX_TSTATES_PER_FRAME ticks, the interrupt at the start */
void zx_key(zx_t *z, int halfrow, int bit, int down);
/* One character row (8 pixel lines, 32 cells) as 256 x 8 RGB565 pixels. flash_phase 0 or 1 swaps ink and paper where the FLASH bit is set. */
void zx_render_band(const zx_t *z, int band, int flash_phase, uint16_t *out);
uint16_t zx_border_rgb(int border);
#endif
