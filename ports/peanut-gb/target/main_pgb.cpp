/*
 * Size-measurement entry point for the Peanut-GB front end (Fox Red Labs, BSD-3-Clause).
 * Null display (swallows lines), stick at rest, clock that counts frames. With -DPGB_ROM_EMBEDDED the ROM is
 * linked in from rom_data.c; without it the ROM pointer is a fixed address (where a QSPI or flash image would map),
 * so the numbers are code and RAM only. The board's real drivers replace the null ones next.
 */
#include <frl/abstract.h>
#include "pgb_frontend.h"

extern "C" void __cxa_pure_virtual() { for (;;) {} }

#ifdef PGB_ROM_EMBEDDED
extern "C" const uint8_t frl_rom_data[];
extern "C" const uint32_t frl_rom_len;
#define ROM_PTR frl_rom_data
#define ROM_LEN frl_rom_len
#else
#define ROM_PTR ((const uint8_t *)0x00020000u)
#define ROM_LEN 262144u
#endif

static volatile uint32_t g_sink;
static uint32_t g_ms;
static uint32_t n_millis(void *) { return g_ms; }
static void n_delay(void *, uint32_t ms) { g_ms += ms; }
#ifdef PGB_NULL_SCRIPT
/* Scripted input for the instruction counter (speed/emu_pgb.py): script.h holds {read number, keys} pairs made from
 * scripts/tobu.txt by build_target.sh (PGB_SCRIPT=scripts/tobu.txt); one read per frame. Keys: 1 up, 2 down, 4 left, 8 right, 16 A, 32 B. */
#include "script.h"
static uint32_t g_reads;
static void n_read(void *, frl_input_raw_t *o)
{
    static unsigned idx; static int held;
    while (idx < sizeof pgb_script / sizeof pgb_script[0] && pgb_script[idx].frame <= g_reads) held = pgb_script[idx++].keys;
    int x = 2048, y = 2048;
    if (held & 1) y += 1700;
    if (held & 2) y -= 1700;
    if (held & 4) x -= 1500;
    if (held & 8) x += 1500;
    o->joy_x = (uint16_t)x; o->joy_y = (uint16_t)y;
    o->buttons = (uint8_t)(((held & 16) ? FRL_BTN_1 : 0) | ((held & 32) ? FRL_BTN_2 : 0));
    g_reads++;
}
#else
static void n_read(void *, frl_input_raw_t *o) { o->joy_x = 2048; o->joy_y = 2048; o->buttons = 0; }
#endif
static void n_lines(void *, uint16_t y0, uint16_t n, const void *pix) { g_sink = (uint32_t)(size_t)pix + y0 + n; }

static uint8_t cart_ram[8192];

int main(void)
{
    frl_platform_t pf = {};
#ifdef FRL_TARGET_1BPP
    pf.display.fmt = FRL_FMT_1BPP;
#else
    pf.display.fmt = FRL_FMT_RGB565;
#endif
    pf.display.write_lines = n_lines;
    pf.input.read = n_read;
    pf.millis = n_millis; pf.delay_ms = n_delay;
    frl_set_platform(&pf);
    if (pgb_init(ROM_PTR, ROM_LEN, cart_ram, sizeof cart_ram)) for (;;) {}
    for (;;) { pgb_run_frame(); pgb_present(); g_ms += 17; }
}
