#include "ba_ctl.h"

#define LONG_F 70u   /* 0.7 s */
#define BOTH_F 30u
#define EQ_F 200u      /* 2 s */
#define LISTEN_F 500u  /* 5 s */
volatile uint8_t ba_ctl_listening = 1;
#define MIN_F 3u     /* 30 ms debounce */

uint8_t ba_buttons_frame(ba_buttons_t *k, int a, int b)
{
    uint8_t act = ACT_NONE;
    if (a && b) {
        if (++k->ab_t >= BOTH_F) k->both = 1;
    } else {
        if (k->ab_t >= LISTEN_F) act = ACT_LISTEN; else if (k->ab_t >= EQ_F) act = ACT_EQ; else if (k->ab_t >= BOTH_F) act = ACT_SHUFFLE;   /* on release */
        k->ab_t = 0;
    }
    if (a) {
        if (++k->a_t == LONG_F && !k->both && !b) { act = ACT_NEXT_FX; k->a_long = 1; }
    } else {
        if (k->a_t >= MIN_F && k->a_t < LONG_F && !k->a_long && !k->both) act = ACT_NEXT_SONG;
        k->a_t = 0; k->a_long = 0;
    }
    if (b) {
        if (++k->b_t == LONG_F && !k->both && !a) { act = ACT_VOLUME; k->b_long = 1; }
    } else {
        if (k->b_t >= MIN_F && k->b_t < LONG_F && !k->b_long && !k->both) act = ACT_NEXT_THEME;
        k->b_t = 0; k->b_long = 0;
    }
    if (!a && !b) k->both = 0;
    return act;
}

const char *ba_ctl_apply(uint8_t act, ba_show_t *s, ba_lights_t *l, volatile uint8_t *vol, const char **name, uint32_t *value)
{
    *name = ""; *value = 0;
    switch (act) {
    case ACT_NEXT_SONG:
        ba_show_next_song(s);
        ba_lights_count(l, 1, 64, 64, 64);
        return "next song";
    case ACT_NEXT_THEME:
        l->theme = (uint8_t)((l->theme + 1u) % TH_COUNT);
        ba_lights_count(l, (uint8_t)(l->theme + 1u), 40, 0, 64);
        *name = ba_theme_name(l->theme); *value = l->theme;
        return "theme";
    case ACT_NEXT_FX:
        s->fxmode = (uint8_t)((s->fxmode + 1u) % FX_COUNT);
        ba_lights_count(l, (uint8_t)(s->fxmode + 1u), 0, 64, 0);
        *name = ba_fx_name(s->fxmode); *value = s->fxmode;
        return "effects";
    case ACT_VOLUME:
        *vol = (uint8_t)(*vol >= 16u ? 4u : *vol + 2u);
        ba_lights_count(l, (uint8_t)(*vol / 2u), 0, 20, 64);
        *name = "of 16"; *value = *vol;
        return "volume";
    case ACT_SHUFFLE:
        ba_show_set_shuffle(s, !s->shuffle);
        if (s->shuffle) ba_lights_count(l, 8, 0, 64, 0); else ba_lights_count(l, 8, 64, 0, 0);
        *name = s->shuffle ? "on" : "off"; *value = s->shuffle;
        return "shuffle";
    case ACT_EQ:
        s->eq_on = (uint8_t)!s->eq_on;
        if (s->eq_on) ba_lights_count(l, 8, 64, 40, 0); else ba_lights_count(l, 1, 64, 40, 0);
        *name = s->eq_on ? "on" : "off"; *value = s->eq_on;
        return "speaker EQ";
    case ACT_LISTEN:
        ba_ctl_listening = (uint8_t)!ba_ctl_listening;
        if (ba_ctl_listening) ba_lights_count(l, 8, 0, 40, 64); else ba_lights_count(l, 2, 40, 40, 40);
        *name = ba_ctl_listening ? "on" : "off"; *value = ba_ctl_listening;
        return "listening";
    default: return 0;
    }
}
