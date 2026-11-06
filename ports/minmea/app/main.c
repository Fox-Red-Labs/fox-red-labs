#include <stdint.h>
#include "minmea.h"

/* Results sit in globals so a debugger (or the map file) can see them. */
volatile int      g_rmc_ok, g_gga_ok, g_ts_ok;
volatile int32_t  g_lat_e4, g_lon_e4, g_alt_dm, g_ts;
volatile int      g_sats;

static const char RMC[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
static const char GGA[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";

int main(void)
{
    struct minmea_sentence_rmc rmc;
    struct minmea_sentence_gga gga;
    if (minmea_sentence_id(RMC, false) == MINMEA_SENTENCE_RMC && minmea_parse_rmc(&rmc, RMC)) {
        g_rmc_ok = 1;
        g_lat_e4 = (int32_t)(minmea_tocoord(&rmc.latitude) * 1e4f);  /* x10^4 deg */
        g_lon_e4 = (int32_t)(minmea_tocoord(&rmc.longitude) * 1e4f);
        struct timespec ts;
        if (minmea_gettime(&ts, &rmc.date, &rmc.time) == 0) { g_ts_ok = 1; g_ts = (int32_t)ts.tv_sec; }
    }
    if (minmea_sentence_id(GGA, false) == MINMEA_SENTENCE_GGA && minmea_parse_gga(&gga, GGA)) {
        g_gga_ok = 1; g_sats = gga.satellites_tracked;
        g_alt_dm = minmea_rescale(&gga.altitude, 10);
    }
    for (;;) { __asm volatile("wfi"); }
}
