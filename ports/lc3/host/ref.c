/* Mac reference: run the three LE Audio configurations and print the CRCs the board must reproduce. */
#include <stdio.h>
#include "lc3run.h"
#ifdef LC3_KEEP_OUT
extern int16_t keep_out[48000];
#endif
int main(int argc, char **argv)
{
    static const int cfg[3][2] = { {16000, 32}, {24000, 48}, {48000, 96} };
    int verbose = argc > 1;
    for (int i = 0; i < 3; i++) {
        static lc3res_t r; static lc3detail_t d; lc3run(cfg[i][0], cfg[i][1], &r, &d, 0);
        printf("cfg %d hz %u kbps %u nbytes %u frames %u errors %u enc_size %u dec_size %u bits_crc=0x%08x pcm_crc=0x%08x\n",
               i, r.hz, r.kbps, r.nbytes, r.frames, r.errors, r.enc_size, r.dec_size, r.bits_crc, r.pcm_crc);
#ifdef LC3_KEEP_OUT
        if (i == 2) { FILE *f = fopen("pcm48k.raw", "wb"); fwrite(keep_out, 2, 48000, f); fclose(f); }   /* decoded PCM of the 48 kHz configuration */
#endif
        if (verbose) for (unsigned f = 0; f < r.frames; f++) printf("F %u %08x %08x\n", f, d.enc_crc[f], d.dec_crc[f]);
    }
    return 0;
}
