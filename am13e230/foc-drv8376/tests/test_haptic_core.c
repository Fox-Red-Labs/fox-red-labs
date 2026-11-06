#include <assert.h>
#include <stdio.h>
#include "haptic_core.h"
int main(void) {
    const unsigned dets[]={2,3,4,6,8,12};
    for(unsigned k=0;k<6;k++) for(unsigned w=1;w<=10;w++) {
        unsigned n=dets[k], S=HAPTIC_SECTORS/n;
        for(int net=-200;net<=200;net++) {
            int dir; unsigned d=haptic_command(net,n,w,&dir);
            int e=haptic_error(net,S);
            if(e==0) { assert(dir==0 && d==0); continue; }
            assert(dir==(e>0?-1:1));                       /* always toward the centre */
            assert(d>=HAPTIC_DUTY_MIN && d<=HAPTIC_DUTY_MAX);
            assert(d<=15U);                                /* under the six-step runs' own cap */
            int dir2; unsigned d2=haptic_command(net+(int)S,n,w,&dir2);
            assert(d2==d && dir2==dir);                    /* repeats every detent */
        }
        /* toy motor: one sector per 5 ticks of torque; from any start it reaches a centre and stays */
        for(int start=-50;start<=50;start++) {
            int net=start,acc=0;
            for(int t=0;t<2000;t++) { int dir; unsigned d=haptic_command(net,n,w,&dir);
                if(d){acc+=dir;if(acc>=5){net++;acc=0;}else if(acc<=-5){net--;acc=0;}} else acc=0; }
            assert(haptic_error(net,S)==0);
        }
    }
    int dir; assert(haptic_command(1,5,5,&dir)==0 && dir==0);   /* 5 detents is not a divisor of 24 */
    assert(haptic_command(1,12,0,&dir)==0);
    assert(haptic_command(1,12,11,&dir)==0);
    puts("haptic core OK");
    return 0;
}
