/* Force-included (-include) so minmea.c sees a declaration of timegm. */
#include <time.h>
time_t timegm(struct tm *tm);
