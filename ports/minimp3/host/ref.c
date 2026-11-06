/* Mac reference: decode a clip with the same common/mp3run.c and print the result as one line of key=value, plus optional raw PCM.
 * build: host/build.sh   run: ref <file.mp3> [out.raw] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mp3run.h"
static mp3detail_t d;
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    static mp3run_t r; mp3run(b, (int)n, &r, &d, NULL);
    printf("frames=%u ch=%u hz=%u kbps=%u samples_per_ch=%u hash_all=0x%08x used=%u of %ld\n", r.frames, r.channels, r.hz, r.kbps, r.samples_per_ch, r.hash_all, r.bytes_used, n);
    for (unsigned i = 0; i < r.frames; i++) printf("F %u 0x%08x\n", i, d.hash[i]);
    if (argc > 2) { /* full raw PCM: decode again, writing out */
        FILE *o = fopen(argv[2], "wb"); extern int mp3_dump(FILE *, const uint8_t *, int); mp3_dump(o, b, (int)n); fclose(o);
    }
    return 0;
}
