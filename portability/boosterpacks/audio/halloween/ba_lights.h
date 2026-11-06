/* Light engine for the Halloween show: the synth's own note events (ba_show_events_t) become colors for an 8-LED ring
 * and on/off for two board LEDs. No microphone. Call ba_lights_frame() BA_LIGHTS_FPS times a second.
 * Themes:
 *   music      drum hits flash white, the bass note picks the hue (pitch class x 30 degrees), the lead note is a
 *              head at (note x 8 / 12) mod 8 with a fading trail: a scale walks the ring, an octave is one lap
 *   pumpkin    orange and purple LEDs alternate; the pattern steps one place on every drum hit
 *   fire       random orange flicker, brighter when the music is louder
 *   lightning  dark; a white strobe on kick and snare, and bursts all through a thunder sound
 *   ghost      slow pale green-white fades that follow the melody
 *   heartbeat  a red double pulse on every kick (75 a minute when there is none)
 *   witch      green and purple swirling round the ring, a hue swing on a single LED
 * Between songs every theme shows a candle: warm, calm, a slow flicker.
 * Board LEDs: red on for 60 ms after a kick or snare, green on for 60 ms after a new bass note.
 * After a control change the ring shows a count for 0.8 s (ba_lights_count); a single RGB LED blinks that count. */
#ifndef BA_LIGHTS_H
#define BA_LIGHTS_H
#include <stdint.h>
#include "ba_show.h"

#define BA_RING_N 8
#define BA_LIGHTS_FPS 100u
#define BA_LIGHTS_MAX 64u  /* per color channel, of 255: about 15 mA a LED at most, 120 mA for the ring */

enum { TH_MUSIC, TH_PUMPKIN, TH_FIRE, TH_LIGHTNING, TH_GHOST, TH_HEARTBEAT, TH_WITCH, TH_COUNT };

typedef struct {
    uint8_t rgb[BA_RING_N][3];  /* index 0 is the ring's first LED (D1) */
    uint8_t red, green;         /* board LEDs, 1 = on */
    uint8_t led[3];             /* one RGB LED, 0..255 a channel: the theme on a single light (LP-MSPM33C321A) */
    uint8_t theme;
    /* state */
    uint32_t lead_seen, bass_seen, drum_seen, sfx_seen, rng;
    uint16_t flash, trail[BA_RING_N], bass_glow, hue, lead_hue;
    uint16_t fire[BA_RING_N];
    uint8_t chase, strobe, red_left, green_left;
    uint16_t thunder_left, beat_t, no_kick;
    uint16_t count_left; uint8_t count_n, count_rgb[3];
    uint32_t frame, gap_frame;
} ba_lights_t;

void ba_lights_init(ba_lights_t *l, uint32_t seed);
void ba_lights_frame(ba_lights_t *l, const ba_show_events_t *ev);
void ba_lights_count(ba_lights_t *l, uint8_t n, uint8_t r, uint8_t g, uint8_t b);  /* show n LEDs for 0.8 s */
const char *ba_theme_name(uint8_t t);
void ba_hsv(uint16_t hue, uint8_t sat, uint8_t val, uint8_t rgb[3]);

#endif
