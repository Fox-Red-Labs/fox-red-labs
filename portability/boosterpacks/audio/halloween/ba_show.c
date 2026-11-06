#include "ba_show.h"

int16_t ba_soft_limit(int32_t x, volatile uint32_t *count)
{
    /* below 20000 the signal passes unchanged; above, the excess is squeezed so that no input reaches past 32767:
     * y = 20000 + 12767 * e / (e + 12767), e = |x| - 20000 (a smooth knee, no hard clip) */
    int32_t a = x < 0 ? -x : x;
    if (a <= 20000) return (int16_t)x;
    if (count) (*count)++;
    int32_t e = a - 20000;
    int32_t y = 20000 + (int32_t)((int64_t)12767 * e / (e + 12767));
    return (int16_t)(x < 0 ? -y : y);
}

static const char *const fx_names[FX_COUNT] = { "clean", "haunted", "graveyard", "mad lab" };
const char *ba_fx_name(uint8_t m) { return m < FX_COUNT ? fx_names[m] : "?"; }

/* 2^(-k/12) in Q16 for k = 0, 1, 2 */
static const uint32_t shift_q16[3] = { 65536u, 61858u, 58386u };

static uint8_t total(const ba_show_t *s) { uint32_t t = (uint32_t)s->n + s->ntracks; return (uint8_t)(t > BA_SHOW_MAX_SONGS ? BA_SHOW_MAX_SONGS : t); }

const char *ba_show_title(const ba_show_t *s, uint8_t idx)
{
    if (idx < s->n) return s->list[idx].name;
    if ((uint8_t)(idx - s->n) < s->ntracks) return s->tracks[idx - s->n].name;
    return "?";
}

static void make_order(ba_show_t *s)
{
    uint8_t N = total(s);
    for (uint8_t i = 0; i < N; i++) s->order[i] = i;
    if (!s->shuffle) return;
    for (uint8_t i = (uint8_t)(N - 1u); i > 0; i--) {
        uint8_t j = (uint8_t)(ba_rand(&s->rng) % (i + 1u));
        uint8_t t = s->order[i]; s->order[i] = s->order[j]; s->order[j] = t;
    }
    if (s->first < N && s->idx == 0xFF) {   /* the first pass after power-up opens with the chosen song */
        for (uint8_t i = 0; i < N; i++) if (s->order[i] == s->first) { s->order[i] = s->order[0]; s->order[0] = s->first; }
        return;
    }
    /* never repeat the song that just played */
    if (N > 1 && s->order[0] == s->idx) { uint8_t t = s->order[0]; s->order[0] = s->order[1]; s->order[1] = t; }
}

static uint32_t row_len(const ba_show_t *s, uint16_t tempo)
{
    return s->rate * 30u / tempo * 100u / s->tempo_pct;
}

static void start_song(ba_show_t *s)
{
    s->idx = s->order[s->pos];
    s->ev.song = s->idx;
    s->ev.in_gap = 0;
    if (s->idx >= s->n) {   /* a recorded track */
        ba_adpcm_start(&s->player, &s->tracks[s->idx - s->n], s->rate);
        s->key_shift = 0; s->tempo_pct = 100; s->swap = 0;
        s->on_acc = 0; s->on_avg = 2000; s->on_n = 0; s->on_count = 0;
        s->ev.row_samples = s->rate / 8u;
        s->ev.song_seq++;
        return;
    }
    const ba_show_song_t *g = &s->list[s->idx];
    s->key_shift = (int8_t)-(int8_t)(ba_rand(&s->rng) % 3u);
    s->tempo_pct = (uint8_t)(95u + ba_rand(&s->rng) % 11u);
    s->swap = (uint8_t)(ba_rand(&s->rng) & 1u);
    s->cur.rows = g->rows;
    s->cur.nrows = g->nrows;
    s->cur.samples_per_row = row_len(s, g->tempo);
    for (int v = 0; v < BA_VOICES; v++) s->cur.vol[v] = g->vol[v];
    if (s->swap) { uint8_t t = s->cur.vol[0]; s->cur.vol[0] = s->cur.vol[1]; s->cur.vol[1] = t; }
    ba_chip_init(&s->chip, &s->cur, s->rate);
    for (int v = 0; v < 3; v++) s->logical[v] = s->base[v] = 0;
    s->rows_played = 0;
    s->mark_i = 0;
    s->ev.song = s->idx;
    s->ev.in_gap = 0;
    s->ev.row_samples = s->cur.samples_per_row;
    s->ev.song_seq++;
}

void ba_show_init(ba_show_t *s, const ba_show_song_t *list, uint8_t n, uint32_t rate_hz, uint32_t gap_ms, uint32_t seed)
{
    uint8_t *p = (uint8_t *)s; for (unsigned i = 0; i < sizeof *s; i++) p[i] = 0;
    s->list = list;
    s->n = n > BA_SHOW_MAX_SONGS ? BA_SHOW_MAX_SONGS : n;
    s->rate = rate_hz;
    s->rng = seed | 1u;
    s->gap_samples = rate_hz * gap_ms / 1000u;
    s->shuffle = 1;
    s->first = BA_SHOW_FIRST_SONG;
    s->gain_q8 = BA_SHOW_GAIN_Q8;
    ba_eq_init(&s->eq);
    s->eq_on = (rate_hz == 16000u);   /* the EQ was fitted at 16 kHz */
    s->idx = 0xFF;
    ba_sfx_init(&s->sfx, rate_hz, seed * 2654435761u + 1u);
    ba_wind_init(&s->wind, rate_hz, seed ^ 0x5bd1e995u);
    make_order(s);
    start_song(s);
}

void ba_show_add_tracks(ba_show_t *s, const ba_track_t *t, uint8_t nt, const char *first_title)
{
    s->tracks = t; s->ntracks = nt;
    if (first_title) for (uint8_t i = 0; i < nt; i++) {
        const char *a = t[i].name, *b = first_title; int ok = 1;
        while (*b) { if (*a++ != *b++) { ok = 0; break; } }
        if (ok) { s->first = (uint8_t)(s->n + i); break; }
    }
    s->idx = 0xFF; s->pos = 0;
    make_order(s);
    start_song(s);
}

void ba_show_next_song(ba_show_t *s) { if (s->gap_left) s->gap_left = 1; else s->skip = 1; }
void ba_show_set_shuffle(ba_show_t *s, int on) { s->shuffle = (uint8_t)(on != 0); }
void ba_show_scare(ba_show_t *s, uint8_t sfx) { ba_sfx_start(&s->sfx, sfx); s->ev.sfx = sfx; s->ev.sfx_seq++; }

static void begin_gap(ba_show_t *s, int scare)
{
    s->ev.in_gap = 1;
    s->gap_len = scare ? s->gap_samples : s->rate / 2u;
    s->gap_left = s->gap_len ? s->gap_len : 1u;
    if (scare) {
        static const uint8_t pick[] = { SFX_THUNDER, SFX_DOOR, SFX_HEART, SFX_CACKLE, SFX_WAIL, SFX_WIND };
        ba_show_scare(s, pick[ba_rand(&s->rng) % sizeof pick]);
    }
}

static void next_in_order(ba_show_t *s)
{
    if (++s->pos >= total(s)) { s->pos = 0; make_order(s); }
    start_song(s);
}

/* the synth is about to load row r: tempo marks, then events */
static void row_events(ba_show_t *s, uint16_t r)
{
    const ba_show_song_t *g = &s->list[s->idx];
    while (g->marks && s->mark_i < g->nmarks && g->marks[s->mark_i][0] <= r) {
        s->cur.samples_per_row = row_len(s, g->marks[s->mark_i][1]);
        s->ev.row_samples = s->cur.samples_per_row;
        s->mark_i++;
    }
    const uint8_t *row = g->rows[r];
    if (row[0] >= 12) { s->ev.lead_note = (uint8_t)(row[0] + s->key_shift); s->ev.lead_seq++; }
    if (row[2] >= 12) { s->ev.bass_note = (uint8_t)(row[2] + s->key_shift); s->ev.bass_seq++; }
    if (row[3] >= BA_DRUM_KICK) { s->ev.drum = row[3]; s->ev.drum_seq++; }
}

/* a recorded track: decode, and turn sudden rises in loudness into drum events so the lights follow the record */
static int16_t track(ba_show_t *s)
{
    if (s->skip || s->player.done) { begin_gap(s, !s->skip); s->skip = 0; return 0; }
    int16_t x = ba_adpcm_next(&s->player);
    s->on_acc += (uint32_t)(x < 0 ? -x : x);
    if (++s->on_n >= s->rate / 100u) {            /* 10 ms blocks */
        uint32_t b = s->on_acc / s->on_n;
        if (b > s->on_avg * 8u / 5u && b > 1500u) {
            s->ev.drum = (b > s->on_avg * 3u) ? BA_DRUM_KICK : BA_DRUM_SNARE; s->ev.drum_seq++;
            s->ev.lead_note = (uint8_t)(60u + ba_rand(&s->rng) % 24u); s->ev.lead_seq++;
            if ((++s->on_count & 3u) == 0u) { s->ev.bass_note = (uint8_t)(36u + ba_rand(&s->rng) % 12u); s->ev.bass_seq++; }
        }
        s->on_avg = (s->on_avg * 15u + b) / 16u;
        s->on_acc = 0; s->on_n = 0;
    }
    return x;
}

static int16_t music(ba_show_t *s)
{
    if (s->idx >= s->n) return track(s);
    int lead = s->swap ? 1 : 0;
    if (s->chip.sample_in_row >= s->cur.samples_per_row) {           /* the synth loads a new row on this call */
        if (s->skip || s->rows_played >= s->cur.nrows) {             /* the song is over */
            begin_gap(s, !s->skip);
            s->skip = 0;
            return 0;
        }
        for (int v = 0; v < 3; v++) s->chip.step[v] = s->logical[v];  /* the chip holds notes in its own terms */
        row_events(s, s->rows_played);
        s->rows_played++;
        s->chip.sample_in_row = s->cur.samples_per_row;               /* load now, even if a tempo mark lengthened the row */
        int16_t x = ba_chip_next(&s->chip);
        for (int v = 0; v < 3; v++) {
            s->logical[v] = s->chip.step[v];
            s->base[v] = (uint32_t)(((uint64_t)s->chip.step[v] * shift_q16[-s->key_shift]) >> 16);
        }
        /* the swap: the lead's notes go to the 50 percent voice, the harmony's to the 25 percent voice */
        s->chip.step[0] = s->base[s->swap ? 1 : 0];
        s->chip.step[1] = s->base[s->swap ? 0 : 1];
        s->chip.step[2] = s->base[2];
        uint32_t b0 = s->base[0], b1 = s->base[1]; s->base[0] = s->swap ? b1 : b0; s->base[1] = s->swap ? b0 : b1;
        return x;
    }
    if (s->skip) { s->chip.sample_in_row = s->cur.samples_per_row; return 0; }   /* end at the next call */
    uint8_t full = s->list[s->idx].vol[0];                           /* the lead's own volume, on whichever voice */
    if (s->fxmode != FX_HAUNTED) s->cur.vol[lead] = full;
    else if (s->base[lead]) {
        /* vibrato 5.5 Hz, about +/-0.6 percent; tremolo 4 Hz on the lead's volume */
        s->lfo_v += (uint32_t)(((uint64_t)55u << 32) / 10u / s->rate);
        s->lfo_t += (uint32_t)(((uint64_t)4u << 32) / s->rate);
        int32_t v = ba_sin8(s->lfo_v);
        s->chip.step[lead] = (uint32_t)((int64_t)s->base[lead] + (((int64_t)s->base[lead] * v) >> 22));
        int32_t tr = (ba_sin8(s->lfo_t) + 32768) >> 13;               /* 0..7 */
        int32_t vv = (int32_t)full - (int32_t)full * tr / 16;
        s->cur.vol[lead] = (uint8_t)(vv < 0 ? 0 : vv);
    }
    return ba_chip_next(&s->chip);
}

int16_t ba_show_next(ba_show_t *s)
{
    int32_t x;
    if (s->gap_left) {
        if (--s->gap_left) x = 0;
        else { next_in_order(s); x = music(s); }
    } else x = music(s);
    /* effect modes */
    if (s->fxmode == FX_GRAVEYARD) x += ba_wind_next(&s->wind) / 3;
    if (s->fxmode == FX_MADLAB && !s->ev.in_gap) {
        if (s->zap_left == 0) { s->zap_left = s->rate * 4u; ba_show_scare(s, SFX_ZAP); }
        s->zap_left--;
    }
    x += ba_sfx_next(&s->sfx);
    if (s->fxmode == FX_HAUNTED) {
        int32_t d = s->echo[s->echo_i];
        int32_t y = x + d * 5 / 8;                                   /* echo, 250 ms, wet 5/8 */
        int32_t fb = x + d / 2;                                      /* feedback one half */
        if (fb > 32767) fb = 32767; if (fb < -32768) fb = -32768;
        s->echo[s->echo_i] = (int16_t)fb;
        if (++s->echo_i >= (s->rate / 4u < BA_ECHO_N ? s->rate / 4u : BA_ECHO_N)) s->echo_i = 0;
        x = y * 3 / 4;
    }
    if (s->fxmode == FX_MADLAB) {                                     /* 5-bit levels, held for 3 samples */
        if (s->crush_n == 0) s->crush_hold = (int16_t)((x >> 11) << 11);
        if (++s->crush_n >= 3) s->crush_n = 0;
        x = s->crush_hold;
    }
    if (s->eq_on && s->rate == 16000u) x = ba_eq_next(&s->eq, x);
    x = ba_soft_limit((x * (int32_t)s->gain_q8) >> 8, &s->limited);
    int32_t a = x < 0 ? -x : x;
    if (a > s->env) s->env = a; else s->env -= (s->env >> 9) + 1;
    if (s->env < 0) s->env = 0;
    s->ev.level = (uint16_t)s->env;
    return (int16_t)x;
}
