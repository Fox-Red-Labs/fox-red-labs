/* The bench body shared by every target: decode both clips once, leave results in RAM for the probe (or emulator) to read.
 * Symbols: mp_res[2] (0 = 128 kbps stereo, 1 = 64 kbps mono), mp_det[2], mp_done (0x4D503344 when both finished). */
#include "mp3run.h"
extern const unsigned char mp3_clip_stereo[], mp3_clip_mono[];
extern const unsigned int mp3_clip_stereo_len, mp3_clip_mono_len;
volatile mp3run_t mp_res[2];
mp3detail_t mp_det[2];
volatile uint32_t mp_done;
void mp3bench(uint32_t (*now)(void))
{
    static mp3run_t r;
    mp3run(mp3_clip_stereo, (int)mp3_clip_stereo_len, &r, &mp_det[0], now); for (unsigned i = 0; i < sizeof r / 4; i++) ((volatile uint32_t *)&mp_res[0])[i] = ((uint32_t *)&r)[i];
    mp3run(mp3_clip_mono, (int)mp3_clip_mono_len, &r, &mp_det[1], now); for (unsigned i = 0; i < sizeof r / 4; i++) ((volatile uint32_t *)&mp_res[1])[i] = ((uint32_t *)&r)[i];
    mp_done = 0x4D503344u;
}
