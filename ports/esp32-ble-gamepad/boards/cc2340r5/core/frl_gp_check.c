#include <string.h>
#include "frl_gp_check.h"

static frl_gp_t gp;
static uint8_t rpt[4][FRL_GP_REPORT_MAX];
static uint8_t rptLen[4];
static uint32_t descUs, packUs;
static volatile uint8_t sink;

static void out_u32(uint32_t v)
{
    char b[11]; int i = 10;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    frl_out(&b[i]);
}

static void out_hex(const uint8_t *p, unsigned n)
{
    static const char H[] = "0123456789abcdef";
    char line[65];
    unsigned i, k = 0;
    for (i = 0; i < n; i++) {
        line[k++] = H[p[i] >> 4];
        line[k++] = H[p[i] & 15];
        if (k >= 64) { line[k] = 0; frl_out(line); k = 0; }
    }
    line[k] = 0;
    frl_out(line);
}

static void out_rec(const char *tag, const uint8_t *p, unsigned n)
{
    frl_out(tag); frl_out(" len="); out_u32(n); frl_out(" ");
    out_hex(p, n);
    frl_out("\r\n");
}

int frl_gp_check_init(void)
{
    frl_gp_config_t cfg;
    uint8_t scratch[FRL_GP_REPORT_MAX];
    uint32_t t0, t1;
    unsigned i;
    int rc;

    frl_gp_config_default(&cfg);
    rc = frl_gp_begin(&gp, &cfg);                 /* all state zero */
    rptLen[0] = frl_gp_pack(&gp, rpt[0]);         /* RPT1 */

    frl_gp_press(&gp, FRL_BUTTON_5); frl_gp_press(&gp, FRL_BUTTON_16); frl_gp_pressStart(&gp);
    frl_gp_setAxes(&gp, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
    frl_gp_setHat1(&gp, FRL_HAT_DOWN_RIGHT);
    rptLen[1] = frl_gp_pack(&gp, rpt[1]);         /* RPT2 */

    frl_gp_release(&gp, FRL_BUTTON_5); frl_gp_releaseStart(&gp);
    frl_gp_setHat1(&gp, FRL_HAT_CENTERED);
    frl_gp_setAxes(&gp, 0, 0, 0, 0, 0, 0, 0, 0);
    rptLen[2] = frl_gp_pack(&gp, rpt[2]);         /* RPT3 */

    frl_gp_press(&gp, FRL_BUTTON_1); frl_gp_press(&gp, FRL_BUTTON_8); frl_gp_press(&gp, FRL_BUTTON_9);
    frl_gp_pressSelect(&gp);
    frl_gp_setAxes(&gp, -32768, -1, 1, 255, 256, -256, 12345, -12345);
    frl_gp_setHat1(&gp, FRL_HAT_UP_LEFT);
    rptLen[3] = frl_gp_pack(&gp, rpt[3]);         /* RPT4 */

    {   /* timing runs on a scratch copy so the stored state is untouched */
        static frl_gp_t tmp;
        t0 = frl_micros();
        for (i = 0; i < 200; i++) { frl_gp_begin(&tmp, &cfg); sink = tmp.desc[i & 31]; }
        t1 = frl_micros();
        descUs = (t1 - t0) / 200;
        t0 = frl_micros();
        for (i = 0; i < 1000; i++) { tmp.x = (int16_t)i; sink = frl_gp_pack(&tmp, scratch) + scratch[i & 7]; }
        t1 = frl_micros();
        packUs = (t1 - t0) / 1000;
    }
    return rc;
}

void frl_gp_check_print(void)
{
    frl_out("FRL-GP1 begin\r\n");
    frl_out("INFO heap_free="); out_u32(frl_heap_free());
    frl_out(" uptime_ms="); out_u32(frl_uptime_ms()); frl_out("\r\n");
    frl_out("TIME desc_us="); out_u32(descUs);
    frl_out(" pack_us="); out_u32(packUs); frl_out("\r\n");
    out_rec("MAP", gp.desc, gp.descLen);
    out_rec("RPT1", rpt[0], rptLen[0]);
    out_rec("RPT2", rpt[1], rptLen[1]);
    out_rec("RPT3", rpt[2], rptLen[2]);
    out_rec("RPT4", rpt[3], rptLen[3]);
    frl_out("FRL-GP1 end\r\n");
}

const frl_gp_t *frl_gp_check_gamepad(void) { return &gp; }
