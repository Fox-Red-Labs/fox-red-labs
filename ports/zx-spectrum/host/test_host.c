/* Host checks of the machine around the chips Z80: the ROM-less test image runs, the interrupt wakes HALT every frame, the screen
 * decoder follows the Spectrum's interleaved address layout, keyboard rows read back, the border port works.
 *   test_host rom.bin [frames] [ppm_out]      (rom.bin from tools/mkrom.py) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/zx.h"
static zx_t z; static uint8_t rom[16384]; static int bad;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); bad++; } } while (0)
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); if (!f || fread(rom, 1, 16384, f) != 16384) return 2; fclose(f);
    int frames = argc > 2 ? atoi(argv[2]) : 120;
    zx_init(&z, rom);
    for (int i = 0; i < frames; i++) zx_run_frame(&z);
    CHECK(z.ram[0] == 0x40 && z.ram[1] == 0x41 && z.ram[0x100] == 0x41, "bitmap fill is L xor H");
    uint8_t d = z.cpu.d;           /* the program's frame counter */
    printf("frames=%d, D (HALT wake-ups)=%d\n", frames, d);
    CHECK(frames - d >= 5 && frames - d <= 6, "one HALT wake-up per frame after the bitmap fill (6,144 bytes at 57 T-states is five frames)");
    CHECK(z.ram[0x1800 + 5] == (uint8_t)(((d + 5) & 0x7F) | 1), "attribute rewrite follows the counter");
    /* the screen decoder: put one pixel at line 100, column 3 and read it back through a band */
    memset(z.ram, 0, 6912); z.ram[0x1800 + (100 / 8) * 32 + 3] = 0x38;     /* paper white, ink black */
    int y = 100; z.ram[((y & 0xC0) << 5) | ((y & 7) << 8) | (((y >> 3) & 7) << 5) | 3] = 0x80;
    static uint16_t buf[256 * 8]; zx_render_band(&z, 100 / 8, 0, buf);
    CHECK(buf[(100 % 8) * 256 + 3 * 8] == 0 && buf[(100 % 8) * 256 + 3 * 8 + 1] != 0 && buf[(99 % 8) * 256 + 3 * 8] != 0, "pixel (24,100) decodes to black on white, neighbours white");
    /* ULA port: keyboard row read through IN A,(FE) */
    zx_t k; static uint8_t krom[16384];
    uint8_t prog[] = { 0x01, 0xFE, 0xFB, 0xED, 0x78, 0x32, 0x00, 0x40, 0x3E, 0x05, 0xD3, 0xFE, 0x76 };   /* LD BC,FBFE ; IN A,(C) ; LD (4000),A ; LD A,5 ; OUT (FE),A ; HALT */
    memcpy(krom, prog, sizeof prog); zx_init(&k, krom); zx_key(&k, 2, 0, 1);              /* Q pressed (half-row 2 = Q W E R T) */
    zx_run_frame(&k);
    CHECK(k.ram[0] == 0xBE, "IN A,(C) with B=FB reads 0xBE with Q down");
    CHECK(k.border == 5, "OUT (FE),5 sets the border");
    if (argc > 3) {
        FILE *o = fopen(argv[3], "wb"); fprintf(o, "P6\n256 192\n255\n");
        static uint16_t b2[256 * 8];
        for (int b = 0; b < 24; b++) { zx_render_band(&z, b, 0, b2); for (int i = 0; i < 256 * 8; i++) { uint16_t p = b2[i]; unsigned char c[3] = { (unsigned char)((p >> 8) & 0xF8), (unsigned char)((p >> 3) & 0xFC), (unsigned char)((p << 3) & 0xF8) }; fwrite(c, 1, 3, o); } }
        fclose(o);
    }
    printf(bad ? "%d FAILED\n" : "all host checks passed\n", bad);
    return bad != 0;
}
