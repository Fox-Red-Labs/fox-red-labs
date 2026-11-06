/* Plaits bench: renders a fixed note sequence through the unmodified Plaits voice, one engine at a time, in 12-sample blocks at 48 kHz.
 * Shared by the Mac reference build and the board build. Per block it keeps the CRC-32 of the 12 stereo frames and the cycles spent inside Voice::Render. */
#ifndef PLAITSRUN_H_
#define PLAITSRUN_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PL_ENGINES 24
#define PL_NOTES 4
#define PL_BLOCKS_PER_NOTE 64
#define PL_BLOCKS (PL_NOTES * PL_BLOCKS_PER_NOTE)   /* 256 blocks = 64 ms per engine */
#define PL_BLOCK 12                                  /* Plaits' own block size (kBlockSize) */
#define PL_GATE_BLOCKS 48                            /* gate high for the first 48 blocks of each note */
typedef struct {
    uint32_t engine, blocks, crc_all, peak, nonzero, active_engine;
    uint32_t cyc_lo, cyc_hi;          /* 64-bit sum of cycles over all blocks */
    uint32_t cyc_first, cyc_min, cyc_max;   /* first block (engine switch, data load); min and max over blocks 1..end */
    uint32_t cyc_worst_idx;
} pl_res_t;
typedef struct { uint32_t voice_bytes, shared_bytes, shared_free_after_init; } pl_info_t;
extern pl_res_t pl_res[PL_ENGINES];
extern uint32_t pl_crc_blk[PL_ENGINES][PL_BLOCKS];
extern uint32_t pl_cyc_blk[PL_ENGINES][PL_BLOCKS];
extern pl_info_t pl_info;
extern int16_t pl_keep[PL_BLOCKS * PL_BLOCK * 2];    /* stereo frames of the engine named by pl_keep_engine, -1 for none */
extern int pl_keep_engine;
extern const char* const pl_names[PL_ENGINES];
uint32_t pl_crc32(uint32_t crc, const void* p, unsigned n);
/* Renders engine e (0..23) with a freshly initialised voice. cyc() returns a free-running 32-bit cycle count, or 0 on the Mac. */
void pl_run_engine(int e, uint32_t (*cyc)(void));
#ifdef __cplusplus
}
#endif
#endif
