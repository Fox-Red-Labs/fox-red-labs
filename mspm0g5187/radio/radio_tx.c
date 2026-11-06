/*
 * The weather-station demo, sending side: the LP-MSPM0G5187 reads the Sensors BoosterPack
 * (HDC2080 temperature and humidity, OPT3001 light) and sends one packet a second through the
 * BP-CC1407P-9-FE at 915 MHz, 50 kbps 2-GFSK, with TI's host driver and TI's genfsk radio settings.
 * The packet is plain text, for example "FRL n=12 t=24.38C rh=37.7% lux=296.00", so any receiver
 * that prints it shows the readings. Progress and the radio's own "sent" status go to the
 * backchannel UART at 115200.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>
#include "host/trx_host.h"
#include "rcl_settings_2gfsk50kbps.h"

static void putc_(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void putu(uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }
static void puthex(uint32_t v, int digits) { for (int i = digits - 1; i >= 0; i--) putc_("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void putfix(int32_t v, uint32_t scale) { if (v < 0) { putc_('-'); v = -v; } putu((uint32_t)v / scale); putc_('.'); uint32_t f = (uint32_t)v % scale; for (uint32_t s = scale / 10; s; s /= 10) putc_((char)('0' + (f / s) % 10)); }
/* the same text into a buffer, for the packet */
static char *pb; static void bc(char c) { *pb++ = c; } static void bs(const char *s) { while (*s) bc(*s++); }
static void bu(uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); bs(b + i); }
static void bfix(int32_t v, uint32_t scale) { if (v < 0) { bc('-'); v = -v; } bu((uint32_t)v / scale); bc('.'); uint32_t f = (uint32_t)v % scale; for (uint32_t s = scale / 10; s; s /= 10) bc((char)('0' + (f / s) % 10)); }

static uint32_t errors;
static int i2c_xfer_tx(uint8_t addr, const uint8_t *d, uint8_t n)
{
    uint32_t to;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    DL_I2CC_flushTXFIFO(I2C_INST);
    if (n) DL_I2CC_fillTXFIFO(I2C_INST, (uint8_t *)d, n);
    DL_I2CC_startTransfer(I2C_INST, addr, DL_I2CC_DIRECTION_TX, n);
    delay_cycles(2000);
    for (to = 0; to < 400000 && (DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_BUSY); to++) { }
    int err = (DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_ERROR) != 0;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    if (err) { DL_I2CC_flushTXFIFO(I2C_INST); }
    delay_cycles(2000);
    return err ? -1 : 0;
}
static int i2c_xfer_rx(uint8_t addr, uint8_t *d, uint8_t n)
{
    uint32_t to;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    DL_I2CC_startTransfer(I2C_INST, addr, DL_I2CC_DIRECTION_RX, n);
    delay_cycles(2000);
    for (uint8_t i = 0; i < n; i++) {
        for (to = 0; to < 400000 && DL_I2CC_isRXFIFOEmpty(I2C_INST); to++) { }
        if (DL_I2CC_isRXFIFOEmpty(I2C_INST)) { errors++; return -1; }
        d[i] = DL_I2CC_receiveData(I2C_INST);
    }
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    delay_cycles(2000);
    return 0;
}
static int rd(uint8_t a, uint8_t reg, uint8_t *d, uint8_t n) { if (i2c_xfer_tx(a, &reg, 1)) { errors++; return -1; } return i2c_xfer_rx(a, d, n); }
static int wr(uint8_t a, uint8_t reg, uint8_t v) { uint8_t b[2] = { reg, v }; return i2c_xfer_tx(a, b, 2); }
static int wr16(uint8_t a, uint8_t reg, uint16_t v) { uint8_t b[3] = { reg, (uint8_t)(v >> 8), (uint8_t)v }; return i2c_xfer_tx(a, b, 3); }
static int rd16(uint8_t a, uint8_t reg, uint16_t *out) { uint8_t b[2] = { 0, 0 }; if (rd(a, reg, b, 2)) return -1; *out = (uint16_t)((b[0] << 8) | b[1]); return 0; }
static void ms(uint32_t m) { delay_cycles(m * (CPUCLK_FREQ / 1000)); }


#define TX_SLOT 0u
#define STREAM_ID 0u
#define RF_CONFIG_ID 1u
#define FREQ_KHZ 915000u
#define MAX_PAYLOAD 60u

static volatile uint32_t ev;            /* every event the driver reported, ORed */
static volatile uint8_t last_cmd_status, last_err_status;
static TRX_Request_CommandStore cmd_tx;
static uint8_t pkt[1 + MAX_PAYLOAD];

static void cb(TRX_Host_Handle h, uintptr_t cfg, TRX_Request *req, uint64_t e, uintptr_t arg)
{
    (void)h; (void)cfg; (void)arg;
    if ((e & TRX_EventLastStatusError) && req) { last_err_status = ((TRX_Request_LastStatus *)req)->status; }
    if ((e & TRX_EventCmdStatus) && req) { last_cmd_status = ((TRX_Request_CommandStatus *)req)->status; }
    ev |= (uint32_t)e;
}
/* wait until any bit of mask (or an error) arrives; print the step and what came back */
static bool step(const char *name, uint32_t mask, uint32_t timeout_ms)
{
    uint32_t t = 0;
    while (!(ev & (mask | TRX_EventLastStatusError)) && t < timeout_ms) { ms(1); t++; }
    bool ok = (ev & mask) && !(ev & TRX_EventLastStatusError);
    puts_(name); puts_(ok ? " ok" : " FAILED"); puts_(" ev=0x"); puthex(ev, 8);
    if (ev & TRX_EventLastStatusError) { puts_(" status="); putu(last_err_status); }
    puts_(" ("); putu(t); puts_(" ms)\r\n");
    ev = 0;
    return ok;
}

int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_clearPins(HDC_PWR_PORT, HDC_PWR_HDC_PIN);   /* pin 6 low: HDC2080 on */
    DL_GPIO_clearPins(DRV_PWR_PORT, DRV_PWR_DRV_PIN);   /* pin 24 low: DRV5055 on */
    DL_GPIO_setPins(OPT_PWR_PORT, OPT_PWR_OPT_PIN);     /* pin 27 high: OPT3001 on */
    DL_GPIO_setPins(TMP_PWR_PORT, TMP_PWR_TMP_PIN);     /* pin 28 high: TMP117 on */
    ms(50);
    /* bus recovery before the I2C peripheral takes the pins: nine clocks with SDA released, then a STOP (open drain by output enable) */
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_8 | DL_GPIO_PIN_9);
    DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_8);
    for (int i = 0; i < 9; i++) { DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_9); delay_cycles(1600); DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_9); delay_cycles(1600); }
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_8); delay_cycles(1600); DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_8); delay_cycles(1600);   /* STOP: SDA low then high while SCL high */
    SYSCFG_DL_GPIO_init(); SYSCFG_DL_I2C_init();
    DL_GPIO_clearPins(HDC_PWR_PORT, HDC_PWR_HDC_PIN); DL_GPIO_clearPins(DRV_PWR_PORT, DRV_PWR_DRV_PIN);   /* the GPIO init put the switches back to off */
    DL_GPIO_setPins(OPT_PWR_PORT, OPT_PWR_OPT_PIN); DL_GPIO_setPins(TMP_PWR_PORT, TMP_PWR_TMP_PIN); ms(50);
#ifdef TMP_EXTERNAL_WIRE   /* build_uart.sh with TMP_EXTERNAL=1: leave BP pin 28 (PA12) undriven so a wire from 3V3 to pin 28 can power the TMP117 */
    DL_GPIO_disableOutput(TMP_PWR_PORT, TMP_PWR_TMP_PIN);
#endif
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN | RADIO_RST_PIN);
    ms(100);
    puts_("\r\nradio_tx: G5187 + Sensors BoosterPack -> CC1407P, 915 MHz 50 kbps 2-GFSK\r\n");

    TRX_Host_Params p = { .bitRate = 1000000u, .generalCb = cb, .arg = 0 };
    TRX_Host_Handle h = TRX_Host_open(&p);
    if (!h) { puts_("open FAILED\r\n"); for (;;) { } }
    puts_("open ok\r\n"); ev = 0;

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
    int opt_on = 0; uint32_t sent = 0, failed = 0;
    for (uint32_t n = 1;; n++) {
        /* read the sensors */
        int32_t t100 = 0; uint32_t rh10 = 0, lux100 = 0; bool hdc = false, opt = false;
        { uint16_t m = 0; uint8_t b[4] = {0};
          if (rd16(0x40, 0xFC, &m) == 0 && m == 0x4954) { wr(0x40, 0x0F, 0x01); ms(20); rd(0x40, 0x00, b, 4);
              t100 = (int32_t)((b[1] << 8 | b[0]) * 165 * 100L / 65536) - 4000; rh10 = (uint32_t)((b[3] << 8 | b[2]) * 1000L / 65536); hdc = true; } }
        { uint16_t m = 0, r = 0;
          if (rd16(0x44, 0x7E, &m) == 0 && m == 0x5449) { if (!opt_on) { wr16(0x44, 0x01, 0xCE10); opt_on = 1; }
              rd16(0x44, 0x00, &r); lux100 = (uint32_t)(1u << (r >> 12)) * (r & 0xFFF); opt = true; } else { opt_on = 0; } }
        /* build the packet: [length][text] */
        pb = (char *)&pkt[1];
        bs("FRL n="); bu(n);
        if (hdc) { bs(" t="); bfix(t100, 100); bs("C rh="); bfix((int32_t)rh10, 10); bc('%'); } else bs(" hdc=none");
        if (opt) { bs(" lux="); bfix((int32_t)lux100, 100); } else bs(" lux=none");
        uint8_t len = (uint8_t)(pb - (char *)&pkt[1]); pkt[0] = len;
        /* send it */
        ev = 0; last_cmd_status = 0xFF;
        TRX_Host_storeStream(h, STREAM_ID, pkt, (uint16_t)(1u + len), Stream_Retention_Flush_Streaming, cb, TRX_EventStreamStoreComplete);
        uint32_t w = 0; while (!(ev & (TRX_EventStreamStoreComplete | TRX_EventLastStatusError)) && w < 2000) { ms(1); w++; }
        ev = 0;
        TRX_Host_submitCmd(h, TX_SLOT);
        w = 0; while (!(ev & (TRX_EventCmdStatus | TRX_EventLastStatusError)) && w < 2000) { ms(1); w++; }
        bool ok = (ev & TRX_EventCmdStatus) && last_cmd_status == TRX_CommandStatus_Finished;
        if (ok) sent++; else failed++;
        puts_("tx "); puts_((char *)&pkt[1]); puts_(ok ? " -> sent" : " -> NOT sent"); puts_(" status="); putu(last_cmd_status);
        puts_(" ev=0x"); puthex(ev, 8); puts_(" sent="); putu(sent); puts_(" failed="); putu(failed); puts_("\r\n");
        ms(1000);
    }
}
