#ifndef HAPTIC_CORE_H
#define HAPTIC_CORE_H
/* Haptic knob core: pure arithmetic, no hardware, so it can be tested on the Mac (tests/test_haptic_core.c).
 * The Hall sensors give 24 sectors per shaft turn (4 pole pairs x 6), so the angle is known to 15 degrees.
 * Detents sit every S = 24 / detents sectors. The torque command is a coarse spring toward the nearest detent
 * centre, in whole sectors: nothing at the centre, more the farther out, never above HAPTIC_DUTY_MAX. */
#define HAPTIC_SECTORS      24
#define HAPTIC_DUTY_MIN     5U    /* below this the motor does not move: cogging and friction */
#define HAPTIC_DUTY_MAX     14U   /* the six-step runs allow 15; the knob stays under it */
#define HAPTIC_SECONDS_MAX  20U   /* one run; the MCU ends it itself, the host cannot extend it */

static inline int haptic_detents_ok(unsigned n) { return n==2U||n==3U||n==4U||n==6U||n==8U||n==12U; }

/* Signed distance in sectors from the nearest detent centre, in (-S/2, S/2]. */
static inline int haptic_error(int net, unsigned spacing)
{
    int s=(int)spacing; int m=((net%s)+s)%s;
    if(m*2>s) m-=s;
    return m;
}
/* dir: +1 push forward, -1 push back, 0 no torque. Returns the duty (0 when dir is 0). */
static inline unsigned haptic_command(int net, unsigned detents, unsigned strength, int *dir)
{
    *dir=0;
    if(!haptic_detents_ok(detents) || strength<1U || strength>10U) return 0U;
    unsigned spacing=HAPTIC_SECTORS/detents;
    int e=haptic_error(net,spacing);
    if(e==0) return 0U;
    *dir = e>0 ? -1 : 1;
    unsigned a=(unsigned)(e>0?e:-e), half=spacing/2U; if(half==0U) half=1U; if(a>half) a=half;
    unsigned d=((4U+strength)*a+half-1U)/half;          /* strength 1..10 -> 5..14 at the far side of a detent */
    if(d<HAPTIC_DUTY_MIN) d=HAPTIC_DUTY_MIN;
    if(d>HAPTIC_DUTY_MAX) d=HAPTIC_DUTY_MAX;
    return d;
}
#endif
