/* Host test for the Halloween listener: the synthetic clips (halloween/ba_testclips.h) at five levels each, through
 * halloween/ba_listen.c, the same code the board runs. Prints the confusion matrix; run with host/run_halloween.sh. */
#include <stdio.h>
#include "ba_listen.h"
#include "ba_testclips.h"

int main(void)
{
    static const char *kn[TC_COUNT] = { "clap", "knock", "whistle", "shout", "quiet", "tick" };
    static const uint8_t want[TC_COUNT] = { LS_CLAP, LS_KNOCK, LS_WHISTLE, LS_SHOUT, LS_NONE, LS_NONE };
    static const uint16_t gains[] = { 64, 128, 256, 512, 1024 };
    int m[TC_COUNT][LS_COUNT] = { { 0 } }, ok = 0, all = 0;
    for (int k = 0; k < TC_COUNT; k++)
        for (unsigned g = 0; g < sizeof gains / sizeof gains[0]; g++) {
            ba_listen_t l; ba_listen_init(&l); uint32_t rng = 12345u + (uint32_t)k * 77u + g;
            uint8_t got = LS_NONE;
            for (uint32_t i = 0; i < 8000u; i++) ba_listen_sample(&l, ba_testclip(TC_QUIET, i, &rng, 256));  /* 1 s to learn the floor */
            for (uint32_t i = 0; i < TC_LEN + 8000u; i++) {
                uint8_t c = ba_listen_sample(&l, i < TC_LEN ? ba_testclip((uint8_t)k, i, &rng, gains[g]) : ba_testclip(TC_QUIET, i, &rng, 256));
                if (c && !got) got = c;
            }
            m[k][got]++; all++; if (got == want[k]) ok++;
            printf("%-8s gain %4u -> %-8s (%u ms, zcr %u, voiced %u%%, pitch %u Hz, peak %u, floor %u)\n", kn[k], gains[g], ba_listen_name(got),
                   l.last_ms, l.last_zcr, l.last_voiced_pct, l.last_pitch_hz, (unsigned)l.last_peak, (unsigned)l.last_floor);
        }
    printf("\nconfusion (rows: clip, columns: heard none clap knock whistle shout)\n");
    for (int k = 0; k < TC_COUNT; k++) { printf("%-8s", kn[k]); for (int c = 0; c < LS_COUNT; c++) printf(" %3d", m[k][c]); printf("\n"); }
    printf("%d of %d clips right\n", ok, all);
    return 0;
}
