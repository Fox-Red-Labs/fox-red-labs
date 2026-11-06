#include "ba_lights.h"

static const char *const theme_names[TH_COUNT] = { "music", "pumpkin", "fire", "lightning", "ghost", "heartbeat", "witch" };
const char *ba_theme_name(uint8_t t) { return t < TH_COUNT ? theme_names[t] : "?"; }

/* hue 0 to 359, sat and val 0 to 255, integer only */
void ba_hsv(uint16_t hue, uint8_t sat, uint8_t val, uint8_t rgb[3])
{
    hue %= 360u;
    uint32_t sector = hue / 60u, f = (hue % 60u) * 255u / 60u;
    uint32_t p = (uint32_t)val * (255u - sat) / 255u;
    uint32_t q = (uint32_t)val * (255u - sat * f / 255u) / 255u;
    uint32_t t = (uint32_t)val * (255u - sat * (255u - f) / 255u) / 255u;
    uint32_t r, g, b;
    switch (sector) {
    case 0: r = val; g = t; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = t; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = t; g = p; b = val; break;
    default: r = val; g = p; b = q; break;
    }
    rgb[0] = (uint8_t)r; rgb[1] = (uint8_t)g; rgb[2] = (uint8_t)b;
}

void ba_lights_init(ba_lights_t *l, uint32_t seed)
{
    uint8_t *p = (uint8_t *)l;
    for (unsigned i = 0; i < sizeof *l; i++) p[i] = 0;
    l->hue = 270;
    l->rng = seed | 1u;
    l->theme = TH_PUMPKIN;   /* the brightest theme opens the show */
}

void ba_lights_count(ba_lights_t *l, uint8_t n, uint8_t r, uint8_t g, uint8_t b)
{
    l->count_n = n > BA_RING_N ? BA_RING_N : n;
    l->count_left = (uint16_t)(BA_LIGHTS_FPS * 8u / 10u);
    l->count_rgb[0] = r; l->count_rgb[1] = g; l->count_rgb[2] = b;
}

static uint8_t cap(uint32_t v) { return (uint8_t)(v > BA_LIGHTS_MAX ? BA_LIGHTS_MAX : v); }
static void set(ba_lights_t *l, int i, uint32_t r, uint32_t g, uint32_t b) { l->rgb[i][0] = cap(r); l->rgb[i][1] = cap(g); l->rgb[i][2] = cap(b); }
static void hsv_cap(ba_lights_t *l, int i, uint16_t h, uint8_t s, uint32_t v) { uint8_t c[3]; ba_hsv(h, s, (uint8_t)(v > 255u ? 255u : v), c); set(l, i, c[0], c[1], c[2]); }

/* a smooth 2..256 breathing curve from a phase 0..255 */
static uint32_t breath(uint32_t ph)
{
    uint32_t x = ph & 255u;
    uint32_t tri = x < 128u ? x * 2u : (255u - x) * 2u;
    return tri * tri / 254u + 2u;
}

static void candle(ba_lights_t *l)
{
    uint32_t b = breath((l->gap_frame * 256u) / (3u * BA_LIGHTS_FPS));   /* a 3 s breath */
    for (int i = 0; i < BA_RING_N; i++) {
        int32_t f = (int32_t)l->fire[i] + (int32_t)(ba_rand(&l->rng) % 9u) - 4;
        if (f < 0) f = 0; if (f > 24) f = 24;
        l->fire[i] = (uint16_t)f;
        uint32_t v = (10u + b * 22u / 256u + (uint32_t)f / 2u) * BA_LIGHTS_MAX / 64u;
        hsv_cap(l, i, (uint16_t)(24u + (uint32_t)f / 3u), 255, v);
    }
    l->red = 0;
    l->green = (uint8_t)(b > 200u);
}

void ba_lights_frame(ba_lights_t *l, const ba_show_events_t *ev)
{
    l->frame++;
    int kick = 0, snare = 0, hat = 0, bass = 0, lead = 0, sfx = 0;
    if (ev->drum_seq != l->drum_seen) {
        l->drum_seen = ev->drum_seq;
        kick = ev->drum == BA_DRUM_KICK; snare = ev->drum == BA_DRUM_SNARE; hat = ev->drum == BA_DRUM_HAT;
        if (!hat) l->red_left = 6;
    }
    if (ev->bass_seq != l->bass_seen) { l->bass_seen = ev->bass_seq; bass = 1; l->green_left = 6; l->hue = (uint16_t)((ev->bass_note % 12u) * 30u); }
    if (ev->lead_seq != l->lead_seen) { l->lead_seen = ev->lead_seq; lead = 1; }
    if (ev->sfx_seq != l->sfx_seen) { l->sfx_seen = ev->sfx_seq; sfx = ev->sfx; }
    if (sfx == SFX_THUNDER) l->thunder_left = (uint16_t)(BA_LIGHTS_FPS * 25u / 10u);
    if (sfx == SFX_SCREAM) l->thunder_left = (uint16_t)(BA_LIGHTS_FPS * 12u / 10u);
    unsigned pos = ((unsigned)ev->lead_note * BA_RING_N / 12u) % BA_RING_N;

    if (ev->in_gap) {
        candle(l);
        l->gap_frame++;
        l->flash = 0; l->bass_glow = 0;
        for (int i = 0; i < BA_RING_N; i++) l->trail[i] = 0;
        if (l->thunder_left) {                      /* lightning still answers a thunder or a scream in a gap */
            l->thunder_left--;
            if ((ba_rand(&l->rng) & 15u) == 0) for (int i = 0; i < BA_RING_N; i++) set(l, i, 64, 64, 64);
        }
        goto overlay;
    }
    l->gap_frame = 0;
    switch (l->theme) {
    default:
    case TH_MUSIC: {
        if (kick || snare || hat) { uint16_t k = kick ? 256 : snare ? 180 : 52; if (k > l->flash) l->flash = k; }
        if (bass) l->bass_glow = 256;
        if (lead) { l->trail[pos] = 256; l->lead_hue = (uint16_t)(l->hue + 180u); }
        uint8_t bg[3], hd[3];
        uint32_t bgv = 12u + (uint32_t)l->bass_glow * 16u / 256u;        /* 12 to 28 of 64 */
        ba_hsv(l->hue, 255, (uint8_t)(bgv * BA_LIGHTS_MAX / 64u), bg);
        for (int i = 0; i < BA_RING_N; i++) {
            ba_hsv(l->lead_hue, 200, (uint8_t)(BA_LIGHTS_MAX * (uint32_t)l->trail[i] / 256u), hd);
            uint32_t w = (uint32_t)BA_LIGHTS_MAX * l->flash / 256u;
            set(l, i, bg[0] + hd[0] + w, bg[1] + hd[1] + w, bg[2] + hd[2] + w);
        }
        l->flash = (uint16_t)(l->flash * 8u / 10u);
        l->bass_glow = (uint16_t)(l->bass_glow * 95u / 100u);
        for (int i = 0; i < BA_RING_N; i++) l->trail[i] = (uint16_t)(l->trail[i] * 93u / 100u);
        break; }
    case TH_PUMPKIN: {
        if (kick || snare) { l->chase = (uint8_t)((l->chase + 1u) % BA_RING_N); l->flash = kick ? 256 : 160; }
        for (int i = 0; i < BA_RING_N; i++) {
            int odd = ((i + l->chase) & 1);
            uint32_t v = (36u + 28u * l->flash / 256u) * 255u / 64u;
            hsv_cap(l, i, odd ? 275 : 28, 255, v * BA_LIGHTS_MAX / 255u);
        }
        l->flash = (uint16_t)(l->flash * 85u / 100u);
        break; }
    case TH_FIRE: {
        uint32_t loud = ev->level;                                      /* 0..32000 */
        for (int i = 0; i < BA_RING_N; i++) {
            uint32_t target = 8u + (uint32_t)(ba_rand(&l->rng) % (8u + loud / 600u));   /* 8 .. about 70 */
            l->fire[i] = (uint16_t)((l->fire[i] * 3u + target) / 4u);
            uint16_t hue = (uint16_t)(10u + ba_rand(&l->rng) % 28u);
            hsv_cap(l, i, hue, 255, (uint32_t)l->fire[i] * 4u);
        }
        break; }
    case TH_LIGHTNING: {
        if (kick || snare) l->strobe = 3;
        int on = 0;
        if (l->strobe) { on = (l->strobe != 2); l->strobe--; }               /* flash, dark, flash */
        if (l->thunder_left) { l->thunder_left--; if ((ba_rand(&l->rng) & 7u) == 0) on = 1; }
        for (int i = 0; i < BA_RING_N; i++) { if (on) set(l, i, 64, 64, 64); else set(l, i, 0, 0, 2); }
        break; }
    case TH_GHOST: {
        if (lead) l->trail[pos] = 256;
        uint32_t b = breath((l->frame * 256u) / (5u * BA_LIGHTS_FPS));
        for (int i = 0; i < BA_RING_N; i++) {
            uint32_t v = 3u + b * 6u / 256u + (uint32_t)l->trail[i] * 44u / 256u;
            set(l, i, v * 6u / 10u, v, v * 7u / 10u);                         /* pale green-white */
            l->trail[i] = (uint16_t)(l->trail[i] * 97u / 100u);
        }
        break; }
    case TH_WITCH: {
        uint32_t ph = (l->frame * 256u) / (2u * BA_LIGHTS_FPS);
        if (lead) l->flash = 200;
        for (int i = 0; i < BA_RING_N; i++) {
            uint32_t b = breath(ph + (uint32_t)i * 32u);
            uint16_t hue = (uint16_t)(120u + b * 160u / 256u);              /* green 120 to purple 280 */
            hsv_cap(l, i, hue, 255, (20u + 30u * l->flash / 256u) * 4u);
        }
        l->flash = (uint16_t)(l->flash * 9u / 10u);
        break; }
    case TH_HEARTBEAT: {
        if (kick) { l->beat_t = 1; l->no_kick = 0; }
        else if (++l->no_kick > BA_LIGHTS_FPS * 3u / 2u && l->beat_t == 0 && l->frame % 80u == 0) l->beat_t = 1; /* 75 a minute */
        uint32_t v = 2;
        if (l->beat_t) {
            uint16_t t = l->beat_t++;
            if (t < 12) v = 64u * (12u - t) / 12u;                              /* lub */
            else if (t >= 18 && t < 28) v = 44u * (28u - t) / 10u;              /* dub */
            if (t >= 28) l->beat_t = 0;
        }
        for (int i = 0; i < BA_RING_N; i++) set(l, i, v, 0, v / 12u);
        l->red = (uint8_t)(v > 20u);
        break; }
    }
    if (l->theme != TH_HEARTBEAT) l->red = l->red_left ? 1 : 0;
    l->green = l->green_left ? 1 : 0;
overlay:
    {   /* the single LED: the brightest of the ring for music and ghost, else the first LED; scaled from 64 to 255 */
        int mx = (!ev->in_gap && (l->theme == TH_MUSIC || l->theme == TH_GHOST));
        for (int c = 0; c < 3; c++) {
            uint32_t v = l->rgb[0][c];
            if (mx) for (int i = 1; i < BA_RING_N; i++) if (l->rgb[i][c] > v) v = l->rgb[i][c];
            v = v * 255u / BA_LIGHTS_MAX; l->led[c] = (uint8_t)(v > 255u ? 255u : v);
        }
    }
    if (l->red_left) l->red_left--;
    if (l->green_left) l->green_left--;
    if (l->count_left) {
        l->count_left--;
        for (int i = 0; i < BA_RING_N; i++) {
            if (i < l->count_n) set(l, i, l->count_rgb[0], l->count_rgb[1], l->count_rgb[2]);
            else set(l, i, 0, 0, 0);
        }
        /* the single LED blinks the count: 120 ms on, 120 ms off, from the start of the 0.8 s window (up to 3 blinks
         * show in it; longer counts read on the ring and the console) */
        uint32_t k = (uint32_t)(BA_LIGHTS_FPS * 8u / 10u) - l->count_left, n = k / 12u;
        int on = (n % 2u == 0u) && (n / 2u < l->count_n);
        for (int c = 0; c < 3; c++) l->led[c] = on ? (uint8_t)(l->count_rgb[c] * 255u / BA_LIGHTS_MAX) : 0;
    }
}
