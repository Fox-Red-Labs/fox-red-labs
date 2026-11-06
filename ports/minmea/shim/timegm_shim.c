/* TI's libc has no timegm. UTC calendar time to seconds since 1970.
   The seconds are computed in 64 bits: with TI Arm Clang's 32-bit long, days * 86400 overflows after 2038. */
#include <stdint.h>
#include <time.h>
static long days_from_civil(long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}
time_t timegm(struct tm *tm)
{
    int64_t days = days_from_civil(tm->tm_year + 1900L, tm->tm_mon + 1, tm->tm_mday);
    int64_t secs = days * 86400 + (int64_t)tm->tm_hour * 3600 + (int64_t)tm->tm_min * 60 + tm->tm_sec;
    if (sizeof(time_t) < sizeof(int64_t) && (secs > INT32_MAX || secs < INT32_MIN))
        return (time_t)-1;   /* does not fit a 32-bit time_t */
    return (time_t)secs;
}
