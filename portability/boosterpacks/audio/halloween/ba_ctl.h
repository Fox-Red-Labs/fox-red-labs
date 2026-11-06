/* Two-button controls for the Halloween show, read at the light frame rate (100 a second). No flash writes: every
 * setting starts at its default at power-up.
 *   A short: next song          B short: next light theme
 *   A long (0.7 s): next effect mode   B long: volume step (4, 6, ... 16, then 4)
 *   A and B held together, let go after 0.3 to 2 s: shuffle on or off; after 2 to 5 s: speaker EQ on or off (the
 *   A/B comparison); after 5 s or more: listening on or off
 * Each change shows on the ring for 0.8 s as a count of LEDs, and the app prints it. */
#ifndef BA_CTL_H
#define BA_CTL_H
#include <stdint.h>
#include "ba_show.h"
#include "ba_lights.h"

enum { ACT_NONE, ACT_NEXT_SONG, ACT_NEXT_THEME, ACT_NEXT_FX, ACT_VOLUME, ACT_SHUFFLE, ACT_LISTEN, ACT_EQ };

typedef struct { uint16_t a_t, b_t, ab_t; uint8_t a_long, b_long, both; } ba_buttons_t;

uint8_t ba_buttons_frame(ba_buttons_t *k, int a_down, int b_down);   /* returns an ACT_ value */
/* apply an action; returns a short label ("theme"), and *name and *value say what it is now */
const char *ba_ctl_apply(uint8_t act, ba_show_t *s, ba_lights_t *l, volatile uint8_t *vol, const char **name, uint32_t *value);
extern volatile uint8_t ba_ctl_listening;   /* the listener's on/off switch, on at power-up */

#endif
