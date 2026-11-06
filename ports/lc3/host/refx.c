/* Mac reference for the extras bench: prints the CRCs the board must reproduce. */
#include <stdio.h>
#include "lc3x.h"
#include "lc3xcfg.h"
int main(void)
{
    for (int i = 0; i < LC3X_N; i++) {
        static lc3xres_t r; lc3xrun(lc3x_cfg[i][0], lc3x_cfg[i][1], lc3x_cfg[i][2], lc3x_cfg[i][3], lc3x_cfg[i][4], &r, 0);
        printf("cfg %d dt %u hz %u kbps %u nch %u loss %u nbytes %u frames %u errors %u bits_crc=0x%08x pcm_crc=0x%08x\n",
               i, r.dt_us, r.hz, r.kbps, r.nch, r.loss_every, r.nbytes, r.frames, r.errors, r.bits_crc, r.pcm_crc);
    }
    return 0;
}
