/*
 * Bench program for the CC1407P host driver on the F2800137 (and, with -DHOST_BUILD, on a PC for the
 * reference bytes). Runs the real driver against fake_radio hooks, then stops at bench_done.
 * Results are plain globals read through the debugger:
 *   sz[]       sizeof of the message structs (in the compiler's unit: bytes on a PC, 16-bit words on the C28x)
 *   crc_*      Plat_Util_computeCRC over the 9 bytes "123456789" (CRC-16/CCITT-FALSE check value 0x29B1)
 *   open_ok    TRX_Host_open returned a handle
 *   fake_*     what the driver sent (hooks_fake.c)
 */
#include <stdint.h>
#include <stddef.h>
#include "host/trx_host.h"
#include "hal/platform_util.h"
#include "hal_bare.h"
#include "rcl_settings_wisun.h"
extern volatile uint16_t fake_log[], fake_log_n, fake_xfers;

volatile uint16_t sz[12];
volatile uint16_t crc_check, open_ok, ev_lo, ev_hi, magic = 0x1234u;
static volatile uint32_t done_events;

static void cb(TRX_Host_Handle h, uintptr_t cfg, TRX_Request *req, uint64_t ev, uintptr_t arg)
{
    (void)h; (void)cfg; (void)req; (void)arg;
    done_events |= (uint32_t)ev;
}
#ifdef HOST_BUILD
#include <stdio.h>
#include <stdlib.h>
static void report(void)
{
    printf("sz"); for (int i = 0; i < 12; i++) printf(" %u", sz[i]);
    printf("\ncrc %04X open %u xfers %u logn %u\nlog", crc_check, open_ok, fake_xfers, fake_log_n);
    for (unsigned i = 0; i < fake_log_n; i++) printf(" %02X", fake_log[i]);
    printf("\n");
}
#endif
/* The debugger puts a breakpoint here; hooks_fake.c calls it after the first SPI transfer. */
void bench_done(void)
{
    ev_lo = (uint16_t)done_events; ev_hi = (uint16_t)(done_events >> 16);
#ifdef HOST_BUILD
    report();
    exit(0);
#endif
}
int main(void)
{
    static const uint8_t chk[9] = { '1','2','3','4','5','6','7','8','9' };
#if defined(__TMS320C2000__)
    __asm(" EALLOW"); *(volatile uint16_t *)0x7029u = 0x68u; __asm(" EDIS");   /* WDCR: watchdog off, as SysCtl_disableWatchdog() does */
#endif
    sz[0] = sizeof(TRX_TransactionHeader); sz[1] = sizeof(TRX_TransactionFooter);
    sz[2] = sizeof(TRX_Request_Header); sz[3] = sizeof(TRX_Request_LastStatus); sz[4] = sizeof(TRX_Phy);
    sz[5] = sizeof(TRX_Request_CommandStore); sz[6] = sizeof(TRX_Request_CommandSubmit);
    sz[7] = sizeof(TRX_Request_CommandStop); sz[8] = sizeof(TRX_Request_ConfigStore);
    sz[9] = sizeof(TRX_PowerMode); sz[10] = sizeof(TRX_DeviceConfigData); sz[11] = sizeof(TRX_Request);
    crc_check = Plat_Util_computeCRC(chk, 9u);

    TRX_Host_Params params = { .bitRate = 12000000u, .generalCb = cb, .arg = 0 };
    TRX_Host_Handle h = TRX_Host_open(&params);   /* sends CONFIG_LIST, then waits for an answer */
    open_ok = (h != NULL);
    bench_done();   /* reached only if the driver returned from open */
    for (;;) { }
}
