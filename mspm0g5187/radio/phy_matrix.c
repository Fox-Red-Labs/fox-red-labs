/*
 * CC1407P protocol matrix on the LP-MSPM0G5187: every radio profile TI's rf_diagnostics sample
 * offers (simplelink-zephyr v3.7.0-ti-1.00.01_ga, src/radio/trx/PhySettings_trx.c), configured
 * and transmitted by TI's host driver from this MSPM0. For each profile it measures, on the host:
 *   - the time to switch the radio to it (device config, then PHY config store and persist, then
 *     the transmit command store)
 *   - per packet: the stream store time and the time from submit to the radio's command status
 *   - SPI bytes and SPI time the host spent per packet
 * at two payload sizes, and prints one CSV line per profile and size on the backchannel UART.
 * All at 915 MHz, +14 dBm, high-power amplifier. Payloads start with "FRL P<n> " so a receiver
 * that hears them can say which profile it was.
 */
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "host/trx_host.h"
#include "hal_bare.h"
#ifndef RESET_EACH
#define RESET_EACH 0
#endif
#ifndef KICK_ON_REFUSAL
#define KICK_ON_REFUSAL 1
#endif
#include "settings/rcl_settings_wisun.h"
#include "settings/rcl_settings_2gfsk50kbps.h"
#include "settings/rcl_settings_2gfsk38p4kbps.h"
#include "settings/rcl_settings_2gfsk9p6kbps.h"
#include "settings/rcl_settings_general_purpose_gfsk.h"
#include "settings/rcl_settings_2gfsk50kbps_lp.h"
#include "settings/rcl_settings_sunoqpsk.h"
#include "settings/rcl_settings_ofdmlr_8p33ksps.h"
#include "settings/rcl_settings_ofdmlr_16p67ksps.h"
#include "settings/rcl_settings_ofdmlr_66p67ksps.h"
#include "settings/rcl_settings_xlr_20ksps.h"

/* ---------- serial ---------- */
static void putc_(char c) { while (DL_UART_Main_isTXFIFOFull(UART_0_INST)) { } DL_UART_Main_transmitData(UART_0_INST, (uint8_t)c); }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void putu(uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }
static void puthex(uint32_t v, int d) { for (int i = d - 1; i >= 0; i--) putc_("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void ms(uint32_t m) { delay_cycles(m * (CPUCLK_FREQ / 1000)); }

/* ---------- microsecond clock on TIMG0 (down-counter, overflow counted in its interrupt) ---------- */
static volatile uint32_t tick_ovf;
static uint32_t ticks_per_ms = 1;
void TIMG0_IRQHandler(void) { if (DL_TimerG_getPendingInterrupt(TIMG0) == DL_TIMERG_IIDX_ZERO) tick_ovf++; }   /* reading the pending index clears the flag */
static uint32_t ticks(void)
{
    /* Safe with interrupts off (the driver calls the SPI hook inside critical sections): a wrap whose
     * interrupt has not run yet shows as the raw ZERO flag still set; count it here. */
    uint32_t pm; __asm volatile("mrs %0, primask\n cpsid i" : "=r"(pm) :: "memory");
    uint32_t c = DL_TimerG_getTimerCount(TIMG0);
    uint32_t o = tick_ovf;
    if (DL_TimerG_getRawInterruptStatus(TIMG0, DL_TIMER_INTERRUPT_ZERO_EVENT)) {
        c = DL_TimerG_getTimerCount(TIMG0);   /* re-read after seeing the flag: the wrap is before this read */
        o++;
    }
    if (!(pm & 1u)) __asm volatile("cpsie i" ::: "memory");
    return (o << 16) + (0xFFFFu - c);
}
uint32_t frl_now_us(void) { return (uint32_t)((uint64_t)ticks() * 1000u / ticks_per_ms); }
static void clock_init(void)
{
    DL_TimerG_reset(TIMG0); DL_TimerG_enablePower(TIMG0); delay_cycles(16);
    DL_TimerG_ClockConfig cc = { .clockSel = DL_TIMER_CLOCK_BUSCLK, .divideRatio = DL_TIMER_CLOCK_DIVIDE_8, .prescale = 0 };
    DL_TimerG_setClockConfig(TIMG0, &cc);
    DL_TimerG_TimerConfig tc = { .timerMode = DL_TIMER_TIMER_MODE_PERIODIC, .period = 0xFFFF, .startTimer = DL_TIMER_START };
    DL_TimerG_initTimerMode(TIMG0, &tc);
    DL_TimerG_enableInterrupt(TIMG0, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
    DL_TimerG_enableClock(TIMG0);
    uint32_t a = ticks(); delay_cycles(CPUCLK_FREQ / 10); uint32_t b = ticks();   /* 100 ms by the CPU clock */
    ticks_per_ms = (b - a) / 100u; if (!ticks_per_ms) ticks_per_ms = 1;
}

/* SPI accounting kept by hooks_mspm0g5187.c */
extern volatile uint32_t hb_spi_bytes, hb_spi_us, hb_spi_xfers;

/* ---------- the profile table (after TI's PhySettings_trx.c) ---------- */
enum { H_SUNFSK, H_GENFSK, H_OQPSK, H_OFDM, H_OFDMLR };
typedef struct {
    const char *name; TRX_RadioCommand_Modem modem; uint8_t hdr; uint16_t opt; uint8_t rate;
    uint8_t rfMode; const uint32_t *cfg; const uint32_t *cfgBytes; uint16_t maxPayload;
} Phy;
#define CFG(x) LRF_mainRegConfig_##x, &LRF_mainRegConfig_##x##_byteCount
static const Phy phys[] = {
    { "Wi-SUN FSK mode 2b",              TRX_RadioCommand_Modem_FSK,   H_SUNFSK, TRX_PHY_FEATURE_FSK_MODE_2B_WISUN, 0, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN FSK mode 4b",              TRX_RadioCommand_Modem_FSK,   H_SUNFSK, TRX_PHY_FEATURE_FSK_MODE_4B_WISUN, 0, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt1 MCS6",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_1_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS6, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt2 MCS3",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS3, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt2 MCS4",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS4, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt2 MCS5",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS5, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt2 MCS6",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS6, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt3 MCS4",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS4, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt3 MCS5",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS5, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt3 MCS6",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS6, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt4 MCS4",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS4, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt4 MCS5",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS5, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "Wi-SUN OFDM opt4 MCS6",           TRX_RadioCommand_Modem_OFDM,  H_OFDM, TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN, TRX_PayloadHeader_SunOFDM_Rate_MCS6, TRX_RfMode_SUN, CFG(wisun), 2047 },
    { "2-GFSK 50 kbps high performance", TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS, 0, TRX_RfMode_PropFSK, CFG(2gfsk50kbps), 255 },
    { "2-GFSK 38.4 kbps",                TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK38P4KBPS, 0, TRX_RfMode_PropFSK, CFG(2gfsk38p4kbps), 255 },
    { "2-GFSK 9.6 kbps",                 TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK9P6KBPS, 0, TRX_RfMode_PropFSKPeakAGC, CFG(2gfsk9p6kbps), 255 },
    { "General purpose GFSK",            TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_WHITENING_DISABLED_GENERAL_PURPOSE_GFSK, 0, TRX_RfMode_PropFSK, CFG(generalPurposeGfsk), 255 },
    { "2-GFSK 50 kbps low power",        TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS_LP, 0, TRX_RfMode_PropFSK, CFG(2gfsk50kbpsLp), 255 },
    { "SUN O-QPSK rate mode 3, 50 kbps", TRX_RadioCommand_Modem_OQPSK, H_OQPSK, 0, 0, TRX_RfMode_SUN, CFG(sunoqpsk), 2047 },
    { "OFDM-LR 8.33 ksps DSSS 6",        TRX_RadioCommand_Modem_OFDM,  H_OFDMLR, 0, TRX_PayloadHeader_LrOFDM_Rate_DSSS6, TRX_RfMode_SUN, CFG(ofdmlr8p33ksps), 255 },
    { "OFDM-LR 16.67 ksps DSSS 6",       TRX_RadioCommand_Modem_OFDM,  H_OFDMLR, 0, TRX_PayloadHeader_LrOFDM_Rate_DSSS6, TRX_RfMode_SUN, CFG(ofdmlr16p67ksps), 255 },
    { "OFDM-LR 66.67 ksps DSSS 2",       TRX_RadioCommand_Modem_OFDM,  H_OFDMLR, 0, TRX_PayloadHeader_LrOFDM_Rate_DSSS2, TRX_RfMode_SUN, CFG(ofdmlr66p67ksps), 255 },
    { "XLR 20 ksps DSSS 2",              TRX_RadioCommand_Modem_FSK,   H_GENFSK, TRX_PHY_FEATURE_DSSS_2_XLR_20KSPS, 0, TRX_RfMode_XLR, CFG(xlr20ksps), 255 },
};
#define NPHY (sizeof phys / sizeof phys[0])

/* ---------- driver plumbing ---------- */
static volatile uint32_t ev;
static volatile uint8_t last_err, last_cmd;
static void cb(TRX_Host_Handle h, uintptr_t c, TRX_Request *req, uint64_t e, uintptr_t a)
{
    (void)h; (void)c; (void)a;
    if ((e & TRX_EventLastStatusError) && req) last_err = ((TRX_Request_LastStatus *)req)->status;
    if ((e & TRX_EventCmdStatus) && req) last_cmd = ((TRX_Request_CommandStatus *)req)->status;
    ev |= (uint32_t)e;
}
/* wait for any bit of mask; returns microseconds waited, or 0xFFFFFFFF on timeout or error */
static uint32_t wait_ev(uint32_t mask, uint32_t timeout_ms)
{
    uint32_t t0 = frl_now_us();
    while (!(ev & (mask | TRX_EventLastStatusError))) {
        if (frl_now_us() - t0 > timeout_ms * 1000u) return 0xFFFFFFFFu;
    }
    uint32_t dt = frl_now_us() - t0;
    if ((ev & TRX_EventLastStatusError) && !(ev & mask)) return 0xFFFFFFFFu;
    return dt;
}

extern volatile uint8_t spiTransportDebugFlags, spiTxStartedNotCcb, spiCcbNotParsed;
static void fail_line(unsigned idx, uint16_t len, unsigned n, const char *step)
{
    puts_("FAIL,"); putu(idx); puts_(","); putu(len); puts_(","); putu(n); puts_(","); puts_(step);
    puts_(",ev=0x"); puthex(ev, 8); puts_(",err="); putu(last_err); puts_(",cmd=0x"); puthex(last_cmd, 2);
    puts_(",flags=0x"); puthex(spiTransportDebugFlags, 2); puts_(",txNotCcb="); putu(spiTxStartedNotCcb); puts_(",ccbNotParsed="); putu(spiCcbNotParsed); puts_("\r\n");
}
static TRX_Request_CommandStore cmd;
static uint8_t pkt[sizeof(TRX_PayloadHeader) + 256];

/* header + "FRL P<i> n=<k> " + filler, total payload length len (bytes after the header) */
static uint16_t build(const Phy *p, unsigned idx, uint32_t n, uint16_t len)
{
    uint16_t h;
    memset(pkt, 0, sizeof pkt);
    if (p->hdr == H_GENFSK) { pkt[0] = (uint8_t)len; h = 1; }
    else {
        h = sizeof(TRX_PayloadHeader);
        if (p->hdr == H_SUNFSK) { TRX_PayloadHeader_SunFSK *x = (void *)pkt; x->length = len; x->modulation = TRX_PayloadHeader_Modulation_FSK; x->fcs_mode = 0; x->whitening = 1; }
        if (p->hdr == H_OQPSK) { TRX_PayloadHeader_SUNOQPSK *x = (void *)pkt; x->length = len; x->rate_mode = TRX_PayloadHeader_SunOQPSK_RateMode3_50kbps; x->modulation = TRX_PayloadHeader_Modulation_OQPSK; }
        if (p->hdr == H_OFDM) { TRX_PayloadHeader_SunOFDM *x = (void *)pkt; x->length = len; x->rate = p->rate; x->scrambler = 0; x->modulation = TRX_PayloadHeader_Modulation_OFDM; }
        if (p->hdr == H_OFDMLR) { TRX_PayloadHeader_OFDMLR *x = (void *)pkt; x->length = (uint8_t)len; x->rate = p->rate; x->modulation = TRX_PayloadHeader_Modulation_OFDM; }
    }
    char t[24]; int k = 0; const char *s = "FRL P"; while (*s) t[k++] = *s++;
    if (idx >= 10) t[k++] = (char)('0' + idx / 10); t[k++] = (char)('0' + idx % 10);
    t[k++] = ' '; t[k++] = 'n'; t[k++] = '=';
    char nb[11]; int j = 10; nb[j] = 0; do { nb[--j] = (char)('0' + n % 10); n /= 10; } while (n); while (nb[j]) t[k++] = nb[j++];
    t[k++] = ' ';
    for (uint16_t i = 0; i < len; i++) pkt[h + i] = (uint8_t)(i < k ? t[i] : 'a' + (i % 26));
    return (uint16_t)(h + len);
}

static TRX_Host_Params hp = { .bitRate = 1000000u, .generalCb = cb, .arg = 0 };
/* open the driver (it resets the radio), erase its stored configs, store the front-end config */
static TRX_Host_Handle bring_up(TRX_Host_Handle old)
{
    if (old) TRX_Host_close(old);
    uint32_t t0 = frl_now_us();
    TRX_Host_Handle h = TRX_Host_open(&hp);
    if (!h) return 0;
    puts_("open_us,"); putu(frl_now_us() - t0); puts_("\r\n");
    ev = 0; TRX_Host_eraseNv(h, cb); wait_ev(TRX_EventNvEraseComplete, 3000); ev = 0;
    t0 = frl_now_us();
    TRX_Host_storeConfig(h, TRX_CONFIG_ID_FRONTEND, (uint8_t *)LRF_frontendRegConfig_wisun, (uint16_t)LRF_frontendRegConfig_wisun_byteCount,
                         0xFEFEFEFEu, true, cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
    wait_ev(TRX_EventConfigStoreComplete, 3000); ev = 0;
    TRX_Host_persistConfig(h, TRX_CONFIG_ID_FRONTEND); wait_ev(TRX_EventConfigPersistComplete, 3000); ev = 0;
    puts_("frontend_us,"); putu(frl_now_us() - t0); puts_("\r\n");
    return h;
}

#ifndef NO_MATRIX_MAIN
int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN | RADIO_RST_PIN);
    clock_init();
    ms(100);
    puts_("\r\nphy_matrix: CC1407P profiles from TI rf_diagnostics, host LP-MSPM0G5187, 915 MHz +14 dBm\r\n");
    puts_("timer ticks per ms: "); putu(ticks_per_ms); puts_("\r\n");
    TRX_Host_Handle h = bring_up(0);
    if (!h) { puts_("open FAILED\r\n"); for (;;) { } }
#ifdef SPI_SWEEP
    /* SPI clock sweep: the fastest OFDM profile and 50 kbps GFSK, 20 packets back to back at each SPI clock.
     * BUSCLK is 32 MHz; SPI clock = 32 MHz / (2 x (div + 1)). The radio allows up to 12 MHz. */
    static const uint8_t divs[] = { 15, 7, 3, 1 };
    static const unsigned prof[] = { 2, 13 };
    puts_("SWEEP,profile,spi_khz,payload,sent,failed,burst_ms,pkts_per_s,payload_kbps,store_us_avg,tx_us_avg,spi_us_per_pkt\r\n");
    for (unsigned pi = 0; pi < 2; pi++) {
        unsigned i = prof[pi]; const Phy *p = &phys[i];
        for (unsigned d = 0; d < sizeof divs; d++) {
            DL_SPI_disable(SPI_0_INST); DL_SPI_setBitRateSerialClockDivider(SPI_0_INST, divs[d]); DL_SPI_enable(SPI_0_INST);
            h = bring_up(h); if (!h) { puts_("open FAILED\r\n"); for (;;) { } }
            ev = 0; TRX_Host_storeConfig(h, 1u, (uint8_t *)p->cfg, (uint16_t)*p->cfgBytes, 0xF0000000u + i, true, cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
            wait_ev(TRX_EventConfigStoreComplete, 5000); ev = 0; TRX_Host_persistConfig(h, 1u); wait_ev(TRX_EventConfigPersistComplete, 5000); ev = 0;
            TRX_DeviceConfigData dc = { .dioConfig = { .dio0 = DIO0_UNCHANGED, .dio2 = DIO2_HIGH_PA, .dio3 = DIO3_LOW_PA_AND_LNA, .dio4 = DIO4_UNCHANGED, .dio5 = DIO5_UNCHANGED, .dio6 = DIO6_UNCHANGED },
                .clockConfig = TRX_ClockConfig_XOSC, .powerMode = { .dwellTimeUs = 0u, .powerPolicy = (TRX_PowerPolicy)0 }, .rfMode = (TRX_RfMode)p->rfMode };
            TRX_Host_storeDeviceConfig(h, dc, cb); wait_ev(TRX_EventDeviceConfigStoreComplete, 3000); ev = 0;
            memset(&cmd, 0, sizeof cmd);
            cmd.slot = 0; cmd.cmd_id = TRX_RadioCommand_Transmit; cmd.trigger = Command_Trigger_Immediate; cmd.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
            cmd.params.tx.phy0.config_id = 1u; cmd.params.tx.phy0.option_mask = p->opt; cmd.params.tx.pa = TRX_TxPa_High; cmd.params.tx.frequency = 915000u; cmd.params.tx.modem = p->modem;
            cmd.params.tx.power = (TRX_TxPower){ .fraction = 0, .dBm = 14 };
            TRX_Host_storeCmds(h, &cmd, 1u, NULL, cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete); wait_ev(TRX_EventCmdStoreComplete, 3000); ev = 0;
            static const uint16_t lens[2] = { 60, 200 };
            for (unsigned s2 = 0; s2 < 2; s2++) {
                uint16_t len = lens[s2]; uint32_t sent = 0, failed = 0, st = 0, txs = 0;
                uint32_t u0 = hb_spi_us, t0 = frl_now_us();
                for (unsigned n = 0; n < 20; n++) {
                    uint16_t tot = build(p, i, 1000u * (d + 1) + n, len);
                    uint32_t a = frl_now_us(); TRX_Host_Status sst;
                    while ((sst = TRX_Host_storeStream(h, 0, pkt, tot, Stream_Retention_Flush_Streaming, cb, TRX_EventStreamStoreComplete)) != TRX_Host_Success && frl_now_us() - a < 2000000u) {
                        if (sst == TRX_Host_No_Resources) hb_int_isr();
                        delay_cycles(CPUCLK_FREQ / 10000);
                    }
                    if (sst != TRX_Host_Success || wait_ev(TRX_EventStreamStoreComplete, 3000) == 0xFFFFFFFFu) { failed++; ev = 0; continue; }
                    uint32_t b = frl_now_us(); ev = 0; last_cmd = 0xFF;
                    TRX_Host_submitCmd(h, 0);
                    if (wait_ev(TRX_EventCmdStatus, 15000) == 0xFFFFFFFFu || last_cmd != TRX_CommandStatus_Finished) { failed++; continue; }
                    uint32_t c = frl_now_us(); sent++; st += b - a; txs += c - b;
                }
                uint32_t burst = frl_now_us() - t0; uint32_t pk = sent + failed ? sent + failed : 1;
                puts_("SWEEP,"); putu(i); puts_(","); putu(32000u / (2u * (divs[d] + 1u))); puts_(","); putu(len); puts_(","); putu(sent); puts_(","); putu(failed); puts_(",");
                putu(burst / 1000u); puts_(","); putu(burst ? sent * 1000000u / burst : 0); puts_(","); putu(burst ? (uint32_t)((uint64_t)sent * len * 8u * 1000u / burst) : 0); puts_(",");
                putu(sent ? st / sent : 0); puts_(","); putu(sent ? txs / sent : 0); puts_(","); putu((hb_spi_us - u0) / pk); puts_("\r\n");
            }
        }
    }
    puts_("DONE\r\n"); for (;;) { }
#endif
#ifdef NV_TEST
    /* Store and persist the same 1,776-byte PHY config again and again (new reference each time), then send one
     * 20-byte packet; report at which persist the radio stops taking packets. */
    puts_("NVTEST,k,store_ms,persist_ms,stream_ok,tx_ok\r\n");
    for (unsigned k = 1; k <= 12; k++) {
        const Phy *p = &phys[0];
        uint32_t a = frl_now_us();
        ev = 0; TRX_Host_storeConfig(h, 1u, (uint8_t *)p->cfg, (uint16_t)*p->cfgBytes, 0xE0000000u + k, true, cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
        uint32_t ws = wait_ev(TRX_EventConfigStoreComplete, 5000); uint32_t b = frl_now_us(); ev = 0;
        TRX_Host_persistConfig(h, 1u); uint32_t wp = wait_ev(TRX_EventConfigPersistComplete, 5000); uint32_t c = frl_now_us(); ev = 0;
        memset(&cmd, 0, sizeof cmd);
        cmd.slot = 0; cmd.cmd_id = TRX_RadioCommand_Transmit; cmd.trigger = Command_Trigger_Immediate; cmd.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
        cmd.params.tx.phy0.config_id = 1u; cmd.params.tx.phy0.option_mask = p->opt; cmd.params.tx.pa = TRX_TxPa_High; cmd.params.tx.frequency = 915000u; cmd.params.tx.modem = p->modem;
        cmd.params.tx.power = (TRX_TxPower){ .fraction = 0, .dBm = 14 };
        TRX_Host_storeCmds(h, &cmd, 1u, NULL, cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete); wait_ev(TRX_EventCmdStoreComplete, 3000); ev = 0;
        uint16_t tot = build(p, 0, k, 20);
        TRX_Host_storeStream(h, 0, pkt, tot, Stream_Retention_Flush_Streaming, cb, TRX_EventStreamStoreComplete);
        uint32_t w1 = wait_ev(TRX_EventStreamStoreComplete, 3000); ev = 0; last_cmd = 0xFF; uint32_t w2 = 0xFFFFFFFFu;
        if (w1 != 0xFFFFFFFFu) { TRX_Host_submitCmd(h, 0); w2 = wait_ev(TRX_EventCmdStatus, 3000); }
        puts_("NVTEST,"); putu(k); puts_(","); putu(ws == 0xFFFFFFFFu ? 9999 : (b - a) / 1000u); puts_(","); putu(wp == 0xFFFFFFFFu ? 9999 : (c - b) / 1000u);
        puts_(","); putu(w1 != 0xFFFFFFFFu); puts_(","); putu(w2 != 0xFFFFFFFFu && last_cmd == TRX_CommandStatus_Finished); puts_("\r\n");
    }
    puts_("DONE\r\n"); for (;;) { }
#endif

    puts_("CSV,idx,profile,payload,cfg_ms,cfg_bytes,sent,failed,store_us_avg,tx_us_min,tx_us_avg,tx_us_max,spi_bytes_per_pkt,spi_us_per_pkt,last_status,last_err,store_retries,retry_us_avg\r\n");
    static const uint16_t sizes[2] = { 20, 200 };
#ifdef RX_CHECK
    /* receive check: loop the profiles a CC1354P10 can also receive, 20 numbered 60-byte packets each, forever */
    static const uint8_t order[] = { 0, 1, 18, 13 };
#define ORDER(k) order[(k) % sizeof order]
#define MORE(k) 1
#else
#define ORDER(k) (k)
#define MORE(k) ((k) < NPHY)
#endif
    for (unsigned k = 0; MORE(k); k++) {
        unsigned i = ORDER(k);
        const Phy *p = &phys[i];
        /* switch the radio to this profile */
        uint32_t c0 = frl_now_us(); bool cok = true;
#if RESET_ON_FAMILY
        /* The radio keeps the image of its first command after reset: a change of modem (FSK, OFDM, O-QPSK) or RF mode
         * needs a reset first, or the transmit ends with status 0x8A (unknown op). Storing the device config or calling
         * utilSetRfMode did not change that (runs 7 and 8). */
        static int prev_modem = -1, prev_mode = -1;
        if (prev_modem >= 0 && (RESET_EACH || prev_modem != (int)p->modem || prev_mode != (int)p->rfMode)) {
            uint32_t r0 = frl_now_us(); h = bring_up(h);
            puts_("RESET_FAMILY,"); putu(i); puts_(","); putu((frl_now_us() - r0) / 1000u); puts_(" ms\r\n");
            if (!h) { puts_("DONE\r\n"); for (;;) { } }
            c0 = frl_now_us();   /* the reset is reported on its own line; cfg_ms stays the profile switch alone */
        }
        prev_modem = (int)p->modem; prev_mode = (int)p->rfMode;
#endif
#if MATRIX_ERASE_EACH
        /* The radio keeps a stored-but-unpersisted config in its SPI blocks (a 1,776-byte config leaves no room for a
         * 200-byte packet), and its config storage holds only about four persisted 1,776-byte copies. So: erase it,
         * put the front-end config back, then store and persist this profile. */
        ev = 0; TRX_Host_eraseNv(h, cb); if (wait_ev(TRX_EventNvEraseComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "eraseNv"); } ev = 0;
        TRX_Host_storeConfig(h, TRX_CONFIG_ID_FRONTEND, (uint8_t *)LRF_frontendRegConfig_wisun, (uint16_t)LRF_frontendRegConfig_wisun_byteCount,
                             0xFEFEFEFEu, true, cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
        if (cok && wait_ev(TRX_EventConfigStoreComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "feStore"); } ev = 0;
        if (cok) { TRX_Host_persistConfig(h, TRX_CONFIG_ID_FRONTEND); if (wait_ev(TRX_EventConfigPersistComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "fePersist"); } } ev = 0;
#endif
        TRX_Host_storeConfig(h, 1u, (uint8_t *)p->cfg, (uint16_t)*p->cfgBytes, 0xF0000000u + i, true, cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
        if (cok && wait_ev(TRX_EventConfigStoreComplete, 5000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "phyStore"); } ev = 0;
#if MATRIX_PERSIST   /* run 2 persisted every profile and the radio stopped taking packets on every fifth switch */
        if (cok) { TRX_Host_persistConfig(h, 1u); if (wait_ev(TRX_EventConfigPersistComplete, 5000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "phyPersist"); } } ev = 0;
#endif
#if !DEVCFG_FIRST
        /* TI rf_diagnostics order: PHY config stored and persisted first, then the device config (RF mode) */
        TRX_DeviceConfigData dc = {
            .dioConfig = { .dio0 = DIO0_UNCHANGED, .dio2 = DIO2_HIGH_PA, .dio3 = DIO3_LOW_PA_AND_LNA, .dio4 = DIO4_UNCHANGED, .dio5 = DIO5_UNCHANGED, .dio6 = DIO6_UNCHANGED },
            .clockConfig = TRX_ClockConfig_XOSC, .powerMode = { .dwellTimeUs = 0u, .powerPolicy = (TRX_PowerPolicy)0 }, .rfMode = (TRX_RfMode)p->rfMode };
        ev = 0; TRX_Host_storeDeviceConfig(h, dc, cb); if (wait_ev(TRX_EventDeviceConfigStoreComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "deviceConfig"); } ev = 0;
#if SET_RF_MODE
        /* storing the device config alone left the radio on the old image at every RF-mode change (status 0x8A, unknown op) */
        if (cok) { TRX_Host_utilSetRfMode(h, (TRX_RfMode)p->rfMode, cb); if (wait_ev(TRX_EventSetRfModeComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "setRfMode"); } } ev = 0;
#endif
#endif
        memset(&cmd, 0, sizeof cmd);
        cmd.slot = 0; cmd.cmd_id = TRX_RadioCommand_Transmit; cmd.trigger = Command_Trigger_Immediate; cmd.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
        cmd.params.tx.phy0.config_id = 1u; cmd.params.tx.phy0.option_mask = p->opt; cmd.params.tx.pa = TRX_TxPa_High; cmd.params.tx.stream_id = 0;
        cmd.params.tx.frequency = 915000u; cmd.params.tx.modem = p->modem;
        cmd.params.tx.power = (TRX_TxPower){ .fraction = 0, .dBm = 14 };
        TRX_Host_storeCmds(h, &cmd, 1u, NULL, cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
        if (cok && wait_ev(TRX_EventCmdStoreComplete, 3000) == 0xFFFFFFFFu) { cok = false; fail_line(i, 0, 0, "cmdStore"); } ev = 0;
        uint32_t cfg_us = frl_now_us() - c0; bool any_fail = false;
        for (unsigned s = 0; s < 2; s++) {
            uint16_t len = sizes[s]; if (len > p->maxPayload) len = p->maxPayload;
            unsigned N = (s == 0) ? 10 : 5;
#ifdef RX_CHECK
            if (s == 1) break;
            len = 60; N = 20;
#endif
            uint32_t sent = 0, failed = 0, st_sum = 0, tx_min = 0xFFFFFFFFu, tx_max = 0, tx_sum = 0;
            uint32_t b0 = hb_spi_bytes, u0 = hb_spi_us; uint8_t lastst = 0; uint32_t retry_n = 0, retry_us = 0;
            last_err = 0;
            for (unsigned n = 0; n < N && cok; n++) {
#ifdef RX_CHECK
                ms(50);
                uint16_t tot = build(p, i, k * 100u + n, len);
#else
                uint16_t tot = build(p, i, n, len);
#endif
                ev = 0; last_cmd = 0xFF;
                uint32_t a = frl_now_us();
                /* The driver refuses a new stream while the radio still holds the last one; retry until it
                 * accepts. The retry time is the host's turnaround: how soon the next packet can be queued. */
                TRX_Host_Status sst; uint32_t tries = 0;
                while ((sst = TRX_Host_storeStream(h, 0, pkt, tot, Stream_Retention_Flush_Streaming, cb, TRX_EventStreamStoreComplete)) != TRX_Host_Success
                       && frl_now_us() - a < 2000000u) {
                    /* out of SPI buffers: they return when the radio's next message acknowledges the earlier blocks,
                     * so ask for that message now (as the interrupt would) instead of waiting for the 500 ms watchdog */
                    if (sst == TRX_Host_No_Resources && KICK_ON_REFUSAL) hb_int_isr();
                    tries++; delay_cycles(CPUCLK_FREQ / 10000);
                }
                if (sst != TRX_Host_Success) { failed++; puts_("REFUSED,"); putu(i); puts_(","); putu(len); puts_(","); putu(n); puts_(",status="); putu(sst); puts_("\r\n"); if (failed >= 2) break; continue; }
                if (tries) { retry_n++; retry_us += frl_now_us() - a; }
                uint32_t w = wait_ev(TRX_EventStreamStoreComplete, 3000);
                uint32_t b = frl_now_us(); ev = 0;
                if (w == 0xFFFFFFFFu) { failed++; fail_line(i, len, n, "streamStore"); if (failed >= 2) break; continue; }
                TRX_Host_submitCmd(h, 0);
                uint32_t w2 = wait_ev(TRX_EventCmdStatus, 15000);
                uint32_t c = frl_now_us();
                lastst = last_cmd;
                if (w2 == 0xFFFFFFFFu || last_cmd != TRX_CommandStatus_Finished) { failed++; fail_line(i, len, n, w2 == 0xFFFFFFFFu ? "txTimeout" : "txStatus"); if (failed >= 2) break; continue; }
                sent++; st_sum += b - a; uint32_t tx = c - b; tx_sum += tx; if (tx < tx_min) tx_min = tx; if (tx > tx_max) tx_max = tx;
            }
            uint32_t pk = sent + failed ? sent + failed : 1;
            any_fail |= (failed > 0) || !cok;
            puts_("CSV,"); putu(i); puts_(","); puts_(p->name); puts_(","); putu(len); puts_(","); putu(cfg_us / 1000u); puts_(","); putu(*p->cfgBytes); puts_(",");
            putu(sent); puts_(","); putu(failed + (cok ? 0 : N)); puts_(","); putu(sent ? st_sum / sent : 0); puts_(",");
            putu(sent ? tx_min : 0); puts_(","); putu(sent ? tx_sum / sent : 0); puts_(","); putu(tx_max); puts_(",");
            putu((hb_spi_bytes - b0) / pk); puts_(","); putu((hb_spi_us - u0) / pk); puts_(",0x"); puthex(lastst, 2); puts_(","); putu(last_err); puts_(","); putu(retry_n); puts_(","); putu(retry_n ? retry_us / retry_n : 0); puts_("\r\n");
        }
        if (any_fail) {
            uint32_t r0 = frl_now_us();
            h = bring_up(h);
            puts_("RECOVER,"); putu(i); puts_(","); puts_(h ? "ok" : "FAILED"); puts_(","); putu((frl_now_us() - r0) / 1000u); puts_(" ms\r\n");
            if (!h) { puts_("DONE\r\n"); for (;;) { } }
        }
    }
    puts_("DONE\r\n");
    for (;;) { }
}
#endif /* NO_MATRIX_MAIN */
