/* Touch gesture layer: tap, long press and swipe direction from a stream of (down, x, y, time) samples.
 * Portable C, no hardware, no allocation. Feed it one sample per touch poll. */
#ifndef GESTURE_H
#define GESTURE_H
#include <stdint.h>

#ifndef GEST_SLOP_PX
#define GEST_SLOP_PX   14    /* movement under this is still a tap or a hold */
#endif
#ifndef GEST_LONG_MS
#define GEST_LONG_MS   500   /* a hold this long, without moving, is a long press (fires while held) */
#endif
#ifndef GEST_SWIPE_PX
#define GEST_SWIPE_PX  36    /* a drag this far is a swipe (fires as soon as it is reached, finger still down) */
#endif

typedef enum { G_NONE = 0, G_TAP, G_LONG, G_SWIPE_L, G_SWIPE_R, G_SWIPE_U, G_SWIPE_D } gest_kind_t;

typedef struct {
    gest_kind_t kind;
    int16_t x, y;            /* where the finger first touched */
    uint32_t t_sample_us;    /* hal_us() of the sample that produced the event, set by the caller if it wants latency */
} gest_evt_t;

typedef struct {
    uint8_t  down, fired, moved;
    int16_t  x0, y0;
    uint32_t t0;
} gesture_t;

void gesture_init(gesture_t *g);
/* Returns 1 and fills *e when the sample completes a gesture, else 0.
 * Tap: finger lifts without having moved past the slop and before a long press fired.
 * Long press: held still for GEST_LONG_MS; fires once, the later lift gives no tap.
 * Swipe: finger travels GEST_SWIPE_PX from where it landed; direction is the larger axis; fires once. */
int gesture_feed(gesture_t *g, int down, int x, int y, uint32_t now_ms, gest_evt_t *e);
#endif
