#ifndef COMMISSIONING_GUARD_H
#define COMMISSIONING_GUARD_H
#include <stdint.h>
#include <math.h>
/* Gate only a brief all-inputs-low enable test on the 12 V bench supply.
 * This is not permission to run closed-loop control or proof of current polarity. */
static inline int commissioning_ready(int fault, uint32_t vm_mv,
                                      uint32_t a_mv, uint32_t b_mv, uint32_t c_mv)
{
    return !fault && vm_mv >= 9000 && vm_mv <= 15000 &&
           a_mv >= 1350 && a_mv <= 1750 &&
           b_mv >= 1350 && b_mv <= 1750 &&
           c_mv >= 1350 && c_mv <= 1750;
}
/* Measured Hall order from the prior bench run. Non-neighbor transitions
 * contribute no travel; alternating neighbors cancel in a signed total. */
static inline int commissioning_hall_step(unsigned previous, unsigned next)
{
    static const unsigned seq[6] = {1,5,4,6,2,3};
    int a=-1,b=-1;
    for (int i=0;i<6;i++) { if (seq[i]==previous) a=i; if(seq[i]==next) b=i; }
    if(a<0 || b<0) return 0;
    int delta=(b-a+6)%6;
    return delta==1 ? 1 : (delta==5 ? -1 : 0);
}
/* Vector order AB,AC,BC,BA,CA,CB; forward sequence proved in rev17.
 * Six is an invalid sentinel and must never index a PWM output array. */
static inline unsigned commissioning_vector(unsigned hall, int direction)
{
    static const unsigned holding[8]={0,1,5,0,3,2,4,0};
    if(hall<1 || hall>6 || (direction!=1 && direction!=-1)) return 6;
    /* Reverse the proven forward voltage vector, swapping source/sink. */
    return (holding[hall]+(direction==1 ? 1U : 4U))%6U;
}
static inline void commissioning_sine(float angle, float amplitude, float phases[3])
{
    phases[0]=amplitude*cosf(angle);
    phases[1]=amplitude*cosf(angle-2.09439510239f);
    phases[2]=-phases[0]-phases[1];
}
#endif
