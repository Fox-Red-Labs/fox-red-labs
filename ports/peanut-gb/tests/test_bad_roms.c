/*
 * Crafted-ROM tests for the Peanut-GB front end (Fox Red Labs, BSD-3-Clause). Built with
 * -fsanitize=address,undefined and -fno-sanitize-recover, so any out-of-bounds index stops it.
 *
 *   header cases : valid header checksum but a ROM-size or RAM-size code past the end of Peanut-GB's tables.
 *                  pgb_init must refuse them with a message; gb_init called directly (no pre-check) must refuse them
 *                  too (this one needs the patched header).
 *   bank cases   : a tiny MBC3 ROM that selects cart RAM bank 0x0D, 0x0E, 0x0F and 0xFF, then reads and writes
 *                  0xA000 in a loop for several frames (this one needs the patched header: the unpatched code indexes
 *                  the 5-byte RTC array with bank minus 8).
 * Exit 0 only if all pass. Argument "header" or "banks" runs just that group.
 */
#define PEANUT_GB_HEADER_ONLY
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "peanut_gb.h"
#include "pgb_frontend.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); } while (0)

static void fix_checksum(uint8_t *rom)
{
    uint8_t x = 0;
    for (int i = 0x134; i <= 0x14C; i++) x = (uint8_t)(x - rom[i] - 1);
    rom[0x14D] = x;
}

static void make_rom(uint8_t *rom, size_t len, uint8_t type, uint8_t rom_code, uint8_t ram_code)
{
    memset(rom, 0, len);
    rom[0x100] = 0x00; rom[0x101] = 0xC3; rom[0x102] = 0x50; rom[0x103] = 0x01;   /* nop; jp 0x0150 */
    rom[0x147] = type; rom[0x148] = rom_code; rom[0x149] = ram_code;
    fix_checksum(rom);
}

static uint8_t g_cram[0x20000];
static const uint8_t *g_rom;
static size_t g_len;
static uint8_t rd(struct gb_s *gb, const uint_fast32_t a) { (void)gb; return a < g_len ? g_rom[a] : 0xFF; }
static uint8_t crd(struct gb_s *gb, const uint_fast32_t a) { (void)gb; return a < sizeof g_cram ? g_cram[a] : 0xFF; }
static void cwr(struct gb_s *gb, const uint_fast32_t a, const uint8_t v) { (void)gb; if (a < sizeof g_cram) g_cram[a] = v; }
static void err(struct gb_s *gb, const enum gb_error_e e, const uint16_t a) { (void)gb; (void)e; (void)a; }

int main(int argc, char **argv)
{
    int only_banks = argc > 1 && !strcmp(argv[1], "banks"), only_header = argc > 1 && !strcmp(argv[1], "header");
    static uint8_t rom[0x8000];
    static struct gb_s gb;

    /* header cases */
    int bad_codes[][2] = { { 0x09, 0 }, { 0x20, 0 }, { 0xFF, 0 }, { 0, 0x06 }, { 0, 0x30 }, { 0, 0xFF } };
    for (unsigned i = 0; !only_banks && i < sizeof bad_codes / sizeof bad_codes[0]; i++) {
        make_rom(rom, sizeof rom, 0x13, (uint8_t)bad_codes[i][0], (uint8_t)bad_codes[i][1]);
        int r = pgb_init(rom, sizeof rom, g_cram, sizeof g_cram);
        char m[160]; snprintf(m, sizeof m, "pgb_init refuses ROM code 0x%02X RAM code 0x%02X: %s", bad_codes[i][0], bad_codes[i][1], pgb_error_text(r));
        CHECK(r == -5 || r == -6, m);
        g_rom = rom; g_len = sizeof rom;
        enum gb_init_error_e e = gb_init(&gb, rd, crd, cwr, err, NULL);
        snprintf(m, sizeof m, "gb_init alone (no pre-check) refuses ROM code 0x%02X RAM code 0x%02X", bad_codes[i][0], bad_codes[i][1]);
        CHECK(e == GB_INIT_CARTRIDGE_UNSUPPORTED, m);
    }
    make_rom(rom, 64, 0x13, 0, 0);
    if (!only_banks) CHECK(pgb_init(rom, 64, g_cram, sizeof g_cram) == -4, "pgb_init refuses a 64-byte file");

    /* bank cases: MBC3 + RAM + battery (0x13), 32 kB ROM (code 0), 32 kB RAM (code 3) */
    uint8_t banks[] = { 0x0D, 0x0E, 0x0F, 0xFF };
    for (unsigned i = 0; !only_header && i < sizeof banks; i++) {
        make_rom(rom, sizeof rom, 0x13, 0, 3);
        const uint8_t prog[] = {
            0x3E, 0x0A, 0xEA, 0x00, 0x00,        /* ld a,0x0A ; ld (0x0000),a   enable cart RAM */
            0x3E, banks[i], 0xEA, 0x00, 0x40,    /* ld a,bank ; ld (0x4000),a   select bank     */
            0xFA, 0x00, 0xA0,                    /* ld a,(0xA000)               read            */
            0xEA, 0x00, 0xA0,                    /* ld (0xA000),a               write           */
            0xC3, 0x5A, 0x01                     /* jp 0x015A                   loop            */
        };
        memcpy(rom + 0x150, prog, sizeof prog);
        int r = pgb_init(rom, sizeof rom, g_cram, sizeof g_cram);
        char m[120]; snprintf(m, sizeof m, "ROM selecting bank 0x%02X loads (%s)", banks[i], pgb_error_text(r));
        CHECK(r == 0, m);
        if (r == 0) {
            for (int f = 0; f < 5; f++) pgb_run_frame_no_input();
            snprintf(m, sizeof m, "ROM selecting bank 0x%02X ran 5 frames with no out-of-bounds access", banks[i]);
            CHECK(pgb_frames() == 5, m);
        }
    }
    printf(fails ? "FAILED: %d\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
