/*
 * Bare-metal CC1407P transmit host, the same job as TI's ti_cc140xp_tx Zephyr sample:
 * open the driver, store device config, store the Wi-SUN radio settings, store one TX
 * command, then stream one packet and submit it. No RTOS: callbacks set flags and the
 * main loop steps through the sequence. Used to measure what a host needs; not run on
 * hardware yet. Logic and constants follow TI's main.c in simplelink-zephyr
 * v3.7.0-ti-1.00.01_ga.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "host/trx_host.h"
#include "rcl_settings_wisun.h"

#define TX_PAYLOAD_LENGTH 76u
#define TX_PACKET_LENGTH (sizeof(TRX_PayloadHeader) + TX_PAYLOAD_LENGTH)
#define RF_CONFIG_ID 1u

volatile uint32_t frl_stage;
static volatile uint32_t done_events;
static TRX_Request_CommandStore cmd_tx;
static uint8_t tx_packet[TX_PACKET_LENGTH];

static void cb(TRX_Host_Handle h, uintptr_t cfg, TRX_Request *req, uint64_t ev, uintptr_t arg)
{
    (void)h; (void)cfg; (void)req; (void)arg;
    done_events |= (uint32_t)ev;
}
void frl_cc1407p_tx_once(void)
{
    TRX_Host_Params params = { .bitRate = 12000000u, .generalCb = cb, .arg = 0 };
    frl_stage = 1;
    TRX_Host_Handle h = TRX_Host_open(&params);
    frl_stage = 2;
    if (!h) { return; }
    TRX_DeviceConfigData dc = {
        .dioConfig = { .dio0 = DIO0_UNCHANGED, .dio2 = DIO2_HIGH_PA, .dio3 = DIO3_LOW_PA_AND_LNA,
                       .dio4 = DIO4_UNCHANGED, .dio5 = DIO5_UNCHANGED, .dio6 = DIO6_UNCHANGED },
        .clockConfig = TRX_ClockConfig_XOSC,
        .powerMode = { .dwellTimeUs = 0u, .powerPolicy = TRX_PowerPolicy_StandbyDisallow },
        .rfMode = TRX_RfMode_SUN,
    };
    frl_stage = 30;
    (void)TRX_Host_storeDeviceConfig(h, dc, cb);
    frl_stage = 40;
    (void)TRX_Host_eraseNv(h, cb);
    frl_stage = 50;
    (void)TRX_Host_storeConfig(h, TRX_CONFIG_ID_FRONTEND, (uint8_t *)LRF_frontendRegConfig_wisun,
        (uint16_t)LRF_frontendRegConfig_wisun_byteCount, 0xFEFEFEFEu, true, cb,
        TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
    frl_stage = 60;
    (void)TRX_Host_persistConfig(h, TRX_CONFIG_ID_FRONTEND);
    frl_stage = 70;
    (void)TRX_Host_storeConfig(h, RF_CONFIG_ID, (uint8_t *)LRF_mainRegConfig_wisun,
        (uint16_t)LRF_mainRegConfig_wisun_byteCount, 0xF000000Du, false, cb,
        TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
    frl_stage = 80;
    (void)TRX_Host_persistConfig(h, RF_CONFIG_ID);
    cmd_tx.slot = 0; cmd_tx.cmd_id = TRX_RadioCommand_Transmit;
    cmd_tx.trigger = Command_Trigger_Immediate; cmd_tx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
    cmd_tx.params.tx.phy0.config_id = RF_CONFIG_ID;
    cmd_tx.params.tx.phy0.option_mask = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN;
    cmd_tx.params.tx.pa = TRX_TxPa_High; cmd_tx.params.tx.stream_id = 0;
    cmd_tx.params.tx.frequency = 920600u; cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;
    cmd_tx.params.tx.power = TRX_txPowerLevelsHigh_wisun.powerLevels[TRX_txPowerLevelsHigh_wisun.numEntries - 1];
    frl_stage = 90;
    (void)TRX_Host_storeCmds(h, &cmd_tx, 1u, NULL, cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
    ((TRX_PayloadHeader_SunFSK *)tx_packet)->length = TX_PAYLOAD_LENGTH;
    ((TRX_PayloadHeader_SunFSK *)tx_packet)->modulation = TRX_PayloadHeader_Modulation_FSK;
    ((TRX_PayloadHeader_SunFSK *)tx_packet)->whitening = 1u;
    frl_stage = 100;
    (void)TRX_Host_storeStream(h, 0, tx_packet, sizeof(tx_packet), Stream_Retention_Flush_Streaming, cb,
        TRX_EventLastStatusError | TRX_EventStreamStoreComplete);
    frl_stage = 110;
    (void)TRX_Host_submitCmd(h, 0);
}

