#include "gesture.h"

void gesture_init(gesture_t *g) { g->down = g->fired = g->moved = 0; g->x0 = g->y0 = 0; g->t0 = 0; }

static int iabs(int v) { return v < 0 ? -v : v; }

int gesture_feed(gesture_t *g, int down, int x, int y, uint32_t now, gest_evt_t *e)
{
    if (down) {
        if (!g->down) {                       /* finger just landed */
            g->down = 1; g->fired = g->moved = 0; g->x0 = (int16_t)x; g->y0 = (int16_t)y; g->t0 = now;
            return 0;
        }
        if (g->fired) return 0;
        int dx = x - g->x0, dy = y - g->y0;
        int ax = iabs(dx), ay = iabs(dy);
        if (ax >= GEST_SWIPE_PX || ay >= GEST_SWIPE_PX) {
            g->fired = 1;
            e->kind = ax >= ay ? (dx < 0 ? G_SWIPE_L : G_SWIPE_R) : (dy < 0 ? G_SWIPE_U : G_SWIPE_D);
            e->x = g->x0; e->y = g->y0;
            return 1;
        }
        if (ax > GEST_SLOP_PX || ay > GEST_SLOP_PX) g->moved = 1;
        if (!g->moved && now - g->t0 >= GEST_LONG_MS) {
            g->fired = 1; e->kind = G_LONG; e->x = g->x0; e->y = g->y0;
            return 1;
        }
        return 0;
    }
    if (!g->down) return 0;                   /* still up */
    g->down = 0;
    if (g->fired || g->moved) return 0;       /* already reported, or a drag that was neither tap nor swipe */
    e->kind = G_TAP; e->x = g->x0; e->y = g->y0;
    return 1;
}
