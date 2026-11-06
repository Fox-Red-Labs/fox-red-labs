/*
 * CC1407P radio lab on the LP-MSPM0G5187: one image, driven by text commands on the backchannel
 * UART (115200 8N1), so a script on the Mac can run receive, ping, power, RSSI-scan and carrier-sense
 * tests against a second radio. Profiles and helpers come from phy_matrix.c (TI's rf_diagnostics
 * PHY list). Every command answers with lines that start with a fixed word and end with "DONE ...".
 *
 *   P <idx> [mask]           radio reset, then profile idx (0..22) with an optional option mask (hex)
 *   F <kHz>                  frequency, default 915000
 *   W <dBm> <pa>             transmit power and amplifier (0 low, 1 high), default 14 1
 *   T <n> <len> <gap_ms>     send n numbered packets "FRL P<idx> n=<k>" padded to len bytes
 *   R <ms>                   receive for ms; prints each packet and the radio's counters
 *   G <n> <len> <tmo_ms> <gap_ms>   ping: send "FRL PING k", wait tmo_ms for "FRL PONG k r=<rssi>"
 *   E <ms>                   echo: answer each "FRL PING k" with "FRL PONG k r=<rssi>" for ms
 *   S <from> <to> <step>     RSSI across a band (kHz), receiver on at each step
 *   C <thr_dBm> <tmo_ms>     carrier sense (energy), prints the radio's idle or busy status
 *   V                        radio firmware version
 */
#define NO_MATRIX_MAIN
#include "phy_matrix.c"

/* ---------- serial input ---------- */
static char line[96]; static unsigned lpos;
static int getline_(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        char c = (char)DL_UART_Main_receiveData(UART_0_INST);
        if (c == '\r' || c == '\n') { if (lpos) { line[lpos] = 0; lpos = 0; return 1; } continue; }
        if (lpos < sizeof line - 1) line[lpos++] = c;
    }
    return 0;
}
static const char *arg_p;
static int32_t argi(int32_t dflt)
{
    while (*arg_p == ' ') arg_p++;
    if (!*arg_p) return dflt;
    int neg = 0; int32_t v = 0; int hex = 0;
    if (*arg_p == '-') { neg = 1; arg_p++; }
    if (arg_p[0] == '0' && (arg_p[1] == 'x' || arg_p[1] == 'X')) { hex = 1; arg_p += 2; }
    while (*arg_p && *arg_p != ' ') {
        char c = *arg_p++;
        if (hex) v = v * 16 + (c <= '9' ? c - '0' : (c | 32) - 'a' + 10); else v = v * 10 + (c - '0');
    }
    return neg ? -v : v;
}
static void puti(int32_t v) { if (v < 0) { putc_('-'); v = -v; } putu((uint32_t)v); }

/* ---------- state ---------- */
static TRX_Host_Handle h;
static int cur = -1; static uint16_t cur_mask;
static uint32_t freq_khz = 915000u; static int8_t pwr = 14; static uint8_t pa = 1;
static volatile int8_t rssi_now; static volatile uint8_t got_rssi;
static volatile TRX_Request_CommandStatus_Params_Rx rxstat;
static volatile uint8_t rx_done, rx_status;

/* received packets, copied out of the driver's buffer in its callback */
#define RING 16
#define RING_D 224
static struct { uint16_t len; int8_t rssi; uint32_t ts, t_host; uint8_t d[RING_D]; } ring[RING];
static volatile uint32_t rhead, rtail, rdrop, rover;
static uint8_t rxbuf[sizeof(TRX_PayloadHeader) + 300 + 1 + 4];

static void lab_cb(TRX_Host_Handle hh, uintptr_t c, TRX_Request *req, uint64_t e, uintptr_t a)
{
    cb(hh, c, req, e, a);
    if ((e & TRX_EventCmdStatus) && req) {
        TRX_Request_CommandStatus *s = (TRX_Request_CommandStatus *)req;
        if (s->command_id == TRX_RadioCommand_Receive) { memcpy((void *)&rxstat, s->params, sizeof rxstat); rx_status = s->status; rx_done = 1; }
    }
    if ((e & TRX_EventGetRSSIComplete) && req) { rssi_now = (int8_t)((TRX_Request_LastStatus *)req)->params[0]; got_rssi = 1; }
}
static void rx_cb(TRX_Host_Handle hh, uintptr_t data, TRX_Request *req, uint64_t e, uintptr_t a)
{
    (void)hh; (void)req; (void)a;
    if (e & TRX_EventStreamStoreOverflow) { rover++; return; }
    if (!(e & TRX_EventFinalStreamStoreReceived) || !data || cur < 0) return;
    const uint8_t *d = (const uint8_t *)data; uint16_t len; const uint8_t *pl;
    if (phys[cur].hdr == H_GENFSK) { len = d[0]; pl = d + 1; }
    else { const TRX_PayloadHeader *ph = (const TRX_PayloadHeader *)d; len = phys[cur].hdr == H_OFDMLR ? ph->ofdmlr.length : ph->length; pl = d + sizeof(TRX_PayloadHeader); }
    if (len > 300) len = 300;
    if (rhead - rtail >= RING) { rdrop++; return; }
    unsigned i = rhead % RING;
    ring[i].len = len; ring[i].rssi = (int8_t)pl[len];
    memcpy(&ring[i].ts, pl + len + 1, 4); ring[i].t_host = frl_now_us();
    memcpy(ring[i].d, pl, len < RING_D ? len : RING_D);
    rhead++;
}

/* ---------- radio set-up ---------- */
static uint8_t chain_rx;   /* 1: the transmit chains to the receive in slot 1 on the radio itself */
static void store_tx(void)
{
    memset(&cmd, 0, sizeof cmd);
    if (chain_rx) { cmd.enable_on_true = 1; cmd.slot_on_true = 1; cmd.enable_on_false = 1; cmd.slot_on_false = 1; cmd.chain_trigger = Command_Trigger_Immediate; }
    cmd.slot = 0; cmd.cmd_id = TRX_RadioCommand_Transmit; cmd.trigger = Command_Trigger_Immediate; cmd.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
    cmd.params.tx.phy0.config_id = 1u; cmd.params.tx.phy0.option_mask = cur_mask; cmd.params.tx.pa = pa ? TRX_TxPa_High : TRX_TxPa_Low; cmd.params.tx.stream_id = 0;
    cmd.params.tx.frequency = freq_khz; cmd.params.tx.modem = phys[cur].modem;
    cmd.params.tx.power = (TRX_TxPower){ .fraction = 0, .dBm = pwr };
    ev = 0; TRX_Host_storeCmds(h, &cmd, 1u, NULL, lab_cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
    wait_ev(TRX_EventCmdStoreComplete, 3000); ev = 0;
}
static void store_rx(uint32_t timeout_us, int repeat)
{
    static TRX_Request_CommandStore rc;
    memset(&rc, 0, sizeof rc);
    rc.slot = 1; rc.cmd_id = TRX_RadioCommand_Receive; rc.trigger = Command_Trigger_Immediate; rc.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
    rc.params.rx.phy0.config_id = 1u; rc.params.rx.phy0.option_mask = cur_mask; rc.params.rx.stream_id = 1;
    rc.params.rx.frequency = freq_khz; rc.params.rx.modem = phys[cur].modem;
    rc.params.rx.timeout = timeout_us; rc.params.rx.repeat = (uint8_t)repeat; rc.params.rx.search_strategy = 0;
    ev = 0; TRX_Host_storeCmds(h, &rc, 1u, NULL, lab_cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
    wait_ev(TRX_EventCmdStoreComplete, 3000); ev = 0;
}
static int configure(int i, int32_t mask)
{
    const Phy *p = &phys[i];
    uint32_t t0 = frl_now_us();
    h = bring_up(h); if (!h) return 0;
    cur = i; cur_mask = mask >= 0 ? (uint16_t)mask : p->opt;
    TRX_Host_storeConfig(h, 1u, (uint8_t *)p->cfg, (uint16_t)*p->cfgBytes, 0xF0000000u + i, true, lab_cb, TRX_EventConfigStoreComplete | TRX_EventConfigPersistComplete);
    if (wait_ev(TRX_EventConfigStoreComplete, 5000) == 0xFFFFFFFFu) return 0; ev = 0;
    TRX_Host_persistConfig(h, 1u); if (wait_ev(TRX_EventConfigPersistComplete, 5000) == 0xFFFFFFFFu) return 0; ev = 0;
    TRX_DeviceConfigData dc = { .dioConfig = { .dio0 = DIO0_UNCHANGED, .dio2 = DIO2_HIGH_PA, .dio3 = DIO3_LOW_PA_AND_LNA, .dio4 = DIO4_UNCHANGED, .dio5 = DIO5_UNCHANGED, .dio6 = DIO6_UNCHANGED },
        .clockConfig = TRX_ClockConfig_XOSC, .powerMode = { .dwellTimeUs = 0u, .powerPolicy = (TRX_PowerPolicy)0 }, .rfMode = (TRX_RfMode)p->rfMode };
    TRX_Host_storeDeviceConfig(h, dc, lab_cb); if (wait_ev(TRX_EventDeviceConfigStoreComplete, 3000) == 0xFFFFFFFFu) return 0; ev = 0;
    TRX_Host_registerRxStream(h, 1, rxbuf, sizeof rxbuf, rx_cb, TRX_EventFinalStreamStoreReceived | TRX_EventStreamStoreOverflow);
    store_tx();
    puts_("cfg_ms,"); putu((frl_now_us() - t0) / 1000u); puts_("\r\n");
    return 1;
}

/* payload: the profile's header, then text, then filler up to len bytes */
static uint16_t build_text(const char *txt, uint16_t len)
{
    uint16_t hl = build(&phys[cur], (unsigned)cur, 0, len);   /* header and filler */
    uint16_t off = (uint16_t)(hl - len);
    uint16_t k = 0; while (txt[k] && k < len) { pkt[off + k] = (uint8_t)txt[k]; k++; }
    if (k < len) pkt[off + k] = ' ';
    return hl;
}
static int send(uint16_t tot, uint32_t *tx_us)
{
    uint32_t a = frl_now_us(); TRX_Host_Status sst;
    ev = 0;
    while ((sst = TRX_Host_storeStream(h, 0, pkt, tot, Stream_Retention_Flush_Streaming, lab_cb, TRX_EventStreamStoreComplete)) != TRX_Host_Success
           && frl_now_us() - a < 2000000u) { if (sst == TRX_Host_No_Resources) hb_int_isr(); delay_cycles(CPUCLK_FREQ / 10000); }
    if (sst != TRX_Host_Success || wait_ev(TRX_EventStreamStoreComplete, 3000) == 0xFFFFFFFFu) return 0;
    uint32_t b = frl_now_us(); ev = 0; last_cmd = 0xFF;
    TRX_Host_submitCmd(h, 0);
    if (wait_ev(TRX_EventCmdStatus, 15000) == 0xFFFFFFFFu || last_cmd != TRX_CommandStatus_Finished) return 0;
    if (tx_us) *tx_us = frl_now_us() - b;
    return 1;
}
/* receive until a packet or timeout; leaves packets in the ring */
static uint8_t rx_once(uint32_t timeout_us, int repeat)
{
    store_rx(timeout_us, repeat);
    ev = 0; last_cmd = 0xFF; memset((void *)&rxstat, 0, sizeof rxstat);
    TRX_Host_submitCmd(h, 1);
    wait_ev(TRX_EventCmdStatus, timeout_us / 1000u + 2000u);
    return last_cmd;
}
static void print_pkt(unsigned i)
{
    puts_("RX,"); putu(ring[i].len); puts_(","); puti(ring[i].rssi); puts_(","); putu(ring[i].ts); puts_(",");
    unsigned n = ring[i].len < 64 ? ring[i].len : 64;
    for (unsigned k = 0; k < n; k++) { char c = (char)ring[i].d[k]; putc_(c >= 32 && c < 127 && c != ',' ? c : '.'); }
    puts_("\r\n");
}
static int parse_pong(unsigned i, uint32_t k, int32_t *r)
{
    const char *s = (const char *)ring[i].d; char want[24]; int w = 0; const char *pre = "FRL PONG ";
    while (*pre) want[w++] = *pre++;
    char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = k; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) want[w++] = nb[j++];
    want[w++] = ' '; want[w++] = 'r'; want[w++] = '=';
    if (ring[i].len < (unsigned)w) return 0;
    for (int q = 0; q < w; q++) if (s[q] != want[q]) return 0;
    arg_p = s + w; *r = argi(0); return 1;
}


/* ---------- demos: Sensors BoosterPack light and magnet sensors, LaunchPad buttons and LEDs ---------- */
/* LaunchPad (SLAU967 Table 2-1): red LED PA0, RGB red PA17, green PA24, blue PB13; S1 PA18 (pressed = high), S2 PA7 (pressed = low) */
static uint32_t i2c_err;
static int i2c_tx(uint8_t addr, const uint8_t *d, uint8_t n)
{
    uint32_t to;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    DL_I2CC_flushTXFIFO(I2C_INST);
    if (n) DL_I2CC_fillTXFIFO(I2C_INST, (uint8_t *)d, n);
    DL_I2CC_startTransfer(I2C_INST, addr, DL_I2CC_DIRECTION_TX, n);
    for (to = 0; to < 400000 && (DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_BUSY); to++) { }
    int err = (DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_ERROR) != 0;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    if (err) { DL_I2CC_flushTXFIFO(I2C_INST); i2c_err++; }
    return err;
}
static int i2c_rx(uint8_t addr, uint8_t *d, uint8_t n)
{
    uint32_t to;
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    DL_I2CC_startTransfer(I2C_INST, addr, DL_I2CC_DIRECTION_RX, n);
    for (uint8_t i = 0; i < n; i++) {
        for (to = 0; to < 400000 && DL_I2CC_isRXFIFOEmpty(I2C_INST); to++) { }
        if (DL_I2CC_isRXFIFOEmpty(I2C_INST)) { i2c_err++; return -1; }
        d[i] = DL_I2CC_receiveData(I2C_INST);
    }
    for (to = 0; to < 200000 && !(DL_I2CC_getStatus(I2C_INST) & DL_I2CC_STATUS_IDLE); to++) { }
    return 0;
}
static int rd16(uint8_t a, uint8_t reg, uint16_t *out) { uint8_t b[2] = { 0, 0 }; if (i2c_tx(a, &reg, 1) || i2c_rx(a, b, 2)) return -1; *out = (uint16_t)((b[0] << 8) | b[1]); return 0; }
static int wr16(uint8_t a, uint8_t reg, uint16_t v) { uint8_t b[3] = { reg, (uint8_t)(v >> 8), (uint8_t)v }; return i2c_tx(a, b, 3); }
static void sensors_init(void)
{
    DL_GPIO_clearPins(HDC_PWR_PORT, HDC_PWR_HDC_PIN); DL_GPIO_clearPins(DRV_PWR_PORT, DRV_PWR_DRV_PIN);
    DL_GPIO_setPins(OPT_PWR_PORT, OPT_PWR_OPT_PIN); ms(50);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SDA, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_IOMUX_SCL, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_8 | DL_GPIO_PIN_9); DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_8);
    for (int i = 0; i < 9; i++) { DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_9); delay_cycles(1600); DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_9); delay_cycles(1600); }
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_8); delay_cycles(1600); DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_8); delay_cycles(1600);
    SYSCFG_DL_GPIO_init(); SYSCFG_DL_I2C_init();
    DL_GPIO_clearPins(HDC_PWR_PORT, HDC_PWR_HDC_PIN); DL_GPIO_clearPins(DRV_PWR_PORT, DRV_PWR_DRV_PIN);
    DL_GPIO_setPins(OPT_PWR_PORT, OPT_PWR_OPT_PIN); ms(50);
    wr16(0x44, 0x01, 0xCE10);   /* OPT3001: automatic range, continuous, 800 ms */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM1); DL_GPIO_initDigitalOutput(IOMUX_PINCM39); DL_GPIO_initDigitalOutput(IOMUX_PINCM54); DL_GPIO_initDigitalOutput(IOMUX_PINCM30);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_0 | DL_GPIO_PIN_17 | DL_GPIO_PIN_24); DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_0 | DL_GPIO_PIN_17 | DL_GPIO_PIN_24); DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_13);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM40, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM14, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_ADC12_enableConversions(ADC12_0_INST);
}
static uint32_t lux100(void) { uint16_t r; if (rd16(0x44, 0x00, &r)) return 0xFFFFFFFFu; return (uint32_t)(1u << (r >> 12)) * (r & 0xFFF); }
static uint16_t hall(void)
{
    DL_ADC12_startConversion(ADC12_0_INST);
    for (uint32_t to = 0; to < 100000 && !DL_ADC12_getRawInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED); to++) { }
    uint16_t v = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC12_0_INST);
    return v;
}
static int s1(void) { return DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_18) != 0; }
static int s2(void) { return DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_7) == 0; }
static void rgb(int r, int g, int b)
{
    if (r) DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_17); else DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_17);
    if (g) DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_24); else DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_24);
    if (b) DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_13); else DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_13);
}
static void red(int on) { if (on) DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_0); else DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_0); }
static int stop_requested(void) { return getline_() && line[0] == 'X'; }
static int send_txt(const char *t) { uint16_t n = 0; while (t[n]) n++; return send(build_text(t, n), 0); }
static void hexline(const char *tag, unsigned i)
{
    puts_(tag); putu(ring[i].len); puts_(","); puti(ring[i].rssi); puts_(",");
    unsigned n = ring[i].len < RING_D ? ring[i].len : RING_D;
    for (unsigned k = 0; k < n; k++) puthex(ring[i].d[k], 2);
    puts_("\r\n");
}

int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_setPins(RADIO_PORT, RADIO_CS_PIN | RADIO_RST_PIN);
    clock_init(); ms(100); sensors_init();
    puts_("\r\nradio_lab: CC1407P on LP-MSPM0G5187, commands P F W T R G E S C V\r\n");
    if (!configure(13, -1)) puts_("open FAILED\r\n");
    puts_("READY\r\n");
    for (;;) {
        if (!getline_()) continue;
        char op = line[0]; arg_p = line + 1;
        puts_("> "); puts_(line); puts_("\r\n");
        if (op == 'P') {
            int32_t i = argi(13), m = argi(-1);
            if (i < 0 || i >= (int32_t)NPHY) { puts_("DONE ERR profile\r\n"); continue; }
            int ok = configure((int)i, m);
            puts_("DONE P,"); puti(i); puts_(","); puts_(phys[i].name); puts_(",mask=0x"); puthex(cur_mask, 4); puts_(ok ? ",ok\r\n" : ",FAILED\r\n");
        } else if (op == 'F') {
            freq_khz = (uint32_t)argi(915000); store_tx(); puts_("DONE F,"); putu(freq_khz); puts_("\r\n");
        } else if (op == 'W') {
            pwr = (int8_t)argi(14); pa = (uint8_t)argi(1); store_tx(); puts_("DONE W,"); puti(pwr); puts_(","); putu(pa); puts_("\r\n");
        } else if (op == 'T') {
            uint32_t n = (uint32_t)argi(10), len = (uint32_t)argi(20), gap = (uint32_t)argi(0), ok = 0, sum = 0, t;
            static uint32_t seq; char txt[32];
            for (uint32_t k = 0; k < n; k++) {
                int w = 0; const char *pre = "FRL P"; while (*pre) txt[w++] = *pre++;
                if (cur >= 10) txt[w++] = (char)('0' + cur / 10); txt[w++] = (char)('0' + cur % 10);
                txt[w++] = ' '; txt[w++] = 'n'; txt[w++] = '=';
                char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = seq++; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                txt[w] = 0;
                if (send(build_text(txt, (uint16_t)len), &t)) { ok++; sum += t; }
                if (gap) ms(gap);
            }
            puts_("DONE T,"); putu(ok); puts_(","); putu(n); puts_(",tx_us_avg="); putu(ok ? sum / ok : 0); puts_("\r\n");
        } else if (op == 'R') {
            uint32_t msr = (uint32_t)argi(5000); rhead = rtail = rdrop = rover = 0;
            uint32_t t0 = frl_now_us(); uint8_t st = rx_once(msr * 1000u, 1);
            while (rtail != rhead) { print_pkt(rtail % RING); rtail++; }
            puts_("DONE R,status=0x"); puthex(st, 2); puts_(",sync="); putu(rxstat.num_sync); puts_(",hdr_ok="); putu(rxstat.num_header_ok);
            puts_(",crc_ok="); putu(rxstat.num_crc_ok); puts_(",printed_dropped="); putu(rdrop); puts_(",overflow="); putu(rover);
            puts_(",ms="); putu((frl_now_us() - t0) / 1000u); puts_("\r\n");
        } else if (op == 'G') {
            uint32_t n = (uint32_t)argi(20), len = (uint32_t)argi(30), tmo = (uint32_t)argi(300), gap = (uint32_t)argi(50);
            chain_rx = 1; store_tx();
            uint32_t got = 0, rtt_sum = 0; int32_t lr_sum = 0, rr_sum = 0, lr_min = 127, lr_max = -128;
            char txt[24];
            for (uint32_t k = 0; k < n; k++) {
                int w = 0; const char *pre = "FRL PING "; while (*pre) txt[w++] = *pre++;
                char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = k; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                txt[w] = 0;
                rhead = rtail = 0;
                store_rx(tmo * 1000u, 0);            /* the radio chains from the transmit in slot 0 to this receive */
                uint32_t t0 = frl_now_us(); rx_done = 0;
                if (!send(build_text(txt, (uint16_t)len), 0)) { puts_("PING,"); putu(k); puts_(",txfail\r\n"); continue; }
                { uint32_t tw = frl_now_us(); while (!rx_done && frl_now_us() - tw < (tmo + 2000u) * 1000u) { } }
                int32_t rr; int hit = 0;
                while (rtail != rhead) { unsigned i = rtail % RING; if (!hit && parse_pong(i, k, &rr)) { hit = 1;
                    uint32_t rtt = ring[i].t_host - t0; got++; rtt_sum += rtt; lr_sum += ring[i].rssi; rr_sum += rr;
                    if (ring[i].rssi < lr_min) lr_min = ring[i].rssi; if (ring[i].rssi > lr_max) lr_max = ring[i].rssi;
                    puts_("PING,"); putu(k); puts_(",ok,rtt_us="); putu(rtt); puts_(",here="); puti(ring[i].rssi); puts_(",there="); puti(rr); puts_("\r\n"); }
                    rtail++; }
                if (!hit) { puts_("PING,"); putu(k); puts_(",lost\r\n"); }
                if (gap) ms(gap);
            }
            chain_rx = 0; store_tx();
            puts_("DONE G,"); putu(got); puts_(","); putu(n); puts_(",rtt_us_avg="); putu(got ? rtt_sum / got : 0);
            puts_(",here_avg="); puti(got ? lr_sum / (int32_t)got : 0); puts_(",here_min="); puti(got ? lr_min : 0); puts_(",here_max="); puti(got ? lr_max : 0);
            puts_(",there_avg="); puti(got ? rr_sum / (int32_t)got : 0); puts_("\r\n");
        } else if (op == 'E') {
            uint32_t msr = (uint32_t)argi(10000), t0 = frl_now_us(), answered = 0, other = 0;
            while (frl_now_us() - t0 < msr * 1000u) {
                rhead = rtail = 0;
                rx_once(200000u, 0);
                while (rtail != rhead) {
                    unsigned i = rtail % RING; const char *s = (const char *)ring[i].d;
                    if (ring[i].len > 9 && !memcmp(s, "FRL PING ", 9)) {
                        arg_p = s + 9; int32_t k = argi(0); char txt[32]; int w = 0; const char *pre = "FRL PONG ";
                        while (*pre) txt[w++] = *pre++;
                        char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = (uint32_t)k; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                        txt[w++] = ' '; txt[w++] = 'r'; txt[w++] = '='; int32_t r = ring[i].rssi; if (r < 0) { txt[w++] = '-'; r = -r; }
                        j = 10; kk = (uint32_t)r; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                        txt[w] = 0;
                        if (send(build_text(txt, ring[i].len), 0)) answered++;
                    } else other++;
                    rtail++;
                }
            }
            puts_("DONE E,answered="); putu(answered); puts_(",other="); putu(other); puts_("\r\n");
        } else if (op == 'S') {
            uint32_t f0 = (uint32_t)argi(902000), f1 = (uint32_t)argi(928000), st = (uint32_t)argi(500), keep = freq_khz;
            for (uint32_t f = f0; f <= f1; f += st) {
                freq_khz = f; store_rx(0, 1); ev = 0; TRX_Host_submitCmd(h, 1); ms(3);
                int32_t mx = -128, sum = 0, n = 0;
                for (int q = 0; q < 8; q++) { got_rssi = 0; TRX_Host_utilGetRssi(h, lab_cb); uint32_t t = frl_now_us();
                    while (!got_rssi && frl_now_us() - t < 50000u) { } if (got_rssi && rssi_now != 127) { if (rssi_now > mx) mx = rssi_now; sum += rssi_now; n++; } ms(1); }
                ev = 0; TRX_Host_stopCmd(h, 1, TRX_Command_StopType_Hard); wait_ev(TRX_EventCmdStatus, 500); ev = 0;
                puts_("SCAN,"); putu(f); puts_(","); puti(n ? sum / n : 127); puts_(","); puti(mx); puts_("\r\n");
            }
            freq_khz = keep; store_tx(); puts_("DONE S\r\n");
        } else if (op == 'C') {
            int32_t thr = argi(-90), tmo = argi(100);
            static TRX_Request_CommandStore cc; memset(&cc, 0, sizeof cc);
            cc.slot = 2; cc.cmd_id = TRX_RadioCommand_CarrierSense; cc.trigger = Command_Trigger_Immediate; cc.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
            cc.params.cs.phy0.config_id = 1u; cc.params.cs.phy0.option_mask = cur_mask; cc.params.cs.frequency = freq_khz; cc.params.cs.modem = phys[cur].modem;
            cc.params.cs.mode = TRX_CarrierSense_Mode_Energy; cc.params.cs.exit_condition = TRX_CarrierSense_ExitCondition_Fast;
            cc.params.cs.rssi_window = 4; cc.params.cs.rssi_count = 2; cc.params.cs.timeout = (uint32_t)tmo * 1000u; cc.params.cs.rssi_override = thr;
            ev = 0; TRX_Host_storeCmds(h, &cc, 1u, NULL, lab_cb, TRX_EventCmdStatus | TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete);
            uint32_t w1 = wait_ev(TRX_EventCmdStoreComplete, 3000); uint8_t e1 = last_err; ev = 0; last_cmd = 0xFF;
            uint32_t t0 = frl_now_us(); TRX_Host_submitCmd(h, 2); uint32_t w2 = wait_ev(TRX_EventCmdStatus, (uint32_t)tmo + 2000u);
            puts_("DONE C,store="); puts_(w1 == 0xFFFFFFFFu ? "fail" : "ok"); puts_(",err="); putu(e1); puts_(",status=0x"); puthex(last_cmd, 2);
            puts_(w2 == 0xFFFFFFFFu ? ",no_status" : ",got_status"); puts_(",us="); putu(frl_now_us() - t0); puts_(",thr="); puti(thr); puts_("\r\n");
        } else if (op == 'D') {
            while (*arg_p == ' ') arg_p++;
            char mode = *arg_p ? *arg_p++ : 'N';
            uint32_t msr = (uint32_t)argi(60000), t0 = frl_now_us();
            if (mode == 'V') {
                /* voice: listen continuously and print every frame as hex for the Mac to decode */
                rhead = rtail = 0; store_rx(msr * 1000u, 1); ev = 0; rx_done = 0; TRX_Host_submitCmd(h, 1);
                uint32_t n = 0;
                while (!rx_done && frl_now_us() - t0 < (msr + 2000u) * 1000u) {
                    while (rtail != rhead) { hexline("A,", rtail % RING); rtail++; n++; }
                    if (stop_requested()) { TRX_Host_stopCmd(h, 1, TRX_Command_StopType_Hard); }
                }
                while (rtail != rhead) { hexline("A,", rtail % RING); rtail++; n++; }
                puts_("DONE D V,frames="); putu(n); puts_(",crc_ok="); putu(rxstat.num_crc_ok); puts_(",sync="); putu(rxstat.num_sync); puts_(",dropped="); putu(rdrop); puts_("\r\n");
            } else {
                /* node: mailbox (light), magnet, doorbell (S1), panic (S1 held 2 s), messages (S2), ring-on-demand and a tag heartbeat */
                static const char *const canned[] = { "FRL MSG I'm home", "FRL MSG Come outside", "FRL MSG Dinner?" };
                uint32_t base = lux100(); uint16_t hb = hall(); int dark = 0, mag = 0, p1 = 0, p2 = 0, msgi = 0, panic_sent = 0;
                uint32_t t1 = 0, tag_t = 0, tag_n = 0, ev_n = 0, rings = 0;
                puts_("NODE,base_lux100="); putu(base); puts_(",hall="); putu(hb); puts_("\r\n");
                while (frl_now_us() - t0 < msr * 1000u) {
                    if (stop_requested()) break;
                    char txt[48]; uint32_t now = frl_now_us();
                    uint32_t lx = lux100(); uint16_t hv = hall();
                    if (lx != 0xFFFFFFFFu) {
                        if (!dark && lx * 5u < base) { dark = 1; send_txt("FRL EV MAIL closed"); ev_n++; puts_("EV,MAIL,closed,"); putu(lx); puts_("\r\n"); }
                        else if (dark && lx * 2u > base) { dark = 0; send_txt("FRL EV MAIL open"); ev_n++; puts_("EV,MAIL,open,"); putu(lx); puts_("\r\n"); }
                    }
                    int dh = (int)hv - (int)hb; if (dh < 0) dh = -dh;
                    if (!mag && dh > 300) { mag = 1; send_txt("FRL EV MAGNET near"); ev_n++; puts_("EV,MAGNET,near,"); putu(hv); puts_("\r\n"); }
                    else if (mag && dh < 150) { mag = 0; send_txt("FRL EV MAGNET away"); ev_n++; puts_("EV,MAGNET,away,"); putu(hv); puts_("\r\n"); }
                    int b1 = s1(), b2 = s2();
                    if (b1 && !p1) { t1 = now; panic_sent = 0; }
                    if (b1 && !panic_sent && now - t1 > 2000000u) { panic_sent = 1; send_txt("FRL EV PANIC"); ev_n++; puts_("EV,PANIC\r\n"); rgb(1, 0, 0); }
                    if (!b1 && p1 && !panic_sent) { send_txt("FRL EV BELL"); ev_n++; puts_("EV,BELL\r\n"); }
                    if (b2 && !p2) { send_txt(canned[msgi]); ev_n++; puts_("EV,MSG,"); puts_(canned[msgi] + 8); puts_("\r\n"); msgi = (msgi + 1) % 3; }
                    p1 = b1; p2 = b2;
                    if (now - tag_t > 1000000u) {
                        tag_t = now; int w = 0; const char *pre = "FRL TAG "; while (*pre) txt[w++] = *pre++;
                        char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = tag_n++; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                        txt[w] = 0; send_txt(txt);
                    }
                    rhead = rtail = 0; rx_once(80000u, 0);
                    while (rtail != rhead) {
                        unsigned i = rtail % RING; const char *d = (const char *)ring[i].d;
                        if (ring[i].len >= 8 && !memcmp(d, "FRL RING", 8)) {
                            rings++; puts_("RING,"); puti(ring[i].rssi); puts_("\r\n");
                            int w = 0; const char *pre = "FRL FOUND r="; while (*pre) txt[w++] = *pre++;
                            int32_t r = ring[i].rssi; if (r < 0) { txt[w++] = '-'; r = -r; }
                            char nb[11]; int j = 10; nb[j] = 0; uint32_t kk = (uint32_t)r; do { nb[--j] = (char)('0' + kk % 10); kk /= 10; } while (kk); while (nb[j]) txt[w++] = nb[j++];
                            txt[w] = 0; send_txt(txt);   /* answer first, then blink, so the base hears it at once */
                            for (int q = 0; q < 6; q++) { red(1); rgb(1, 1, 1); ms(150); red(0); rgb(0, 0, 0); ms(150); }
                        } else if (ring[i].len >= 8 && !memcmp(d, "FRL MSG ", 8)) {
                            puts_("MSG,"); for (unsigned k = 8; k < ring[i].len && k < 60; k++) { char c = d[k]; if (c == ' ' && k + 1 < ring[i].len && d[k + 1] >= 'a' && d[k + 1] <= 'z' && k > 40) break; putc_(c >= 32 && c < 127 ? c : '.'); }
                            puts_("\r\n"); for (int q = 0; q < 3; q++) { rgb(0, 1, 0); ms(120); rgb(0, 0, 0); ms(120); }
                        }
                        rtail++;
                    }
                }
                rgb(0, 0, 0); red(0);
                puts_("DONE D N,events="); putu(ev_n); puts_(",rings="); putu(rings); puts_(",tags="); putu(tag_n); puts_(",i2c_err="); putu(i2c_err); puts_("\r\n");
            }
        } else if (op == 'V') {
            puts_("DONE V,profile="); puti(cur); puts_(",freq="); putu(freq_khz); puts_(",pwr="); puti(pwr); puts_(",pa="); putu(pa); puts_("\r\n");
        } else puts_("DONE ERR unknown\r\n");
    }
}
