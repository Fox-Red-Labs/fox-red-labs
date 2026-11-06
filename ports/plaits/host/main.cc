/* Mac reference: renders every engine and prints one line per block (engine, block, CRC-32) plus a summary line per engine. */
#include <stdio.h>
#include <stdlib.h>
#include "plaitsrun.h"
static uint32_t zero(void) { return 0; }
int main(int argc, char** argv) {
    if (argc > 2) { pl_keep_engine = atoi(argv[2]); }
    for (int e = 0; e < PL_ENGINES; e++) pl_run_engine(e, zero);
    for (int e = 0; e < PL_ENGINES; e++) {
        const pl_res_t* r = &pl_res[e];
        printf("E %d %s active=%u crc_all=%08x peak=%u nonzero=%u\n", e, pl_names[e], r->active_engine, r->crc_all, r->peak, r->nonzero);
        if (argc > 1 && argv[1][0] == 'v') for (int b = 0; b < PL_BLOCKS; b++) printf("B %d %d %08x\n", e, b, pl_crc_blk[e][b]);
    }
    printf("voice_bytes=%u shared_free_after_init=%u\n", pl_info.voice_bytes, pl_info.shared_free_after_init);
    if (pl_keep_engine >= 0) { FILE* f = fopen("keep.raw", "wb"); fwrite(pl_keep, sizeof pl_keep, 1, f); fclose(f); }
    return 0;
}
