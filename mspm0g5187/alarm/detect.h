/*
 * APP_DETECT main loop, included by alarm.c. Listens all the time; once a second runs the CNN on the
 * NPU over the last 8.26 s of feature frames, then decides:
 *   pattern: the model's class (smoke, co) in DECIDE_K of the last DECIDE_M inferences, and the
 *            dominant tone's on/off timing (cadence_ok) fitting that pattern in at least one of them
 *   gate:    the window's loudest tonal frame matches the alarm learned with S2: tone within
 *            max(80 Hz, 4 %) and no more than LEVEL_MARGIN_DB quieter; smoke or CO, the report says which
 * Button S2: short press = learn (hold your alarm's test button near the board) or clear an alarm;
 * hold 5 s = forget. What it learned is kept in data flash at 0x41D00000 (one 16-byte record).
 * UART lines (921600): "I ..." each inference, "ALARM ...", "IGNORED ...", "LEARNED ...", "SELFTEST ...".
 * Build flags: STREAM_AUDIO also sends the audio packets (A5 5B, sample index) for check_features.py.
 */
#include <math.h>
#include "features.h"
#include "fe_tables.h"
#include "tvmgen_default.h"
#include "test_vectors.h"

#define RING 1536
#define INFER_HOP 16
#define DECIDE_M 8
#define DECIDE_K 6
#define LEVEL_MARGIN_DB 10.0f
#define TONE_CREST 1.2f                       /* crest channel (dB/10) a beep must reach */
#define REC_ADDR 0x41D00000u
#define REC_MAGIC 0xA1A40001u

static int32_t ring[RING];
static volatile uint32_t widx;               /* samples written since start */
static int16_t fr[FE_T][FE_NCH];             /* feature frames, dB x64 (ch 10, 11: x4096) */
static float nin[FE_C * FE_T];
static float nout[3];
static const char *const cname[3] = { "other", "smoke", "co" };

#if 1
/* Commands from the Mac over the backchannel UART (every detect build): A5 4C = press S2, A5 46 = hold S2.
 * INJECT builds also take audio from the Mac instead of the microphone: instead of the microphone (silent test of the whole chain).
 * Bytes: A5 5C n, then n int16 little-endian samples at 15,873 a second (16-bit full scale = the 18-bit one);
 * A5 4C = press S2 (learn or clear); A5 46 = hold S2 five seconds (forget). */
static volatile uint8_t cmd_press, cmd_forget, cmd_learn;
void UART_0_INST_IRQHandler(void)
{
    static uint8_t st, n, lo;
    while (!DL_UART_isRXFIFOEmpty(UART_0_INST)) {
        uint8_t b = DL_UART_receiveData(UART_0_INST);
        switch (st) {
        case 0: st = b == 0xA5 ? 1 : 0; break;
        case 1: st = b == 0x5C ? 2 : 0; if (b == 0x4C) cmd_press = 1; if (b == 0x46) cmd_forget = 1; if (b == 0x4E) cmd_learn = 1; break;
        case 2: n = b; st = n ? 3 : 0; break;
        case 3: lo = b; st = 4; break;
#ifdef INJECT
        case 4: ring[widx % RING] = (int32_t)(int16_t)(lo | (b << 8)) * 4; widx++; st = --n ? 3 : 0; break;
#else
        case 4: st = --n ? 3 : 0; (void)lo; break;
#endif
        }
    }
}
#endif

static void ring_push_half(int h)
{
    for (int k = 0; k < HALF; k++) {
        int32_t y;
        if (dec_push(dmabuf[h * HALF + k], &y)) { ring[widx % RING] = y; widx++; }
    }
}

/* ---- time: SysTick counts down 80,000 cycles per ms ---- */
static uint32_t now_cycles(void)
{
    uint32_t a, v;
    do { a = ms_ticks; v = SysTick->VAL; } while (a != ms_ticks);
    return a * 80000u + (79999u - v);
}

static void tx_f(float v, int dp)
{
    int32_t s = 1; for (int i = 0; i < dp; i++) s *= 10;
    int32_t q = (int32_t)(v * (float)s + (v < 0 ? -0.5f : 0.5f));
    if (q < 0) { tx('-'); q = -q; }
    tx_int(q / s);
    if (dp) { tx('.'); int32_t r = q % s; for (int32_t d = s / 10; d; d /= 10) { tx((uint8_t)('0' + r / d)); r %= d; } }
}

/* ---- learned alarm, kept in data flash ---- */
typedef struct { uint32_t magic, cls; float f0, l0; } rec_t;
static rec_t learned;

/* One 64-bit flash word (a single 64-bit write is what works on this data flash):
 * word 0 = 0xA1A4 tag in the top half, class in the bottom; word 1 = tone in Hz (low 16 bits), level x64 (high). */
static void rec_load(void)
{
    const volatile uint32_t *r = (const volatile uint32_t *)REC_ADDR;
    uint32_t w0 = r[0], w1 = r[1];
    learned.magic = 0;
    if ((w0 >> 16) == 0xA1A4u && (w0 & 0xFFFFu) >= 1 && (w0 & 0xFFFFu) <= 2) {
        learned.magic = REC_MAGIC; learned.cls = w0 & 0xFFFFu;
        learned.f0 = (float)(w1 & 0xFFFFu); learned.l0 = (int16_t)(w1 >> 16) / 64.0f;
    }
}

static int rec_save(const rec_t *r)
{
    static uint32_t w[2];
    w[0] = 0xA1A40000u | (r->cls & 0xFFFFu);
    w[1] = ((uint32_t)(uint16_t)(r->f0 + 0.5f)) | ((uint32_t)(uint16_t)(int16_t)(r->l0 * 64.0f) << 16);
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectDataMemory(FLASHCTL);
    if (DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, REC_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR) == DL_FLASHCTL_COMMAND_STATUS_FAILED) return -1;
    if (r->magic == 0) return 0;                               /* forget: erased is enough */
    DL_FlashCTL_unprotectDataMemory(FLASHCTL);
    if (DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, REC_ADDR, w) == DL_FLASHCTL_COMMAND_STATUS_FAILED) return -2;
    const volatile uint32_t *q = (const volatile uint32_t *)REC_ADDR;
    return (q[0] == w[0] && q[1] == w[1]) ? 0 : -3;
}

static void npu_init(void)
{
    DL_NPU_reset(NPU);
    DL_NPU_enablePower(NPU);
    while (!(DL_SYSCTL_getStatus() & DL_SYSCTL_STATUS_NPU_READY)) { }
    DL_NPU_clearInterruptStatus(NPU, DL_NPU_INTERRUPT_DONE);
    DL_NPU_enableInterrupt(NPU, DL_NPU_INTERRUPT_DONE);
    NVIC_EnableIRQ(NPU_INT_IRQn);
}

static int run_model(const float *in, uint32_t *cyc)
{
    struct tvmgen_default_inputs ti = { (void *)in };
    struct tvmgen_default_outputs to = { (void *)nout };
    uint32_t t0 = now_cycles();
#ifdef TVMGEN_DEFAULT_TI_NPU
    tvmgen_default_finished = 0;
    tvmgen_default_run(&ti, &to);
    while (!tvmgen_default_finished) { }
#else
    tvmgen_default_run(&ti, &to);              /* CPU-only NNC build (for the NPU-against-CPU check) */
#endif
    if (cyc) *cyc = now_cycles() - t0;
    int c = 0;
    for (int k = 1; k < 3; k++) if (nout[k] > nout[c]) c = k;
    return c;
}

static void selftest(void)
{
    int ok = 0; uint32_t cyc = 0, mx = 0;
    for (int i = 0; i < TV_N; i++) {
        for (int k = 0; k < FE_C * FE_T; k++) nin[k] = tv_x[i][k] / TV_Q;
        int c = run_model(nin, &cyc);
        ok += (c == tv_class[i]);
        if (cyc > mx) mx = cyc;
    }
    static float o[FE_NCH];
    for (int i = 0; i < RING; i++) ring[i] = (int32_t)((i * 2654435761u) >> 20) - 2048;   /* any audio */
    uint32_t t0 = now_cycles();
    fe_frame(ring, RING, 0, o);
    uint32_t fe = now_cycles() - t0;
    for (int i = 0; i < RING; i++) ring[i] = 0;
    tx_str("SELFTEST npu_class_match="); tx_int(ok); tx('/'); tx_int(TV_N);
    tx_str(" npu_us="); tx_int((int32_t)(mx / 80)); tx_str(" frame_us="); tx_int((int32_t)(fe / 80)); tx_str("\r\n");
}

#ifdef STREAM_AUDIO
static uint32_t sent;
static void stream_audio(void)
{
    for (int pk = 0; pk < 4 && widx - sent >= 16; pk++) {   /* at most 4 packets per pass, so frames still get their turn */
        uint8_t cs = 0;
        tx(0xA5); tx(0x5B);
        for (int i = 0; i < 4; i++) tx((uint8_t)(sent >> (8 * i)));
        tx(16);
        for (int k = 0; k < 16; k++) {
            int32_t y = ring[(sent + (uint32_t)k) % RING];
            uint8_t b0 = (uint8_t)y, b1 = (uint8_t)(y >> 8), b2 = (uint8_t)(y >> 16);
            tx(b0); tx(b1); tx(b2); cs = (uint8_t)(cs + b0 + b1 + b2);
        }
        tx(cs);
        sent += 16;
    }
    if (widx - sent > RING) sent = widx - 16;               /* fell a whole ring behind: skip ahead (the host sees the index jump) */
}
#endif

/* window stats for the gate: loudest tonal frame */
static void window_tone(uint32_t nfr, float *f_obs, float *l_obs)
{
    *l_obs = -130.0f; *f_obs = 0;
    for (int t = 0; t < FE_T; t++) {
        const int16_t *q = fr[(nfr - FE_T + (uint32_t)t) % FE_T];
        if (q[11] / 4096.0f >= TONE_CREST && q[12] / 64.0f > *l_obs) { *l_obs = q[12] / 64.0f; *f_obs = q[10] / 4096.0f * 4000.0f; }
    }
}

/* Same as host/features.py normalize(): 10 level channels relative to the loudest frame, peak frequency
 * and crest, then four channels locked onto the window's dominant tone (loudest tonal frame). */
static void build_input(uint32_t nfr)
{
    float ref = FE_REF_FLOOR_DB;
    for (int t = 0; t < FE_T; t++) { float v = fr[t][9] / 64.0f; if (v > ref) ref = v; }
    int ib = -1; int16_t best = 0;
    for (int t = 0; t < FE_T; t++) {
        const int16_t *q = fr[(nfr - FE_T + (uint32_t)t) % FE_T];
        if (q[11] / 4096.0f >= TONE_CREST && (ib < 0 || q[12] > best)) { best = q[12]; ib = t; }
    }
    float fdom = ib >= 0 ? fr[(nfr - FE_T + (uint32_t)ib) % FE_T][10] / 4096.0f : 0.0f;
    for (int t = 0; t < FE_T; t++) {
        const int16_t *q = fr[(nfr - FE_T + (uint32_t)t) % FE_T];
        for (int c = 0; c < 10; c++) {
            float v = (q[c] / 64.0f - ref) / 10.0f;
            nin[c * FE_T + t] = v < -6 ? -6 : (v > 0 ? 0 : v);
        }
        nin[10 * FE_T + t] = q[10] / 4096.0f;
        nin[11 * FE_T + t] = q[11] / 4096.0f;
        float lev = (q[12] / 64.0f - ref) / 10.0f;
        lev = lev < -6 ? -6 : (lev > 0 ? 0 : lev);
        int intone = ib >= 0 && q[11] / 4096.0f >= TONE_CREST && fabsf(q[10] / 4096.0f - fdom) * 4000.0f <= 3.0f * FE_DF + 1e-3f;
        nin[12 * FE_T + t] = intone ? lev : -6.0f;
        nin[13 * FE_T + t] = lev;
        nin[14 * FE_T + t] = intone ? 1.0f : 0.0f;
        nin[15 * FE_T + t] = fdom;
    }
}

/* Second stage, same as host/features.py cadence_ok(): the dominant tone's on/off timing in frames. */
#define T3_BEEP_LO 6
#define T3_BEEP_HI 11
#define T3_SHORT_LO 4
#define T3_SHORT_HI 10
#define T3_LONG_LO 18
#define T3_LONG_HI 27
#define T4_GROUP_LO 9
#define T4_GROUP_HI 16
#define T4_MERGE 3
#define T4_EDGE_MIN 4
#define T4_PERIOD_LO 76
#define T4_PERIOD_HI 103
#define ON_DB 1.2f

static int cadence_ok(const float *z, int cls)
{
    static uint8_t on[FE_T];
    static uint8_t rs[FE_T], rn[FE_T];
    const float *lv = z + 12 * FE_T, *it = z + 14 * FE_T;
    float mx = -1e9f; int any = 0;
    for (int t = 0; t < FE_T; t++) if (it[t] > 0.5f) { any = 1; if (lv[t] > mx) mx = lv[t]; }
    float th = mx - ON_DB;
    for (int t = 0; t < FE_T; t++) on[t] = any && it[t] > 0.5f && lv[t] >= th;
    for (int t = 1; t < FE_T - 1; t++) if (!on[t] && on[t - 1] && on[t + 1]) on[t] = 1;
    int nr = 0;
    for (int t = 0; t < FE_T;) {
        if (on[t]) { int u = t; while (u < FE_T && on[u]) u++; rs[nr] = (uint8_t)t; rn[nr] = (uint8_t)(u - t); nr++; t = u; }
        else t++;
    }
#define IN(v, a, b) ((v) >= (a) && (v) <= (b))
    if (cls == 1) {
        static int fs[FE_T], fn[FE_T]; int nf = 0;
        for (int i = 0; i < nr; i++) if (rs[i] > 0 && rs[i] + rn[i] < FE_T) { fs[nf] = rs[i]; fn[nf] = rn[i]; nf++; }
        for (int i = 0; i + 3 < nf; i++) {
            int ok = 1;
            for (int j = 0; j < 4; j++) if (!IN(fn[i + j], T3_BEEP_LO, T3_BEEP_HI)) ok = 0;
            if (!ok) continue;
            int g[3], sh[3], lg[3];
            for (int j = 0; j < 3; j++) {
                g[j] = fs[i + j + 1] - (fs[i + j] + fn[i + j]);
                sh[j] = IN(g[j], T3_SHORT_LO, T3_SHORT_HI); lg[j] = IN(g[j], T3_LONG_LO, T3_LONG_HI);
            }
            if ((sh[0] && sh[1] && lg[2]) || (lg[0] && sh[1] && sh[2])) return 1;
        }
        return 0;
    }
    /* CO: merge the pieces of each pulse group, then two groups 5.8 s (+-15 %) apart with only quiet between */
    static int gs[FE_T], gn[FE_T]; int ng = 0;
    for (int i = 0; i < nr; i++) {
        if (ng && rs[i] - (gs[ng - 1] + gn[ng - 1]) <= T4_MERGE) gn[ng - 1] = rs[i] + rn[i] - gs[ng - 1];
        else { gs[ng] = rs[i]; gn[ng] = rn[i]; ng++; }
    }
    for (int i = 0; i + 1 < ng; i++) {
        int s0 = gs[i], n0 = gn[i], s1 = gs[i + 1], n1 = gn[i + 1];
        if (s1 + n1 >= FE_T || !IN(n1, T4_GROUP_LO, T4_GROUP_HI)) continue;
        if (s0 == 0) { if (n0 >= T4_EDGE_MIN && IN((s1 + n1) - n0, T4_PERIOD_LO, T4_PERIOD_HI)) return 1; }
        else if (IN(n0, T4_GROUP_LO, T4_GROUP_HI) && IN(s1 - s0, T4_PERIOD_LO, T4_PERIOD_HI)) return 1;
    }
    return 0;
#undef IN
}

int main(void)
{
    SYSCFG_DL_init();
    leds(0, 0, 0);
    tx_str("\r\n#ALARM-DETECT fs=15873 window=128x64.5ms infer_every=16 frames decide=6of8\r\n");
    npu_init();
    fe_init();
    selftest();
    rec_load();
    if (learned.magic) { tx_str("LEARNED-AT-BOOT cls="); tx_str(cname[learned.cls]); tx_str(" f0="); tx_int((int32_t)learned.f0); tx_str(" L0="); tx_f(learned.l0, 1); tx_str("\r\n"); }
    else tx_str("NOT-LEARNED: generic mode, any smoke or CO pattern alarms. Press S2 to learn yours.\r\n");
    DL_UART_enableInterrupt(UART_0_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
#ifdef INJECT
    tx_str("INJECT: audio comes from the Mac over this serial port, not the microphone\r\n");
#endif
#ifdef NO_AUDIO
    /* time the per-inference work on a test vector: input build is skipped (frames are synthetic) */
    for (int k = 0; k < FE_C * FE_T; k++) nin[k] = tv_x[0][k] / TV_Q;
    uint32_t t0 = now_cycles(); int ok1 = cadence_ok(nin, 1), ok2 = cadence_ok(nin, 2); uint32_t tc = now_cycles() - t0;
    t0 = now_cycles(); build_input(FE_T); uint32_t tb = now_cycles() - t0;
    tx_str("TIMING cadence_us="); tx_int((int32_t)(tc / 80)); tx_str(" build_input_us="); tx_int((int32_t)(tb / 80));
    tx_str(" cad="); tx_int(ok1); tx_int(ok2);
    {   /* decimation cost: one DMA half (48 words -> 16 samples) as the microphone interrupt runs it */
        static uint32_t fake[HALF]; for (int k = 0; k < HALF; k++) fake[k] = (uint32_t)(k * 2654435761u);
        uint32_t t1 = now_cycles();
        for (int r = 0; r < 100; r++) for (int k = 0; k < HALF; k++) { int32_t y; if (dec_push(fake[k], &y)) { ring[widx % RING] = y; widx++; } }
        uint32_t tdec = (now_cycles() - t1) / 100;
        tx_str(" dec_half_cycles="); tx_int((int32_t)tdec); tx_str(" dec_cpu_permille="); tx_int((int32_t)(tdec * 1000u / 80000u));
    }
    tx_str("\r\n");
    for (;;) { leds(0, (ms_ticks % 1000) < 100, 0); }
#endif
#ifndef INJECT
    audio_start();
#endif
#ifdef STREAM_AUDIO
    sent = 0;
#endif
    uint32_t ridx = 0, nfr = 0, ninf = 0, ring_over = 0;
    uint8_t hist[DECIDE_M] = { 0 }, okh[DECIDE_M] = { 0 };
    int alarm = 0, learning = 0, nl = 0;
    float lf[16], ll[16]; int lc[3] = { 0 };
    uint32_t learn_until = 0, last_ignored = 0, last_alarm_print = 0, press_t = 0;
    int pressed = 0;
    float o[FE_NCH];
    for (;;) {
#ifdef STREAM_AUDIO
        stream_audio();
#endif
        /* button S2 (pressed = low) */
        int down = !DL_GPIO_readPins(GPIO_BTN_PORT, GPIO_BTN_S2_PIN);
        if (cmd_learn) {
            cmd_learn = 0; alarm = 0;
            learning = 1; nl = 0; lc[1] = lc[2] = 0; learn_until = ms_ticks + 45000;
            for (int i = 0; i < DECIDE_M; i++) hist[i] = okh[i] = 0;
            tx_str("LEARN: sound your alarm now (hold its test button) near the board\r\n");
        }
        if (cmd_press || cmd_forget) {                  /* a press from the Mac: 100 ms, or 6 s for forget */
            if (!pressed) { pressed = 1; press_t = ms_ticks - (cmd_forget ? 6000 : 100); }
            down = 0; cmd_press = cmd_forget = 0;
        }
        if (down && !pressed) { pressed = 1; press_t = ms_ticks; }
        if (!down && pressed) {
            pressed = 0;
            uint32_t d = ms_ticks - press_t;
            if (d >= 5000) {
                learned.magic = 0; rec_save(&learned);
                tx_str("FORGOT: generic mode\r\n");
            } else if (d >= 30) {
                if (alarm) {                    /* forget the recent answers too, so the sound still in the window does not re-alarm */
                    alarm = 0; tx_str("CLEARED\r\n");
                    for (int i = 0; i < DECIDE_M; i++) hist[i] = okh[i] = 0;
                }
                else {
                    learning = 1; nl = 0; lc[1] = lc[2] = 0; learn_until = ms_ticks + 45000;
                    tx_str("LEARN: sound your alarm now (hold its test button) near the board\r\n");
                }
            }
        }
        if (widx - ridx > RING - 32) { ring_over++; ridx = widx - FE_FRAME; }
        if (widx - ridx < FE_FRAME) {
            uint32_t m = ms_ticks;
            if (learning) leds(0, 0, (m / 125) & 1);
            else if (alarm) leds(1, 0, 0);
            else leds(0, (m % 2000) < 40, 0);
            continue;
        }
        fe_frame(ring, RING, ridx % RING, o);
        ridx += FE_FRAME;
        int16_t *q = fr[nfr % FE_T];
        for (int c = 0; c < 10; c++) q[c] = (int16_t)(o[c] * 64.0f);
        q[10] = (int16_t)(o[10] * 4096.0f); q[11] = (int16_t)(o[11] * 4096.0f); q[12] = (int16_t)(o[12] * 64.0f);
        nfr++;
#ifdef DUMP_FEATURES
        tx_str("F "); tx_int((int32_t)(ridx - FE_FRAME));
        for (int c = 0; c < FE_NCH; c++) { tx(' '); tx_f(o[c], 2); }
        tx_str("\r\n");
#endif
        if (nfr < FE_T || nfr % INFER_HOP) continue;

        build_input(nfr);
        uint32_t cyc;
        int cls = run_model(nin, &cyc);
        float f_obs, l_obs;
        window_tone(nfr, &f_obs, &l_obs);
        int cad = cls ? cadence_ok(nin, cls) : 0;
        for (int i = 0; i < DECIDE_M - 1; i++) { hist[i] = hist[i + 1]; okh[i] = okh[i + 1]; }
        hist[DECIDE_M - 1] = (uint8_t)cls;
        okh[DECIDE_M - 1] = (uint8_t)cad;
        ninf++;
        tx_str("I n="); tx_int((int32_t)ninf); tx_str(" cls="); tx_str(cname[cls]);
        tx_str(" logits="); tx_f(nout[0], 2); tx(','); tx_f(nout[1], 2); tx(','); tx_f(nout[2], 2);
        tx_str(" cad="); tx_int(cad); tx_str(" tone_hz="); tx_int((int32_t)f_obs); tx_str(" tone_db="); tx_f(l_obs, 1);
        tx_str(" npu_us="); tx_int((int32_t)(cyc / 80)); tx_str(" over="); tx_int((int32_t)(ring_over + overruns)); tx_str("\r\n");

        int pat = 0;
        for (int c = 1; c <= 2; c++) {
            int k = 0, ok = 0;
            for (int i = 0; i < DECIDE_M; i++) { k += hist[i] == c; ok |= hist[i] == c && okh[i]; }
            if (k >= DECIDE_K && ok) pat = c;          /* the model agrees and the timing check passed at least once */
        }

        if (learning) {
            if (cls >= 1 && cad && f_obs > 0 && nl < 16) { lf[nl] = f_obs; ll[nl] = l_obs; lc[cls]++; nl++; }
            if (nl >= 3) {
                /* medians by insertion sort */
                for (int i = 1; i < nl; i++) for (int j = i; j > 0 && lf[j] < lf[j - 1]; j--) { float t = lf[j]; lf[j] = lf[j - 1]; lf[j - 1] = t; }
                for (int i = 1; i < nl; i++) for (int j = i; j > 0 && ll[j] < ll[j - 1]; j--) { float t = ll[j]; ll[j] = ll[j - 1]; ll[j - 1] = t; }
                learned.magic = REC_MAGIC; learned.cls = lc[2] > lc[1] ? 2 : 1;
                learned.f0 = lf[nl / 2]; learned.l0 = ll[nl / 2];
                int e = rec_save(&learned);
                tx_str("LEARNED cls="); tx_str(cname[learned.cls]); tx_str(" f0="); tx_int((int32_t)learned.f0);
                tx_str(" L0="); tx_f(learned.l0, 1); tx_str(e ? " (not saved to flash, err " : " saved"); if (e) { tx_int(e); tx(')'); } tx_str("\r\n");
                learning = 0;
                for (int i = 0; i < DECIDE_M; i++) hist[i] = okh[i] = 0;
            } else if ((int32_t)(ms_ticks - learn_until) > 0) {
                learning = 0;
                tx_str("LEARN FAILED: no smoke or CO pattern heard in 45 s\r\n");
            }
            continue;
        }
        if (!pat || cls != pat) continue;              /* the current window must still hear it (its tone feeds the gate) */
        const char *why = 0;
        if (learned.magic) {
            float tol = learned.f0 * 0.04f; if (tol < 80) tol = 80;
            /* the learned record is the horn beside the board (tone, loudness): a combination alarm uses the same
             * horn for smoke (temporal-3) and CO (temporal-4), so either pattern counts and the report says which */
            if (fabsf(f_obs - learned.f0) > tol) why = "tone differs from the learned alarm";
            else if (l_obs < learned.l0 - LEVEL_MARGIN_DB) why = "too quiet to be the alarm beside me";
        }
        if (why) {
            if (ms_ticks - last_ignored > 10000) {
                last_ignored = ms_ticks;
                tx_str("IGNORED "); tx_str(cname[pat]); tx_str(" pattern: "); tx_str(why);
                tx_str(" (tone "); tx_int((int32_t)f_obs); tx_str(" Hz, "); tx_f(l_obs, 1); tx_str(" dB)\r\n");
            }
            continue;
        }
        if (!alarm || ms_ticks - last_alarm_print > 10000) {
            alarm = 1; last_alarm_print = ms_ticks;
            tx_str("ALARM "); tx_str(cname[pat]); tx_str(learned.magic ? " from the learned alarm" : " (generic, nothing learned)");
            tx_str(" tone="); tx_int((int32_t)f_obs); tx_str(" Hz level="); tx_f(l_obs, 1); tx_str(" dB\r\n");
        }
    }
}
