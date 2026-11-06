/* Host runner: load a ROM, run frames, print the screen as text. The pass screens are checked by eye.
 *   test_host ROM [platform_byte_at_0x1FF] [frames] [steps_per_frame] [key_script]
 * key_script: "frame:key:1|0,..." e.g. "10:1:1,12:1:0" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/chip8.h"
static chip8_t c;
int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    static uint8_t rom[4096]; FILE *f = fopen(argv[1], "rb"); if (!f) return 2; unsigned n = (unsigned)fread(rom, 1, sizeof rom, f); fclose(f);
    int plat = argc > 2 ? atoi(argv[2]) : 0, frames = argc > 3 ? atoi(argv[3]) : 300, spf = argc > 4 ? atoi(argv[4]) : 12;
    const char *ks = argc > 5 ? argv[5] : "";
    c8_reset(&c, &c8_quirks_vip); c8_load(&c, rom, n);
    if (plat) { c.mem[0x1FF] = (uint8_t)plat; if (plat != 1) c.q = c8_quirks_modern; }
    for (int fr = 0; fr < frames; fr++) {
        const char *p = ks;
        while (*p) { int a, k, d; if (sscanf(p, "%d:%d:%d", &a, &k, &d) == 3 && a == fr) c.key[k] = (uint8_t)d; p = strchr(p, ','); if (!p) break; p++; }
        c8_frame(&c, spf);
    }
    for (int y = 0; y < C8_H; y++) { for (int x = 0; x < C8_W; x++) putchar(c.fb[y * C8_W + x] ? '#' : ' '); putchar('|'); putchar('\n'); }
    printf("steps=%u pc=%03X\n", c.steps, c.pc);
    return 0;
}
