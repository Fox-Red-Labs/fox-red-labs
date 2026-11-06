/* Fox Red Labs: run TI's Wi-Fi CSI presence pipeline inside the Matter thermostat image on the LP-EM-CC35X1.
 * What this file does, and where each piece comes from:
 *  - m33_cde_init: the coprocessor-enable sequence SysConfig generates for the EdgeAI plugin's TINIECDE module
 *    (source/ti/ai/edge_ai/.meta/templates/edgeai_template.c.xdt, plugin 1.20). The 4.7k model's convolutions run on the
 *    M33's custom-datapath extension, which is off at reset. Board_init() calls the weak Board_initHook(); this is the strong one.
 *  - csi_glue_start: a small task that waits for the Matter network commissioning to join the house Wi-Fi, then does
 *    what the example's console commands do: csi_enable (Wlan_Set WLAN_SET_CSI), csi_config -s 1 -p 7
 *    (WLAN_SET_CSI_SOLICITATION), csi_set_mac -a 1 -m <access point> (WLAN_SET_CSI_SOLICITATION_MAC), and the reader
 *    thread from wlan_cmd.c that polls Wlan_Get(WLAN_GET_CSI) and hands each record to csi_pres_push().
 *  - csi_glue_result: the inference result goes to ThermoCsiResult() in ThermostatManager.cpp instead of the DAP serial
 *    protocol the example uses for Edge AI Studio. */
#include <stdint.h>
#include <string.h>
#include "wlan_if.h"
#include "osi_kernel.h"
#include <platform/ti/cc35xx/ti_wifi_structs.h>
#include "csi_presence.h"
#include "csi_glue.h"
#include "FreeRTOS.h"
#include "task.h"

#define CSI_THR_STACK_SIZE   2048u
#define CSI_THREAD_PRIORITY  1u
#define CSI_SOL_PERIOD       7u      /* the example's csi_config -s 1 -p 7 */

/* ---- coprocessor enable (TI's generated code, verbatim) ---- */
static void m33_cde_init(void)
{
    __asm volatile (
        "MOVW r1, #0xED88\n\t"
        "MOVT r1, #0xE000\n\t"
        "LDR  r2, [r1, #0x0]\n\t"
        "MOVW r3, #0xFFFF\n\t"
        "MOVT r3, #0x0000\n\t"
        "ORR  r2, r2, r3\n\t"
        "STR  r2, [r1, #0x0]\n\t"
        "DSB \n\t"
        "ISB \n\t"
        : : : "r1", "r2", "r3", "memory");
}
void Board_initHook(void) { m33_cde_init(); }

/* ---- timing for the per-window line: FreeRTOS ticks (1 ms). The DWT cycle counter read 0 on the first run (2026-10-12,
 * 13 windows all ms=0); whether it needs the CoreSight unlock on this part is not checked, so the tick clock is used. ---- */
static void dwt_start(void) { }
uint32_t csi_glue_cycles(void) { return (uint32_t) xTaskGetTickCount(); }
uint32_t csi_glue_ms_since(uint32_t t0) { return ((uint32_t) xTaskGetTickCount() - t0) * portTICK_PERIOD_MS; }

/* ---- reader thread (wlan_cmd.c csi_thread_entry, trimmed) ---- */
static volatile int s_keep_reading = 0;
static OsiThread_t  s_reader = NULL;
static uint32_t     s_records = 0;

static void csi_reader_entry(void *arg)
{
    WlanGetCSIData_t rec;
    (void) arg;
    for (;;)
    {
        if (s_keep_reading && Wlan_Get(WLAN_GET_CSI, &rec) == 0)
        {
            s_records++;
            csi_pres_push(&rec);
        }
        else
        {
            osi_uSleep(s_keep_reading ? 0 : 200000);
        }
        osi_uSleep(100);
    }
}

static int csi_enable_toward(const uint8_t bssid[6])
{
    WlanCfgCsi_t cfg = { 0 };
    WlanCfgCsiSol_t sol = { 0 };
    WlanCfgCsiSolSetMac_t mac = { 0 };
    int r;
    cfg.csiEnable = 1;
    if ((r = Wlan_Set(WLAN_SET_CSI, &cfg)) != 0) { Report("[CSI] enable failed %d\n\r", r); return r; }
    sol.csiSolEnable = 1; sol.csiSolPeriod = CSI_SOL_PERIOD;
    if ((r = Wlan_Set(WLAN_SET_CSI_SOLICITATION, &sol)) != 0) { Report("[CSI] solicitation failed %d\n\r", r); return r; }
    mac.csiSolAddRemoveMac = 1; memcpy(mac.csiSolMacAddress, bssid, 6);
    if ((r = Wlan_Set(WLAN_SET_CSI_SOLICITATION_MAC, &mac)) != 0) { Report("[CSI] solicitation mac failed %d\n\r", r); return r; }
    return 0;
}

static void csi_watch_task(void *arg)
{
    int started = 0;
    (void) arg;
    dwt_start();
    for (;;)
    {
        int connected = IS_STA_CONNECTED(app_CB.Status);
        if (connected && !started)
        {
            const uint8_t *b = app_CB.CON_CB.ConnectionBSSID;
            osi_uSleep(3000000);                     /* let DHCP and Matter's first exchanges settle */
            if (s_reader == NULL)
            {
                csi_pres_init_task();
                if (osi_ThreadCreate(&s_reader, "csi_rd", CSI_THR_STACK_SIZE, CSI_THREAD_PRIORITY, (void *) csi_reader_entry, NULL) != OSI_OK)
                    Report("[CSI] reader thread failed\n\r");
            }
            if (csi_enable_toward(b) == 0)
            {
                s_keep_reading = 1; started = 1;
                Report("[CSI] started toward %02x:%02x:%02x:%02x:%02x:%02x, window 2 s\n\r", b[0], b[1], b[2], b[3], b[4], b[5]);
            }
            else
                osi_uSleep(10000000);
        }
        else if (!connected && started)
        {
            s_keep_reading = 0; started = 0; csi_pres_reset();
            Report("[CSI] station disconnected, paused\n\r");
        }
        osi_uSleep(2000000);
    }
}

void csi_glue_start(void)
{
    static OsiThread_t watch = NULL;
    if (watch == NULL && osi_ThreadCreate(&watch, "csi_watch", 1024, 1, (void *) csi_watch_task, NULL) != OSI_OK)
        Report("[CSI] watch thread failed\n\r");
}

void csi_glue_result(const void *p, uint32_t compute_ms)
{
    const pres_result_t *r = (const pres_result_t *) p;
    ThermoCsiResult(r->decision, r->p1, r->pavg, r->pps, r->win_index, compute_ms);
}
