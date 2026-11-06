#include "ba_listen.h"
#include "ba_yin.h"

static const char *const names[LS_COUNT] = { "none", "clap", "knock", "whistle", "shout" };
const char *ba_listen_name(uint8_t c) { return c < LS_COUNT ? names[c] : "?"; }

#define ABS_MIN 400u        /* frame energy in code^2: about 20 codes RMS (16 mV), well over the 2-code quiet floor */
#define START_X 8u
#define END_X 3u
#define SHORT_FRAMES 10u    /* 160 ms */
#define MAX_FRAMES 125u     /* 2 s */
#define COOLDOWN 188u       /* 3 s */

void ba_listen_init(ba_listen_t *l)
{
    uint8_t *p = (uint8_t *)l; for (unsigned i = 0; i < sizeof *l; i++) p[i] = 0;
    l->dc_q8 = 2048 << 8;
    l->floor = 50;
    l->warm = 16;
}

void ba_listen_reset_event(ba_listen_t *l) { l->in_event = 0; l->quiet = 0; l->e_acc = 0; l->fn = 0; l->zc = 0; l->warm = 16; l->seeded = 0; }

static uint8_t classify(ba_listen_t *l)
{
    uint32_t ms = (uint32_t)l->ev_frames * 16u;
    uint32_t zcr = l->ev_frames ? l->ev_zc_sum / l->ev_frames : 0;     /* crossings per 128 samples */
    uint32_t vp = l->ev_pitch_n ? (uint32_t)l->ev_voiced * 100u / l->ev_pitch_n : 0;
    l->last_ms = (uint16_t)ms; l->last_zcr = (uint16_t)zcr; l->last_voiced_pct = (uint16_t)vp;
    l->last_pitch_hz = l->ev_pitch_hi; l->last_peak = l->ev_peak; l->last_floor = l->floor;
    if (l->ev_frames < 2u) return LS_NONE;   /* a single 16 ms click (a clock, a switch) is not an event */
    if (l->ev_frames <= SHORT_FRAMES) return zcr >= 30u ? LS_CLAP : LS_KNOCK;   /* 30 a frame is about 940 Hz */
    if (vp >= 50u && l->ev_pitch_hi >= 600u && l->ev_pitch_hi <= 3500u) return LS_WHISTLE;
    return LS_SHOUT;
}

uint8_t ba_listen_sample(ba_listen_t *l, uint16_t code)
{
    /* DC follower with a 1 s time constant, started from the first sample after a pause; the signal is code minus DC */
    if (!l->seeded) { l->dc_q8 = (int32_t)code << 8; l->seeded = 1; }
    l->dc_q8 += (((int32_t)code << 8) - l->dc_q8) >> 13;
    int16_t x = (int16_t)((int32_t)code - (l->dc_q8 >> 8));
    l->hist[l->hn] = x; l->hn = (uint16_t)((l->hn + 1u) % BA_LISTEN_HIST);
    l->e_acc += (uint32_t)((int32_t)x * x);
    if ((x >= 0) != (l->last >= 0)) l->zc++;
    l->last = x;
    if (++l->fn < BA_LISTEN_FRAME) return LS_NONE;
    /* one frame done */
    uint32_t e = l->e_acc / BA_LISTEN_FRAME; uint16_t zc = l->zc;
    l->e_acc = 0; l->fn = 0; l->zc = 0; l->frames++;
    if (l->cooldown) { l->cooldown--; return LS_NONE; }
    if (l->warm) {   /* the first 256 ms after a pause only learn the floor */
        l->warm--;
        if (e < l->floor) l->floor = (l->floor * 3u + e) / 4u; else l->floor += (e - l->floor) / 8u + 1u;
        return LS_NONE;
    }
    if (!l->in_event) {
        if (e > l->floor * START_X && e > ABS_MIN) {
            l->in_event = 1; l->quiet = 0; l->ev_frames = 0; l->ev_zc_sum = 0; l->ev_pitch_n = 0; l->ev_voiced = 0;
            l->ev_pitch_hi = 0; l->ev_peak = 0;
        } else {   /* learn the floor: fall fast, rise slowly */
            if (e < l->floor) l->floor = (l->floor * 3u + e) / 4u; else l->floor += (e - l->floor) / 64u + 1u;
            return LS_NONE;
        }
    }
    l->ev_frames++;
    l->ev_zc_sum = (uint16_t)(l->ev_zc_sum + zc);
    if (e > l->ev_peak) l->ev_peak = e;
    if (l->ev_frames % 3u == 0u) {   /* pitch check on the last 384 samples, in time order */
        static int16_t fr[BA_YIN_FRAME];
        for (unsigned i = 0; i < BA_YIN_FRAME; i++) fr[i] = l->hist[(l->hn + i) % BA_LISTEN_HIST];
        ba_yin_result_t r; ba_yin(fr, 8000u, 4915u, &r);
        l->ev_pitch_n++;
        if (r.voiced) { l->ev_voiced++; uint16_t hz = (uint16_t)(r.freq_x256 >> 8); if (hz > l->ev_pitch_hi) l->ev_pitch_hi = hz; }
    }
    if (e < l->floor * END_X || e < ABS_MIN / 2u) l->quiet++; else l->quiet = 0;
    if (l->quiet >= 3u || l->ev_frames >= MAX_FRAMES) {
        if (l->quiet >= 3u && l->ev_frames > 3u) l->ev_frames = (uint16_t)(l->ev_frames - 3u);
        l->in_event = 0;
        l->cooldown = COOLDOWN;
        return classify(l);
    }
    return LS_NONE;
}
