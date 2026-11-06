#define CHIPS_ASSERT(c)      /* no assert on the board; the host test builds the same file with the same macro */
#define CHIPS_IMPL
#include <string.h>
#ifdef ZX_RAMFUNC
#pragma clang section text = ".TI.ramfunc"      /* the Z80 core and the tick loop run from SRAM (copied at boot); the linker file sets the SRAM bank */
#endif
#include "zx.h"

void zx_init(zx_t *z, const uint8_t *rom16k)
{
    memset(z, 0, sizeof *z);
    z->rom = rom16k;
    z->pins = z80_init(&z->cpu);
}

static inline void run_ticks(zx_t *z, uint32_t n)
{
    uint64_t pins = z->pins;
    const uint8_t *rom = z->rom; uint8_t *ram = z->ram;
    while (n--) {
        pins = z80_tick(&z->cpu, pins);
        if (pins & Z80_MREQ) {
            uint16_t a = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) { Z80_SET_DATA(pins, a < 0x4000 ? rom[a] : ram[a - 0x4000]); }
            else if ((pins & Z80_WR) && a >= 0x4000) ram[a - 0x4000] = Z80_GET_DATA(pins);
        } else if (pins & Z80_IORQ) {
            if (pins & Z80_M1) {                       /* interrupt acknowledge: the bus floats high (mode 1 ignores it) */
                Z80_SET_DATA(pins, 0xFF);
            } else if (!(pins & Z80_A0)) {             /* the ULA answers every even port */
                if (pins & Z80_RD) {
                    uint8_t d = 0xBF, hi = (uint8_t)(pins >> 8);   /* bits 5 and 7 read as 1; no tape signal */
                    uint8_t k = 0;
                    for (int r = 0; r < 8; r++) if (!(hi & (1u << r))) k |= z->keys[r];
                    Z80_SET_DATA(pins, (uint8_t)(d | 0x1F) & (uint8_t)~(k & 0x1F));
                } else if (pins & Z80_WR) {
                    z->border = Z80_GET_DATA(pins) & 7;
                }
            } else if (pins & Z80_RD) {
                Z80_SET_DATA(pins, 0xFF);
            }
        }
    }
    z->pins = pins;
}

void zx_run_frame(zx_t *z)
{
    z->pins |= Z80_INT; run_ticks(z, 32); z->pins &= ~Z80_INT;     /* the interrupt line is held for 32 ticks */
    run_ticks(z, ZX_TSTATES_PER_FRAME - 32);
    z->tstates += ZX_TSTATES_PER_FRAME; z->frames++;
}

#ifdef ZX_RAMFUNC
#pragma clang section text = ""
#endif
void zx_key(zx_t *z, int hr, int bit, int down)
{
    if (down) z->keys[hr] |= (uint8_t)(1u << bit); else z->keys[hr] &= (uint8_t)~(1u << bit);
}

static uint16_t pal[16];
static void pal_init(void)
{
    for (int i = 0; i < 16; i++) {
        int lvl = i & 8 ? 255 : 205, b = (i & 1) ? lvl : 0, r = (i & 2) ? lvl : 0, g = (i & 4) ? lvl : 0;
        pal[i] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    pal[8] = 0;       /* bright black is black */
}
uint16_t zx_border_rgb(int b) { if (!pal[15]) pal_init(); return pal[b & 7]; }

void zx_render_band(const zx_t *z, int band, int flash, uint16_t *out)
{
    if (!pal[15]) pal_init();
    for (int line = 0; line < 8; line++) {
        int y = band * 8 + line;
        uint32_t off = ((y & 0xC0) << 5) | ((y & 7) << 8) | (((y >> 3) & 7) << 5);      /* the Spectrum's interleaved bitmap address */
        const uint8_t *bm = &z->ram[off], *at = &z->ram[0x1800 + band * 32];
        uint16_t *o = out + line * 256;
        for (int cx = 0; cx < 32; cx++) {
            uint8_t a = at[cx], b = bm[cx];
            int br = (a & 0x40) ? 8 : 0;
            uint16_t ink = pal[(a & 7) | br], paper = pal[((a >> 3) & 7) | br];
            if ((a & 0x80) && flash) { uint16_t t = ink; ink = paper; paper = t; }
            for (int k = 0; k < 8; k++) *o++ = (b & (0x80 >> k)) ? ink : paper;
        }
    }
}
