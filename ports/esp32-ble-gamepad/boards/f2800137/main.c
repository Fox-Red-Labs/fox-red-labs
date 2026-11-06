/* FRL-GP1 check for LAUNCHXL-F2800137 (probe CL371081): console on SCIA, GPIO28 RX / GPIO29 TX, 115200 8N1.
   Thin glue only; the gamepad core is gp_core.c. A report "byte" is one 16-bit word holding 0..255,
   printed as two hex digits from its low 8 bits. */
#include "driverlib.h"
#include "device.h"
#include <string.h>
#include "gp_core.h"

#ifndef GP_RAM_FREE_WORDS
#define GP_RAM_FREE_WORDS 0
#endif

static gp_state g;
static gp_byte rpt[4][32];
static unsigned int rptLen[4];
static unsigned long descX100, packX100;            /* microseconds x 100 */
static unsigned long long uptimeCycles;
static unsigned long lastCyc;

static unsigned long cyc(void) { return 0xFFFFFFFFUL - CPUTimer_getTimerCount(CPUTIMER2_BASE); }
static void tickUptime(void) { unsigned long c = cyc(); uptimeCycles += (unsigned long)(c - lastCyc); lastCyc = c; }

static void timerStart(void)
{
    CPUTimer_stopTimer(CPUTIMER2_BASE);
    CPUTimer_selectClockSource(CPUTIMER2_BASE, CPUTIMER_CLOCK_SOURCE_SYS, CPUTIMER_CLOCK_PRESCALER_1);
    CPUTimer_setPeriod(CPUTIMER2_BASE, 0xFFFFFFFFUL);
    CPUTimer_setPreScaler(CPUTIMER2_BASE, 0);
    CPUTimer_reloadTimerCounter(CPUTIMER2_BASE);
    CPUTimer_startTimer(CPUTIMER2_BASE);
    lastCyc = cyc();
}

static void sciInit(void)
{
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);
    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, 115200, (SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE | SCI_CONFIG_PAR_NONE));
    SCI_resetChannels(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);
    SCI_performSoftwareReset(SCIA_BASE);
}

static void putc_(char ch) { SCI_writeCharBlockingNonFIFO(SCIA_BASE, (uint16_t)ch); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void putu(unsigned long v)
{
    char b[12]; int n = 0;
    if (v == 0) b[n++] = '0';
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) putc_(b[--n]);
}
static void putx100(unsigned long v) { putu(v / 100); putc_('.'); putc_((char)('0' + (v / 10) % 10)); putc_((char)('0' + v % 10)); }
static void puthex(const gp_byte *p, unsigned int n)
{
    static const char H[] = "0123456789abcdef";
    unsigned int i;
    for (i = 0; i < n; i++) { putc_(H[(p[i] >> 4) & 0xF]); putc_(H[p[i] & 0xF]); }
}
static void putline(const char *tag, const gp_byte *p, unsigned int n)
{
    puts_(tag); puts_(" len="); putu(n); putc_(' '); puthex(p, n); puts_("\r\n");
}

static void runSequence(void)
{
    memset(&g, 0, sizeof(g));                              /* all state zero, as a fresh object */
    gp_config_default(&g.cfg);
    gp_begin(&g);
    gp_reset_buttons(&g);
    rptLen[0] = gp_pack_report(&g, rpt[0]);
    gp_press(&g, BUTTON_5); gp_press(&g, BUTTON_16); gp_press_start(&g);
    gp_set_axes(&g, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767); gp_set_hat1(&g, HAT_DOWN_RIGHT);
    rptLen[1] = gp_pack_report(&g, rpt[1]);
    gp_release(&g, BUTTON_5); gp_release_start(&g); gp_set_hat1(&g, HAT_CENTERED); gp_set_axes(&g, 0, 0, 0, 0, 0, 0, 0, 0);
    rptLen[2] = gp_pack_report(&g, rpt[2]);
    gp_press(&g, BUTTON_1); gp_press(&g, BUTTON_8); gp_press(&g, BUTTON_9); gp_press_select(&g);
    gp_set_axes(&g, -32768, -1, 1, 255, 256, -256, 12345, -12345); gp_set_hat1(&g, HAT_UP_LEFT);
    rptLen[3] = gp_pack_report(&g, rpt[3]);
}

static void measure(void)
{
    gp_byte m[32];
    unsigned long t0, ovh, tot;
    int i;
    { unsigned long a = cyc(), b = cyc(); ovh = b - a; }
    t0 = cyc();
    for (i = 0; i < 200; i++) gp_build_descriptor(&g);      /* state after runSequence: same config */
    tot = cyc() - t0 - ovh;
    descX100 = tot * 100UL / (200UL * (DEVICE_SYSCLK_FREQ / 1000000UL));
    t0 = cyc();
    for (i = 0; i < 1000; i++) gp_pack_report(&g, m);
    tot = cyc() - t0 - ovh;
    packX100 = tot * 100UL / (1000UL * (DEVICE_SYSCLK_FREQ / 1000000UL));
}

void main(void)
{
    Device_init();
    Device_initGPIO();
    sciInit();
    timerStart();
    runSequence();
    measure();
    runSequence();                                          /* leave the descriptor/state as the sequence made them */
    for (;;)
    {
        unsigned long long ms;
        int k;
        tickUptime();
        ms = uptimeCycles / (DEVICE_SYSCLK_FREQ / 1000UL);
        puts_("FRL-GP1 begin\r\n");
        puts_("INFO heap_free="); putu(GP_RAM_FREE_WORDS); puts_(" uptime_ms="); putu((unsigned long)ms); puts_("\r\n");
        puts_("TIME desc_us="); putx100(descX100); puts_(" pack_us="); putx100(packX100); puts_("\r\n");
        putline("MAP", g.desc, g.hidReportDescriptorSize);
        putline("RPT1", rpt[0], rptLen[0]);
        putline("RPT2", rpt[1], rptLen[1]);
        putline("RPT3", rpt[2], rptLen[2]);
        putline("RPT4", rpt[3], rptLen[3]);
        puts_("FRL-GP1 end\r\n");
        for (k = 0; k < 3000; k++) { DEVICE_DELAY_US(1000); tickUptime(); }
    }
}
