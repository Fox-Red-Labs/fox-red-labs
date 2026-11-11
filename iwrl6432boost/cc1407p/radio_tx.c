/*
 * IWRL6432BOOST sending a counter over TI's CC1407P sub-1 GHz radio, no radar running.
 * The same radio settings as mspm0g5187/radio/radio_tx.c (the run that sent 21,658 of 21,658):
 * 915 MHz, 50 kbps 2-GFSK, high power amplifier at its top level, no whitening, packet = [length][payload].
 * One 20-byte payload a second: "FRL n=000012 RADAR-1". Progress and the radio's status go to the
 * board's first USB serial port at 115200.
 */
#include <stdint.h>
#include <stdbool.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "host/trx_host.h"
#include "rcl_settings_2gfsk50kbps.h"
#include "hal_bare.h"

void hb_hw_init(void);
void hb_poll(void);
uint32_t hb_now_us(void);
uint32_t hb_spi_sysconfig(void);
extern volatile uint32_t hb_spi_bytes, hb_spi_xfers, hb_spi_us, hb_int_calls;

#define TX_SLOT 0u
#define STREAM_ID 0u
#define RF_CONFIG_ID 1u
#define FREQ_KHZ 915000u
#define PAYLOAD 20u
#ifndef PERIOD_MS
#define PERIOD_MS 1000u
#endif

static volatile uint32_t ev;
static volatile uint8_t last_cmd_status, last_err_status;
static TRX_Request_CommandStore cmd_tx;
static uint8_t pkt[1 + PAYLOAD];

static void cb(TRX_Host_Handle h, uintptr_t cfg, TRX_Request *req, uint64_t e, uintptr_t arg)
{
    (void)h; (void)cfg; (void)arg;
    if ((e & TRX_EventLastStatusError) && req) { last_err_status = ((TRX_Request_LastStatus *)req)->status; }
    if ((e & TRX_EventCmdStatus) && req) { last_cmd_status = ((TRX_Request_CommandStatus *)req)->status; }
    ev |= (uint32_t)e;
}
static void ms(uint32_t m) { uint32_t t0 = hb_now_us(); while ((uint32_t)(hb_now_us() - t0) < m * 1000u) { hb_poll(); } }
static bool step(const char *name, uint32_t mask, uint32_t timeout_ms)
{
    uint32_t t0 = hb_now_us();
    while (!(ev & (mask | TRX_EventLastStatusError)) && (uint32_t)(hb_now_us() - t0) < timeout_ms * 1000u) { hb_poll(); }
    bool ok = (ev & mask) && !(ev & TRX_EventLastStatusError);
    DebugP_log("%s %s ev=0x%08x", name, ok ? "ok" : "FAILED", (unsigned)ev);
    if (ev & TRX_EventLastStatusError) { DebugP_log(" status=%u", (unsigned)last_err_status); }
    DebugP_log(" (%u ms)\r\n", (unsigned)((hb_now_us() - t0) / 1000u));
    ev = 0;
    return ok;
}

void radio_tx_main(void *args)
{
    (void)args;
    Drivers_open();
    Board_driversOpen();
    hb_hw_init();
    DebugP_log("\r\nradio_tx: IWRL6432BOOST -> CC1407P, 915 MHz 50 kbps 2-GFSK, %u-byte payload every %u ms\r\n", (unsigned)PAYLOAD, (unsigned)PERIOD_MS);
    DebugP_log("McSPI SYSCONFIG=0x%08x (SIDLEMODE bits 4:3, AUTOIDLE bit 0)\r\n", (unsigned)hb_spi_sysconfig());

    TRX_Host_Params p = { .bitRate = 1000000u, .generalCb = cb, .arg = 0 };
    TRX_Host_Handle h = TRX_Host_open(&p);
    if (!h) { DebugP_log("open FAILED\r\n"); for (;;) { } }
    DebugP_log("open ok\r\n"); ev = 0;

    TRX_Host_eraseNv(h, cb);                          step("eraseNv", TRX_EventNvEraseComplete, 3000);
    TRX_DeviceConfigData dc = {
        .dioConfig = { .dio0 = DIO0_UNCHANGED, .dio2 = DIO2_HIGH_PA, .dio3 = DIO3_LOW_PA_AND_LNA,
                       .dio4 = DIO4_UNCHANGED, .dio5 = DIO5_UNCHANGED, .dio6 = DIO6_UNCHANGED },
        .clockConfig = TRX_ClockConfig_XOSC,
        .powerMode = { .dwellTimeUs = 0u, .powerPolicy = (TRX_PowerPolicy)0 },
        .rfMode = TRX_RfMode_PropFSK,
    };
    TRX_Host_storeDeviceConfig(h, dc, cb);            step("deviceConfig", TRX_EventDeviceConfigStoreComplete, 3000);
    TRX_Host_storeConfig(h, TRX_CONFIG_ID_FRONTEND, (uint8_t *)LRF_frontendRegConfig_2gfsk50kbps,
        (uint16_t)LRF_frontendRegConfig_2gfsk50kbps_byteCount, 0xFEFEFEFEu, true, cb,
        TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
                                                      step("frontendStore", TRX_EventConfigStoreComplete, 3000);
    TRX_Host_persistConfig(h, TRX_CONFIG_ID_FRONTEND); step("frontendPersist", TRX_EventConfigPersistComplete, 3000);
    TRX_Host_storeConfig(h, RF_CONFIG_ID, (uint8_t *)LRF_mainRegConfig_2gfsk50kbps,
        (uint16_t)LRF_mainRegConfig_2gfsk50kbps_byteCount, 0xF000000Du, false, cb,
        TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
                                                      step("phyStore", TRX_EventConfigStoreComplete, 3000);
    TRX_Host_persistConfig(h, RF_CONFIG_ID);          step("phyPersist", TRX_EventConfigPersistComplete, 3000);

    cmd_tx.slot = TX_SLOT; cmd_tx.cmd_id = TRX_RadioCommand_Transmit;
    cmd_tx.enable_on_true = false; cmd_tx.enable_on_false = false; cmd_tx.enable_on_compare = false;
    cmd_tx.trigger = Command_Trigger_Immediate; cmd_tx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt; cmd_tx.allow_delay = false;
    cmd_tx.params.tx.phy0.config_id = RF_CONFIG_ID;
    cmd_tx.params.tx.phy0.option_mask = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS;
    cmd_tx.params.tx.pa = TRX_TxPa_High; cmd_tx.params.tx.stream_id = STREAM_ID;
    cmd_tx.params.tx.frequency = FREQ_KHZ; cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;
    cmd_tx.params.tx.power = TRX_txPowerLevelsHigh_2gfsk50kbps.powerLevels[TRX_txPowerLevelsHigh_2gfsk50kbps.numEntries - 1];
    TRX_Host_storeCmds(h, &cmd_tx, 1u, NULL, cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
                                                      step("cmdStore", TRX_EventCmdStoreComplete, 3000);
    uint32_t sent = 0, failed = 0;
    for (uint32_t n = 1;; n++) {
        uint32_t tstart = hb_now_us();
        /* payload: "FRL n=" + 6 digits + " RADAR-1" = 20 bytes */
        const char *tail = " RADAR-1"; uint8_t *q = &pkt[1];
        *q++ = 'F'; *q++ = 'R'; *q++ = 'L'; *q++ = ' '; *q++ = 'n'; *q++ = '=';
        { uint32_t v = n % 1000000u; for (int d = 5; d >= 0; d--) { q[d] = (uint8_t)('0' + v % 10u); v /= 10u; } q += 6; }
        while (*tail) { *q++ = (uint8_t)*tail++; }
        pkt[0] = (uint8_t)PAYLOAD;
        ev = 0; last_cmd_status = 0xFF;
        TRX_Host_storeStream(h, STREAM_ID, pkt, (uint16_t)(1u + PAYLOAD), Stream_Retention_Flush_Streaming, cb, TRX_EventStreamStoreComplete);
        { uint32_t t0 = hb_now_us(); while (!(ev & (TRX_EventStreamStoreComplete | TRX_EventLastStatusError)) && (uint32_t)(hb_now_us() - t0) < 2000000u) { hb_poll(); } }
        ev = 0;
        TRX_Host_submitCmd(h, TX_SLOT);
        { uint32_t t0 = hb_now_us(); while (!(ev & (TRX_EventCmdStatus | TRX_EventLastStatusError)) && (uint32_t)(hb_now_us() - t0) < 2000000u) { hb_poll(); } }
        bool ok = (ev & TRX_EventCmdStatus) && last_cmd_status == TRX_CommandStatus_Finished;
        if (ok) { sent++; } else { failed++; }
        DebugP_log("tx %.20s -> %s status=%u ev=0x%08x sent=%u failed=%u\r\n", (char *)&pkt[1], ok ? "sent" : "NOT sent",
                   (unsigned)last_cmd_status, (unsigned)ev, (unsigned)sent, (unsigned)failed);
        while ((uint32_t)(hb_now_us() - tstart) < PERIOD_MS * 1000u) { hb_poll(); }
    }
}
