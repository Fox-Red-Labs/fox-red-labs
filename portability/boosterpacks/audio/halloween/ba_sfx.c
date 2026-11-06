#include "ba_sfx.h"
#include "ba_sine_table.h"

uint32_t ba_rand(uint32_t *s) { uint32_t x = *s ? *s : 0x9E3779B9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
int16_t ba_sin8(uint32_t ph) { return ba_sine_tab[ph >> 24]; }
static int32_t noise(uint32_t *s) { return (int32_t)(ba_rand(s) >> 16) - 32768; }
/* phase step for f in Hz at the sample rate, 32-bit phase */
static uint32_t step_hz(uint32_t f_x16, uint32_t rate) { return (uint32_t)(((uint64_t)f_x16 << 28) / rate); } /* f in 1/16 Hz */

static const char *const names[SFX_COUNT] = { "none", "thunder", "wind", "creaking door", "heartbeat", "witch cackle", "scream", "ghost wail", "zap", "blip" };
const char *ba_sfx_name(uint8_t t) { return t < SFX_COUNT ? names[t] : "?"; }

void ba_sfx_init(ba_sfx_t *x, uint32_t rate, uint32_t seed)
{
    uint8_t *p = (uint8_t *)x; for (unsigned i = 0; i < sizeof *x; i++) p[i] = 0;
    x->rate = rate; x->rng = seed | 1u;
}

void ba_sfx_start(ba_sfx_t *x, uint8_t type)
{
    static const uint16_t len_ms[SFX_COUNT] = { 0, 2600, 3000, 1600, 2400, 1700, 1300, 2200, 350, 70 };
    x->type = type < SFX_COUNT ? type : SFX_NONE;
    x->t = 0; x->len = x->rate * len_ms[x->type] / 1000u;
    x->lp1 = x->lp2 = x->lp3 = 0; x->ph = x->ph2 = 0; x->flutter = 256;
}

void ba_sfx_blip(ba_sfx_t *x, uint16_t hz) { ba_sfx_start(x, SFX_BLIP); x->ph2 = step_hz((uint32_t)hz * 16u, x->rate); }

int16_t ba_sfx_next(ba_sfx_t *x)
{
    if (x->type == SFX_NONE) return 0;
    if (x->t >= x->len) { x->type = SFX_NONE; return 0; }
    uint32_t t = x->t++, len = x->len, r = x->rate;
    int32_t env = (int32_t)((uint64_t)(len - t) * 256u / len);  /* 256 down to 0 */
    int32_t out = 0;
    switch (x->type) {
    case SFX_THUNDER: {   /* a crack, then a low rumble that flutters as it dies */
        int32_t n = noise(&x->rng);
        x->lp1 += (n - x->lp1) >> 4;              /* about 80 Hz corner at 8 kHz */
        x->lp2 += (x->lp1 - x->lp2) >> 3;
        if ((t & 255u) == 0) x->flutter = 128 + (int32_t)(ba_rand(&x->rng) >> 24);   /* 128..383 */
        int32_t crack = t < r / 12u ? n * (int32_t)(r / 12u - t) / (int32_t)(r / 12u) : 0;
        out = (x->lp2 * 5 * x->flutter >> 8) * env >> 8;
        out += crack / 2;
        break; }
    case SFX_WIND: {      /* band-limited noise whose band slides up and down */
        int32_t n = noise(&x->rng);
        uint32_t lfo = (uint32_t)(ba_sin8(x->ph) + 32768) >> 12;   /* 0..15 */
        x->ph += step_hz(5, r);                                    /* 0.31 Hz sweep */
        int32_t k = 2 + (int32_t)(lfo >> 2);                        /* shift 2..5 */
        x->lp1 += (n - x->lp1) >> k;
        x->lp2 += (x->lp1 - x->lp2) >> (k + 2);
        int32_t bp = x->lp1 - x->lp2;
        int32_t a = t < r / 2u ? (int32_t)(t * 256u / (r / 2u)) : env;   /* fade in, fade out */
        out = bp * 3 * a >> 8;
        break; }
    case SFX_DOOR: {      /* a hinge: a train of clicks whose rate wanders, each ringing at about 700 Hz */
        uint32_t f = 18u + (uint32_t)((ba_sin8((uint32_t)((uint64_t)t * 0x60000000u / len)) + 32768) >> 11); /* 18..50 Hz */
        x->ph += step_hz(f * 16u, r);
        if (x->ph < step_hz(f * 16u, r)) x->lp3 = 22000;           /* a new click */
        x->ph2 += step_hz(700u * 16u, r);
        out = (x->ph2 & 0x80000000u ? 1 : -1) * x->lp3;
        x->lp3 = x->lp3 * 31 / 32;
        out = out * (env < 64 ? env * 4 : 256) >> 8;
        break; }
    case SFX_HEART: {     /* lub-dub, three beats at 75 a minute */
        uint32_t beat = r * 800u / 1000u, p = t % beat, l1 = r * 90u / 1000u, d0 = r * 190u / 1000u, l2 = r * 80u / 1000u;
        int32_t a = 0; uint32_t f = 0;
        if (p < l1) { a = (int32_t)((l1 - p) * 256u / l1); f = 55u; }
        else if (p >= d0 && p < d0 + l2) { a = (int32_t)((d0 + l2 - p) * 180u / l2); f = 45u; }
        if (a) { x->ph += step_hz(f * 16u, r); out = ba_sin8(x->ph) * a >> 8; }
        else x->ph = 0;
        break; }
    case SFX_CACKLE: {    /* "ha" six times: a buzzy pulse falling in pitch, with a fast wobble */
        uint32_t syl = r * 180u / 1000u, p = t % syl, n = t / syl, on = r * 120u / 1000u;
        if (p < on && n < 8u) {
            uint32_t f = 950u - n * 70u - p * 200u / on;           /* Hz */
            int32_t wob = ba_sin8(x->ph2) >> 9;                     /* +/-64 */
            x->ph2 += step_hz(14u * 16u, r);
            x->ph += step_hz((uint32_t)((int32_t)f * 16 + wob * 8), r);
            int32_t a = (int32_t)(p < on / 6u ? p * 256u / (on / 6u) : (on - p) * 256u / (on - on / 6u));
            out = ((x->ph >> 30) == 0 ? 1 : -1) * 16000 * a >> 8;  /* 25 percent pulse */
        }
        break; }
    case SFX_SCREAM: {    /* up from 600 Hz to 1,400 Hz and down, fast vibrato, some breath noise */
        uint32_t half = len / 3u;
        uint32_t f = t < half ? 600u + 800u * t / half : 1400u - 900u * (t - half) / (len - half);
        int32_t vib = ba_sin8(x->ph2) >> 8;                         /* +/-128 */
        x->ph2 += step_hz(9u * 16u, r);
        x->ph += step_hz((uint32_t)((int32_t)f * 16 + vib * 4), r);
        int32_t saw = (int32_t)(x->ph >> 16) - 32768;
        int32_t a = t < r / 20u ? (int32_t)(t * 256u * 20u / r) : env;
        out = (saw * 3 / 4 + (noise(&x->rng) >> 2)) * a >> 8;
        break; }
    case SFX_WAIL: {      /* a ghost: a triangle that bends up and slides down, slow vibrato */
        uint32_t f = t < len / 3u ? 300u + 500u * t / (len / 3u) : 800u - 550u * (t - len / 3u) / (len - len / 3u);
        int32_t vib = ba_sin8(x->ph2) >> 8;
        x->ph2 += step_hz(5u * 16u, r);
        x->ph += step_hz((uint32_t)((int32_t)f * 16 + vib * 3), r);
        int32_t tri = (int32_t)(x->ph >> 16); tri = tri < 32768 ? tri * 2 - 32768 : (65535 - tri) * 2 - 32768;
        int32_t a = t < r / 4u ? (int32_t)(t * 256u * 4u / r) : env;
        out = tri * 3 / 4 * a >> 8;
        break; }
    case SFX_BLIP: {      /* a short square beep at the pitch set by ba_sfx_blip */
        x->ph += x->ph2;
        out = ((x->ph & 0x80000000u) ? 16000 : -16000) * (env < 64 ? env * 4 : 256) >> 8;
        break; }
    case SFX_ZAP: {       /* a falling sweep, 2 kHz to 100 Hz */
        uint32_t f = 2000u - 1900u * t / len;
        x->ph += step_hz(f * 16u, r);
        out = ((x->ph & 0x80000000u) ? 14000 : -14000) * env >> 8;
        break; }
    default: break;
    }
    if (out > 24000) out = 24000;
    if (out < -24000) out = -24000;
    return (int16_t)out;
}

void ba_wind_init(ba_wind_t *w, uint32_t rate, uint32_t seed) { w->rng = seed | 1u; w->t = 0; w->lp1 = w->lp2 = 0; w->rate = rate; }

int16_t ba_wind_next(ba_wind_t *w)
{
    int32_t n = noise(&w->rng);
    uint32_t cyc = w->rate * 6u;                                   /* a 6 s gust cycle */
    uint32_t ph = (uint32_t)((uint64_t)(w->t % cyc) * 0xFFFFFFFFu / cyc);
    w->t++;
    int32_t g = (ba_sin8(ph) + 32768) >> 8;                        /* 0..255 */
    int32_t k = 3 + (g >> 7);                                      /* the band moves with the gust */
    w->lp1 += (n - w->lp1) >> k;
    w->lp2 += (w->lp1 - w->lp2) >> (k + 2);
    return (int16_t)(((w->lp1 - w->lp2) * (64 + g)) >> 7);
}
