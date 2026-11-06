/*
 * CC1407P on an LP-MSPM0G5187, first light: open TI's host driver over SPI and read the radio's
 * firmware version, printed on the backchannel UART (115200 8N1). Then the sensor pack's
 * temperature, humidity and light go out over the radio (phase 2, after this works).
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>
#include "host/trx_host.h"

static void putc_(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void putu(uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }
static void puthex(uint32_t v, int digits) { for (int i = digits - 1; i >= 0; i--) putc_("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void ms(uint32_t m) { delay_cycles(m * (CPUCLK_FREQ / 1000)); }

static volatile uint32_t events;
static volatile TRX_Request_LastStatus_Param_Version ver;
static volatile uint8_t have_ver, last_status;

static void cb(TRX_Host_Handle h, uintptr_t cfg, TRX_Request *req, uint64_t ev, uintptr_t arg)
{
    (void)h; (void)cfg; (void)arg;
    events |= (uint32_t)ev;
    if ((ev & TRX_EventGetVersionComplete) && req) {
        TRX_Request_LastStatus *ls = (TRX_Request_LastStatus *)req;
        last_status = ls->status;
        const uint8_t *b = ls->params;   /* TRX_Request_LastStatus_Param_Version, byte-packed: major minor patch build hash32 */
        ver.major = b[0]; ver.minor = b[1]; ver.patch = b[2]; ver.build = b[3];
        ver.hash = (uint32_t)b[4] | ((uint32_t)b[5] << 8) | ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24);
        have_ver = 1;
    }
}

int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN | RADIO_RST_PIN);   /* chip select off, radio out of reset */
    ms(100);
    puts_("\r\nradio_host: CC1407P on LP-MSPM0G5187 (UC3 SPI on J20)\r\n");
    TRX_Host_Params p = { .bitRate = 4000000u, .generalCb = cb, .arg = 0 };
    puts_("open: "); 
    TRX_Host_Handle h = TRX_Host_open(&p);
    if (!h) { puts_("FAILED (no handle)\r\n"); for (;;) { } }
    puts_("ok\r\n");
    for (uint32_t n = 1;; n++) {
        have_ver = 0; events = 0;
        TRX_Host_Status st = TRX_Host_utilGetVersion(h, cb);
        puts_("getVersion n="); putu(n); puts_(" status="); putu((uint32_t)st); puts_(" ");
        uint32_t t = 0;
        while (!have_ver && t < 2000u) { ms(1); t++; }
        if (have_ver) {
            puts_("firmware "); putu(ver.major); putc_('.'); putu(ver.minor); putc_('.'); putu(ver.patch); putc_('.'); putu(ver.build);
            puts_(" hash 0x"); puthex(ver.hash, 8); puts_(" status "); putu(last_status);
        } else { puts_("no answer in 2 s, events=0x"); puthex(events, 8); }
        puts_("\r\n");
        ms(3000);
    }
}
