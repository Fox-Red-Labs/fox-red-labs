#include "ba_chip.h"

/* Frequency in 1/16 Hz for MIDI notes 0 to 127 is not stored: compute f = 440 * 2^((n-69)/12) from a 12-entry
 * table of one octave (note 60 to 71, in 1/16 Hz) and shifts. */
static const uint32_t oct4_x16[12] = {4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902};
/* middle C (60) = 261.63 Hz = 4186 /16 Hz */

uint32_t ba_midi_step24(uint8_t note, uint32_t rate_hz)
{
    int n = (int)note - 60;
    int oct = 0;
    while (n < 0) { n += 12; oct--; }
    while (n >= 12) { n -= 12; oct++; }
    uint64_t f16 = oct4_x16[n]; /* 1/16 Hz */
    if (oct > 0) f16 <<= oct; else if (oct < 0) f16 >>= -oct;
    /* step = f / rate * 2^24, f = f16 / 16 */
    return (uint32_t)((f16 << 20) / rate_hz);
}

void ba_chip_init(ba_chip_t *c, const ba_song_t *song, uint32_t rate_hz)
{
    c->song = song;
    c->rate = rate_hz;
    c->sample_in_row = 0;
    c->row = 0;
    for (int i = 0; i < 3; i++) { c->phase[i] = 0; c->step[i] = 0; }
    c->lfsr = 1;
    c->drum = 0;
    c->drum_pos = 0;
    c->noise_div = 1;
    c->noise_cnt = 0;
    c->noise_out = 1;
    /* row 0 is read on the first call */
    c->sample_in_row = song->samples_per_row;
    c->row = (uint16_t)(song->nrows - 1);
}

static void load_row(ba_chip_t *c)
{
    const ba_song_t *s = c->song;
    c->row = (uint16_t)(c->row + 1);
    if (c->row >= s->nrows) c->row = 0;
    const uint8_t *r = s->rows[c->row];
    for (int v = 0; v < 3; v++) {
        if (r[v] == BA_CODE_REST) c->step[v] = 0;
        else if (r[v] != BA_CODE_HOLD) c->step[v] = ba_midi_step24(r[v], c->rate);
    }
    if (r[3] >= BA_DRUM_KICK) {
        c->drum = r[3];
        c->drum_pos = 0;
        c->noise_div = (uint8_t)(((r[3] == BA_DRUM_KICK) ? 24u : (r[3] == BA_DRUM_SNARE) ? 3u : 1u) * c->rate / 8000u); /* the same drum sound at any rate */
        if (!c->noise_div) c->noise_div = 1;
    } else if (r[3] == BA_CODE_REST) {
        c->drum = 0;
    }
    c->sample_in_row = 0;
}

static int16_t tri24(uint32_t ph)
{
    int32_t x = (int32_t)((ph & 0xFFFFFFu) >> 10); /* 14 bits, one cycle */
    if (x >= 8192) x = 16383 - x;                   /* fold: 0 up to 8191 and back */
    return (int16_t)((x - 4096) * 8);               /* about +/-32768 */
}

int16_t ba_chip_next(ba_chip_t *c)
{
    if (c->sample_in_row >= c->song->samples_per_row) load_row(c);
    c->sample_in_row++;
    int32_t mix = 0;
    const uint8_t *vol = c->song->vol;
    /* voice 0: 25 percent pulse */
    if (c->step[0]) { c->phase[0] += c->step[0]; mix += ((c->phase[0] & 0xFFFFFFu) < 0x400000u ? 1 : -1) * 2048 * vol[0] / 4; }
    /* voice 1: 50 percent pulse */
    if (c->step[1]) { c->phase[1] += c->step[1]; mix += ((c->phase[1] & 0xFFFFFFu) < 0x800000u ? 1 : -1) * 2048 * vol[1] / 4; }
    /* voice 2: triangle */
    if (c->step[2]) { c->phase[2] += c->step[2]; mix += (int32_t)tri24(c->phase[2]) * vol[2] / 64; }
    /* voice 3: noise with a decaying envelope */
    if (c->drum) {
        if (++c->noise_cnt >= c->noise_div) {
            c->noise_cnt = 0;
            unsigned bit = (c->lfsr ^ (c->lfsr >> 1)) & 1u;
            c->lfsr = (uint16_t)((c->lfsr >> 1) | (bit << 14));
            c->noise_out = (c->lfsr & 1u) ? 1 : -1;
        }
        unsigned len = ((c->drum == BA_DRUM_HAT) ? 400u : (c->drum == BA_DRUM_SNARE) ? 1200u : 1600u) * c->rate / 8000u; /* 50, 150, 200 ms */
        if (c->drum_pos >= len) { c->drum = 0; }
        else {
            int32_t env = (int32_t)(len - c->drum_pos) * 256 / (int32_t)len; /* 256 to 0 */
            mix += c->noise_out * env * 24 * vol[3] / 16;
            c->drum_pos++;
        }
    }
    /* four voices at full volume sum to about +/-3 x 8192 + 16384; keep inside int16 */
    if (mix > 32000) mix = 32000;
    if (mix < -32000) mix = -32000;
    return (int16_t)mix;
}
