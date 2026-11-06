/* Mac reference: run every configuration and print the CRCs the board must reproduce. */
#include <stdio.h>
#include "opusrun.h"
#include "opuscfg.h"
int main(int argc, char **argv)
{
    int verbose = argc > 1;
    for (int i = 0; i < OP_N; i++) {
        const int *c = op_cfg[i / OP_NCPLX]; static oprun_t r; static uint32_t ec[100], dc[100]; oprun(c[0], c[1], c[2], c[3], op_cplx[i % OP_NCPLX], &r, ec, dc, 0);
        printf("cfg %d hz %u frame_us %u kbps %u app %u cplx %u frames %u errors %u enc_size %u dec_size %u bytes %u min %u max %u toc0 0x%02x silk %u hybrid %u celt %u bits_crc=0x%08x pcm_crc=0x%08x\n",
               i, r.hz, r.frame_us, r.kbps, r.app, r.complexity, r.frames, r.errors, r.enc_size, r.dec_size, r.bytes_total, r.bytes_min, r.bytes_max, r.toc0, r.n_silk, r.n_hybrid, r.n_celt, r.bits_crc, r.pcm_crc);
        if (verbose) for (unsigned f = 0; f < r.frames; f++) printf("F %u %08x %08x\n", f, ec[f], dc[f]);
    }
    return 0;
}
