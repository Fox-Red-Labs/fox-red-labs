#include <assert.h>
#include "commissioning_guard.h"
int main(void) {
    assert(commissioning_ready(0, 12450, 1539, 1550, 1527));
    assert(!commissioning_ready(1, 12450, 1539, 1550, 1527));
    assert(!commissioning_ready(0, 0, 1539, 1550, 1527));
    assert(!commissioning_ready(0, 25000, 1539, 1550, 1527));
    assert(!commissioning_ready(0, 12450, 0, 1550, 1527));
    assert(!commissioning_ready(0, 12450, 1539, 3300, 1527));
    assert(!commissioning_ready(0, 12450, 1539, 1550, 0));
    const unsigned seq[] = {1,5,4,6,2,3,1};
    for (unsigned i=0;i<6;i++) {
        assert(commissioning_hall_step(seq[i],seq[i+1])==1);
        assert(commissioning_hall_step(seq[i+1],seq[i])==-1);
    }
    assert(commissioning_hall_step(1,1)==0);
    assert(commissioning_hall_step(1,4)==0);
    assert(commissioning_hall_step(0,5)==0);
    assert(commissioning_hall_step(3,7)==0);
    /* Reverse must exchange the source and sink of proven forward torque. */
    const unsigned holding[8]={0,1,5,0,3,2,4,0};
    for(unsigned h=1;h<=6;h++) {
        assert(commissioning_vector(h,1)==(holding[h]+1)%6);
        assert(commissioning_vector(h,-1)==(commissioning_vector(h,1)+3)%6);
    }
    assert(commissioning_vector(0,1)==6);
    assert(commissioning_vector(7,-1)==6);
    assert(commissioning_vector(1,0)==6);
    float phases[3];
    commissioning_sine(0.0f,0.012f,phases);
    assert(phases[0]>0.0119f && phases[1]<-0.0059f && phases[2]<-0.0059f);
    for(unsigned i=0;i<360;i++) {
        commissioning_sine(i*0.01745329252f,0.04f,phases);
        assert(fabsf(phases[0]+phases[1]+phases[2])<0.000001f);
        for(unsigned p=0;p<3;p++) assert(fabsf(phases[p])<=0.040001f);
    }
    return 0;
}
