/* Host test for the audio algorithms (tuner, spectrum, DTMF, gate, echo) against double-precision references.
 *   host/run_algos.sh builds it with the algos and core sources and runs it. Exit 0 when every check passes.
 * Also writes WAV files of the gate and echo output after the DAC8311 frame model (out/algos_*.wav). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include "ba_yin.h"
#include "ba_fft.h"
#include "ba_dtmf.h"
#include "ba_gate.h"
#include "ba_dac8311.h"
#include "ba_amath.h"

#define RATE 8000
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const char *capfile;

static uint32_t rs = 12345;
static double urand(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs & 0xFFFFFF) / 16777216.0; }
static double grand(void) { double u = urand() + 1e-12, v = urand(); return sqrt(-2 * log(u)) * cos(2 * M_PI * v); }
static double now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
static int16_t cl(double v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : lrint(v)); }

/* ---- the DAC8311 model, to play the gate and echo output through the real frame code ---- */
static struct { int sync, sclk, din, nclk; uint32_t sr; uint16_t out; } dac = { 1 };
static void m_sync(int l) { if (dac.sync && !l) { dac.nclk = 0; dac.sr = 0; } if (!dac.sync && l && dac.nclk == 16) dac.out = (uint16_t)(dac.sr & 0x3FFF); dac.sync = l; }
static void m_sclk(int l) { if (dac.sclk && !l && !dac.sync) { dac.sr = (dac.sr << 1) | (uint32_t)dac.din; dac.nclk++; } dac.sclk = l; }
static void m_din(int l) { dac.din = l; }
static const ba_hal_t hal = { m_sync, m_sclk, m_din };
static int16_t through_dac(int16_t s) { ba_dac_write16(&hal, ba_dac_word(ba_dac_code_from_s16(s, 16), 0)); return (int16_t)(((int)dac.out - 8192) * 4); }

static void wav(const char *path, const int16_t *x, unsigned n)
{
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(2); }
    uint32_t dlen = n * 2, rate = RATE, br = RATE * 2, sixteen = 16, riff = 36 + dlen; uint16_t one = 1, bits = 16, ba = 2;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f);
    fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dlen, 4, 1, f); fwrite(x, 2, n, f); fclose(f);
}

/* ---- recorded captures from the pack microphone (the "S" lines of the console logs) ---- */
static int load_caps(const char *path, uint16_t caps[][2048], int maxc)
{
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[4096]; int nc = -1; unsigned n = 0;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "CAP ", 4)) { nc++; n = 0; if (nc >= maxc) break; }
        else if (line[0] == 'S' && line[1] == ' ' && nc >= 0) {
            for (char *p = line + 2; isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3]) && n < 2048; p += 4) {
                char h[5] = { p[0], p[1], p[2], p[3], 0 }; caps[nc][n++] = (uint16_t)strtoul(h, 0, 16);
            }
        }
    }
    fclose(f); return nc + 1;
}
static void mic_to_s16(const uint16_t *c, int16_t *x, int n)
{
    double m = 0; for (int i = 0; i < n; i++) m += c[i]; m /= n;
    for (int i = 0; i < n; i++) x[i] = cl((c[i] - m) * 8.0); /* 12-bit code to the 16-bit range */
}

/* ---- YIN reference in double ---- */
static double yin_ref(const double *x, double thr)
{
    double d[BA_YIN_TAUMAX], dn[BA_YIN_TAUMAX], cum = 0;
    for (int t = 1; t < BA_YIN_TAUMAX; t++) { double s = 0; for (int j = 0; j < BA_YIN_W; j++) { double e = x[j] - x[j + t]; s += e * e; } d[t] = s; }
    dn[0] = 1; for (int t = 1; t < BA_YIN_TAUMAX; t++) { cum += d[t]; dn[t] = cum > 0 ? d[t] * t / cum : 1; }
    int best = 0, tmin = RATE / 1200;
    for (int t = tmin; t < BA_YIN_TAUMAX - 1; t++) if (dn[t] < thr) { while (t + 1 < BA_YIN_TAUMAX - 1 && dn[t + 1] < dn[t]) t++; best = t; break; }
    if (!best) return 0;
    double a = d[best - 1], b = d[best], c = d[best + 1], den = 2 * (a - 2 * b + c), dl = den > 0 ? (a - c) / den : 0;
    return RATE / (best + dl);
}
static double cents_of(double f, double ref) { return 1200.0 * log2(f / ref); }

static void test_tuner(void)
{
    printf("\n== 1. Tuner (YIN) ==\n");
    int16_t x[BA_YIN_FRAME]; double xd[BA_YIN_FRAME];
    struct { const char *name; double lo, hi; } bands[] = { {"82-200 Hz", 82, 200}, {"200-500 Hz", 200, 500}, {"500-1000 Hz", 500, 1001} };
    const char *kinds[] = { "clean sine", "guitar-like (3 harmonics)", "sine + noise (20 dB SNR)" };
    double worst_ref = 0; int octave_err = 0, unvoiced = 0, total = 0;
    for (int k = 0; k < 3; k++) {
        for (int b = 0; b < 3; b++) {
            double mx = 0, sum = 0; int cnt = 0;
            for (double f = bands[b].lo; f < bands[b].hi; f *= 1.0 + 1.0 / 60.0) {
                double ph = urand() * 2 * M_PI, amp = 800;
                for (int i = 0; i < BA_YIN_FRAME; i++) {
                    double w = 2 * M_PI * f * i / RATE, v = amp * sin(w + ph);
                    if (k == 1) v = amp * (0.6 * sin(w + ph) + 0.35 * sin(2 * w + 1.3 * ph) + 0.2 * sin(3 * w + 0.4 * ph));
                    if (k == 2) v += grand() * amp / sqrt(2) / 10.0 * 1.0; /* noise RMS = signal RMS / 10 */
                    xd[i] = v; x[i] = cl(lrint(v));
                }
                ba_yin_result_t r; ba_yin(x, RATE, 4915, &r);
                total++;
                if (!r.voiced) { unvoiced++; continue; }
                double fi = r.freq_x256 / 256.0, ce = cents_of(fi, f);
                if (fabs(ce) > 600) octave_err++;
                double fr = yin_ref(xd, 0.15);
                if (fr > 0) { double dr = fabs(cents_of(fi, fr)); if (dr > worst_ref) worst_ref = dr; }
                if (fabs(ce) > mx) mx = fabs(ce); sum += fabs(ce); cnt++;
            }
            printf("  %-26s %-12s max %5.2f cents, mean %5.2f cents (%d freqs)\n", kinds[k], bands[b].name, mx, sum / cnt, cnt);
            if (k == 0 && b < 2) CHECK(mx < 3.0, "clean sine %s within 3 cents (max %.2f)", bands[b].name, mx);
            if (k == 0 && b == 2) CHECK(mx < 15.0, "clean sine %s within 15 cents (max %.2f)", bands[b].name, mx);
            if (k > 0 && b < 2) CHECK(mx < 10.0, "%s %s within 10 cents (max %.2f)", kinds[k], bands[b].name, mx);
        }
    }
    CHECK(octave_err == 0 && unvoiced == 0, "no octave errors and no unvoiced frames in %d tests", total);
    CHECK(worst_ref < 1.0, "fixed point within 1 cent of the double reference (worst %.3f cents)", worst_ref);
    /* open guitar strings */
    double strings[] = { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63 }; const char *names[] = { "E2", "A2", "D3", "G3", "B3", "E4" };
    for (int s = 0; s < 6; s++) {
        for (int i = 0; i < BA_YIN_FRAME; i++) x[i] = cl(800 * sin(2 * M_PI * strings[s] * 1.01 * i / RATE)); /* 1 percent sharp = 17.2 cents */
        ba_yin_result_t r; ba_yin(x, RATE, 4915, &r); int m, c10; ba_pitch_note(r.freq_x256, &m, &c10);
        printf("  string %s sharp by 17.2 cents: note %d, %+.1f cents\n", names[s], m, c10 / 10.0);
        CHECK(fabs(c10 / 10.0 - 17.2) < 3.0, "string %s reads 17.2 cents sharp within 3 (%+.1f)", names[s], c10 / 10.0);
    }
    /* recorded */
    static uint16_t caps[12][2048]; int nc = load_caps(capfile, caps, 12);
    if (nc >= 2) {
        int16_t rx[2048]; mic_to_s16(caps[1], rx, 2048);
        double mx = 0; int vc = 0, n = 0;
        for (int off = 0; off + BA_YIN_FRAME <= 2048; off += 128) { ba_yin_result_t r; ba_yin(rx + off, RATE, 4915, &r); n++; if (r.voiced) { vc++; double ce = fabs(cents_of(r.freq_x256 / 256.0, 1000.0)); if (ce > mx) mx = ce; } }
        printf("  recorded 1 kHz capture (run3 #1, clipped at the ADC rails): %d of %d frames voiced, worst %.2f cents from 1,000 Hz\n", vc, n, mx);
        CHECK(vc == n && mx < 15.0, "recorded 1 kHz tone reads 1,000 Hz within 15 cents in every frame (worst %.2f)", mx);
    } else printf("  (no recorded capture at %s, skipped)\n", capfile);
    /* latency and cost */
    for (int i = 0; i < BA_YIN_FRAME; i++) x[i] = cl(800 * sin(2 * M_PI * 220.0 * i / RATE));
    ba_yin_result_t r; double t0 = now_ns(); for (int i = 0; i < 2000; i++) ba_yin(x, RATE, 4915, &r); double ns = (now_ns() - t0) / 2000;
    printf("  frame %d samples = %.1f ms; compute %.1f us per frame on this host; %d multiply-accumulates\n", BA_YIN_FRAME, BA_YIN_FRAME * 1000.0 / RATE, ns / 1000, (BA_YIN_TAUMAX - 1) * BA_YIN_W);
}

/* ---- spectrum ---- */
static void ref_bars(const int16_t *x, double *bar)
{
    double mag[64];
    for (int k = 0; k < 64; k++) {
        double re = 0, im = 0;
        for (int n = 0; n < 128; n++) { double w = 0.5 - 0.5 * cos(2 * M_PI * n / 128.0), v = x[n] * w; re += v * cos(2 * M_PI * k * n / 128.0); im -= v * sin(2 * M_PI * k * n / 128.0); }
        mag[k] = 2 * sqrt(re * re + im * im) / 128.0;
    }
    for (int b = 0; b < BA_BARS; b++) {
        double pk = 0; for (int k = ba_bar_edge[b]; k < ba_bar_edge[b + 1]; k++) if (mag[k] > pk) pk = mag[k];
        double h = pk >= 1 ? 8 * log2(pk) : 0; bar[b] = h < 0 ? 0 : h > 127 ? 127 : h;
    }
}
static void test_spectrum(void)
{
    printf("\n== 2. Spectrum analyzer (128-point FFT, 16 bars) ==\n");
    int16_t x[128]; uint32_t mag[64]; uint8_t bar[BA_BARS]; double rb[BA_BARS];
    double worst = 0, wl[5] = {0}; int peak_ok = 0, pk_tests = 0; const int lv[5] = {10, 20, 30, 40, 50};
    for (int trial = 0; trial < 60; trial++) {
        double f1 = 100 + urand() * 3800, f2 = 100 + urand() * 3800, a1 = 200 + urand() * 8000, a2 = urand() * 3000;
        for (int i = 0; i < 128; i++) x[i] = cl(a1 * sin(2 * M_PI * f1 * i / RATE) + a2 * sin(2 * M_PI * f2 * i / RATE) + grand() * 20);
        ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar); ref_bars(x, rb);
        for (int b = 0; b < BA_BARS; b++) for (int q = 0; q < 5; q++) if (rb[b] > lv[q]) { double e0 = fabs(bar[b] - rb[b]); if (e0 > wl[q]) wl[q] = e0; }
        for (int b = 0; b < BA_BARS; b++) if (rb[b] > 30) { double e = fabs(bar[b] - rb[b]); if (e > worst) { worst = e; if (getenv("DBG")) { printf("   worst so far: f1 %.0f a1 %.0f f2 %.0f a2 %.0f bar %d fixed %d ref %.2f\n", f1, a1, f2, a2, b, bar[b], rb[b]); } } }
        /* the strongest tone must land in the bar that holds its bin */
        int kb = (int)lrint((a1 >= a2 ? f1 : f2) / 62.5), bi = -1; for (int b = 0; b < BA_BARS; b++) if (kb >= ba_bar_edge[b] && kb < ba_bar_edge[b + 1]) bi = b;
        int bmax = 0; for (int b = 1; b < BA_BARS; b++) if (bar[b] > bar[bmax]) bmax = b;
        if (bi >= 0 && fabs(a1 - a2) > 0.25 * (a1 > a2 ? a1 : a2)) { pk_tests++; if (abs(bmax - bi) <= 1) peak_ok++; else if (getenv("DBG")) printf("   peak miss: f1 %.0f a1 %.0f f2 %.0f a2 %.0f want bar %d got %d\n", f1, a1, f2, a2, bi, bmax); }
    }
    printf("  worst bar error vs double DFT: %.2f px (bars above 30 px); peak in the right bar (+/-1): %d of %d\n", worst, peak_ok, pk_tests);
    printf("  worst bar error by reference level: >10 px %.2f, >20 px %.2f, >30 px %.2f, >40 px %.2f, >50 px %.2f\n", wl[0], wl[1], wl[2], wl[3], wl[4]);
    CHECK(worst <= 2.0, "bars within 2 px of the double-precision DFT (worst %.2f)", worst);
    CHECK(peak_ok == pk_tests, "strongest tone lands in its bar (%d of %d)", peak_ok, pk_tests);
    /* silence */
    memset(x, 0, sizeof x); ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar); int z = 0; for (int b = 0; b < BA_BARS; b++) z += bar[b];
    CHECK(z == 0, "silence gives all bars at 0");
    /* full scale sine: no overflow, bar near top */
    for (int i = 0; i < 128; i++) x[i] = cl(32000 * sin(2 * M_PI * 1000.0 * i / RATE)); ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar);
    int mx = 0; for (int b = 0; b < BA_BARS; b++) if (bar[b] > mx) mx = bar[b];
    CHECK(mx >= 110 && mx <= 127, "near full-scale 1 kHz sine fills the top bar (%d px) without overflow", mx);
    /* recorded */
    static uint16_t caps[12][2048]; int nc = load_caps(capfile, caps, 12);
    if (nc >= 2) {
        int16_t rx[2048]; mic_to_s16(caps[1], rx, 2048); ba_spectrum_mag(rx + 512, mag); ba_spectrum_bars(mag, bar);
        int bmax = 0; for (int b = 1; b < BA_BARS; b++) if (bar[b] > bar[bmax]) bmax = b;
        printf("  recorded 1 kHz capture: bars = "); for (int b = 0; b < BA_BARS; b++) printf("%d ", bar[b]); printf("\n");
        CHECK(ba_bar_edge[bmax] <= 16 && ba_bar_edge[bmax + 1] > 16, "recorded 1 kHz tone peaks in bar %d (bins %d to %d, 1 kHz is bin 16)", bmax, ba_bar_edge[bmax], ba_bar_edge[bmax + 1] - 1);
    }
    double t0 = now_ns(); for (int i = 0; i < 20000; i++) { ba_spectrum_mag(x, mag); ba_spectrum_bars(mag, bar); } double ns = (now_ns() - t0) / 20000;
    printf("  window + FFT + bars: %.2f us per 128-sample frame on this host (16 ms of audio)\n", ns / 1000);
}

/* ---- DTMF ---- */
static const char digits[] = "1234567890*#ABCD";
static const double rowf[4] = { 697, 770, 852, 941 }, colf[4] = { 1209, 1336, 1477, 1633 };
static void dtmf_freqs(char d, double *r, double *c) { for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { static const char k[4][4] = { {'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'} }; if (k[i][j] == d) { *r = rowf[i]; *c = colf[j]; } } }
/* returns the number of wrong sequences out of trials; sequence = 16 digits, 100 ms tone, 100 ms gap */
static int g_share = 8;
static int dtmf_run(double snr_db, int trials, int *false_digits)
{
    int bad = 0, nsamp = 16 * 1600; int16_t *x = malloc(nsamp * 2);
    double amp = 1500; /* per tone peak; signal power = 2 * amp^2 / 2 = amp^2 */
    double sigma = amp / pow(10, snr_db / 20.0);
    for (int t = 0; t < trials; t++) {
        char want[17], got[64]; int ng = 0;
        for (int i = 0; i < 16; i++) want[i] = digits[(i * 5 + t) % 16 ]; want[16] = 0;
        for (int i = 0; i < 16; i++) {
            double r = 0, c = 0; dtmf_freqs(want[i], &r, &c); double ph1 = urand() * 6.28, ph2 = urand() * 6.28;
            for (int n = 0; n < 1600; n++) {
                double v = n < 800 ? amp * (sin(2 * M_PI * r * n / RATE + ph1) + sin(2 * M_PI * c * n / RATE + ph2)) / 1.0 * 0.5 * 1.0 : 0;
                v += grand() * sigma * 0.5 * 1.0;
                x[i * 1600 + n] = cl(v);
            }
        }
        ba_dtmf_t d; ba_dtmf_init(&d); d.share16 = (uint8_t)g_share;
        for (int n = 0; n < nsamp; n++) if (ba_dtmf_push(&d, x[n])) { char z = ba_dtmf_stream(&d); if (z && ng < 63) got[ng++] = z; }
        got[ng] = 0;
        if (strcmp(got, want)) { bad++; if (false_digits && ng > 16) (*false_digits)++; }
    }
    free(x); return bad;
}
static void test_dtmf(void)
{
    printf("\n== 3. DTMF decoder (Goertzel) ==\n");
    /* single-digit sanity for every key, no noise */
    int ok = 0;
    for (int k = 0; k < 16; k++) {
        double r, c; dtmf_freqs(digits[k], &r, &c); ba_dtmf_t d; ba_dtmf_init(&d); char got = 0;
        for (int n = 0; n < 205 * 4; n++) { if (ba_dtmf_push(&d, cl(1000 * (sin(2 * M_PI * r * n / RATE) + sin(2 * M_PI * c * n / RATE))))) { char z = ba_dtmf_stream(&d); if (z) got = z; } }
        if (got == digits[k]) ok++;
    }
    CHECK(ok == 16, "all 16 keys decode with no noise (%d of 16)", ok);
    /* power agreement with a double Goertzel */
    { double r = 770, c = 1336; int16_t x[205]; for (int n = 0; n < 205; n++) x[n] = cl(1500 * (sin(2 * M_PI * r * n / RATE) + sin(2 * M_PI * c * n / RATE)));
      /* the block decision is the product; compare the double power at 770 Hz with the energy-share rule instead */
      double w = 2 * M_PI * 770 / RATE, co = 2 * cos(w), s1 = 0, s2 = 0, e = 0; for (int n = 0; n < 205; n++) { double s = x[n] + co * s1 - s2; s2 = s1; s1 = s; e += (double)x[n] * x[n]; }
      double p = s1 * s1 + s2 * s2 - co * s1 * s2; printf("  double Goertzel: 770 Hz holds %.0f percent of the block energy (rule needs both tones to hold 50 percent together)\n", 100 * p / (e * 205 / 2)); }
    int fd = 0; int bad0 = dtmf_run(40, 200, &fd);
    CHECK(bad0 == 0, "16-digit sequences decode exactly at 40 dB SNR (%d of 200 wrong)", bad0);
    int shares[2] = { 8, 4 };
    for (int si = 0; si < 2; si++) {
        g_share = shares[si];
        printf("  energy share rule %d/16 -- wrong 16-digit sequences out of 200 by SNR (dB; signal power over noise power across 0 to 4 kHz):\n  ", g_share);
        double first_fail = 99, half_fail = 99;
        for (int snr = 12; snr >= -12; snr--) {
            int bad = dtmf_run(snr, 200, 0);
            if (snr % 2 == 0 || bad) printf("%d: %d  ", snr, bad);
            if (bad >= 2 && first_fail == 99) first_fail = snr;   /* 1 percent or more of the sequences wrong */
            if (bad >= 100 && half_fail == 99) half_fail = snr;
        }
        printf("\n  first SNR with 1 percent or more of sequences wrong: %.0f dB; with half wrong: %.0f dB\n", first_fail, half_fail);
        if (si == 0) CHECK(first_fail <= 3 && first_fail >= -3, "default rule breaks near 0 dB SNR (first failure at %.0f dB)", first_fail);
        int fdig = 0; ba_dtmf_t d; ba_dtmf_init(&d); d.share16 = (uint8_t)g_share; int blocks = 0;
        for (int n = 0; n < 205 * 20000; n++) if (ba_dtmf_push(&d, cl(grand() * 1500))) { blocks++; if (ba_dtmf_block_digit(&d)) fdig++; }
        printf("  noise alone: %d false digits in %d blocks\n", fdig, blocks);
    }
    g_share = 8;
    /* noise alone */
    { int fdig = 0; ba_dtmf_t d; ba_dtmf_init(&d); int blocks = 0; for (int n = 0; n < 205 * 1000; n++) if (ba_dtmf_push(&d, cl(grand() * 1500))) { blocks++; if (ba_dtmf_block_digit(&d)) fdig++; }
      CHECK(fdig == 0, "noise alone (RMS 1500) gives no digit in %d blocks (%d false)", blocks, fdig); }
    /* a tune (not DTMF): chiptune-like two notes in the same group */
    { ba_dtmf_t d; ba_dtmf_init(&d); int fdig = 0; for (int n = 0; n < 205 * 200; n++) if (ba_dtmf_push(&d, cl(1500 * (sin(2 * M_PI * 697 * n / RATE) + sin(2 * M_PI * 852 * n / RATE))))) if (ba_dtmf_block_digit(&d)) fdig++;
      CHECK(fdig == 0, "two tones of the same group (697 + 852 Hz) are rejected"); }
    int16_t x[205]; ba_dtmf_t d; ba_dtmf_init(&d); for (int n = 0; n < 205; n++) x[n] = cl(1000 * sin(n * 0.3));
    double t0 = now_ns(); for (int i = 0; i < 5000; i++) for (int n = 0; n < 205; n++) ba_dtmf_push(&d, x[n]); double ns = (now_ns() - t0) / 5000;
    printf("  one 205-sample block (25.6 ms of audio): %.1f us on this host, %d multiplies\n", ns / 1000, 8 * 205);
}

/* ---- gate and echo ---- */
static void test_gate_echo(void)
{
    printf("\n== 4. Noise gate and echo ==\n");
    enum { N = 8 * RATE };
    static int16_t in[N], gated[N], outp[N], dacout[N];
    /* 8 s: noise floor (RMS 60 of 32767) all the time; 1 kHz tone bursts of amplitude 6000 at 1-2 s and 4-5 s */
    for (int n = 0; n < N; n++) { double t = n / (double)RATE, v = grand() * 60; if ((t >= 1 && t < 2) || (t >= 4 && t < 5)) v += 6000 * sin(2 * M_PI * 1000.0 * t); in[n] = cl(v); }
    ba_gate_t g; ba_gate_init(&g, 800, 400, 400, 2, 30, RATE);
    for (int n = 0; n < N; n++) gated[n] = ba_gate_process(&g, in[n]);
    double ein = 0, eg = 0, tin = 0, tg = 0; int cn = 0, tn = 0;
    for (int n = 0; n < N; n++) { double t = n / (double)RATE;
        if ((t > 2.5 && t < 4) || (t > 5.5)) { ein += (double)in[n] * in[n]; eg += (double)gated[n] * gated[n]; cn++; }
        if ((t > 1.1 && t < 1.9) || (t > 4.1 && t < 4.9)) { tin += (double)in[n] * in[n]; tg += (double)gated[n] * gated[n]; tn++; } }
    double att = 10 * log10((ein / cn) / (eg / cn + 1e-9)), pass = 10 * log10((tg / tn) / (tin / tn));
    int first_open = -1; for (int n = 8000; n < 12000; n++) if (abs(gated[n]) > 3000) { first_open = n - 8000; break; }
    printf("  closed: %.1f dB below the input; open: %+.2f dB; first sample above half level %.1f ms after the burst begins\n", att, pass, first_open * 1000.0 / RATE);
    CHECK(att >= 40.0, "noise floor attenuated by at least 40 dB when closed (%.1f dB)", att);
    CHECK(fabs(pass) < 0.5, "tone passes within 0.5 dB when open (%+.2f dB)", pass);
    /* echo: impulse response against the double reference */
    enum { DL = 2000 }; static int16_t line[DL]; ba_echo_t e; ba_echo_init(&e, line, DL, 16384, 16384); /* delay 250 ms, feedback 0.5, wet 0.5 */
    static int16_t imp[DL * 6]; for (int n = 0; n < DL * 6; n++) imp[n] = ba_echo_process(&e, n == 0 ? 16000 : 0);
    double worst = 0;
    for (int k = 1; k <= 5; k++) { double want = 16000 * pow(0.5, k - 1) * 0.5; double err = fabs(imp[k * DL] - want); if (err > worst) worst = err; }
    printf("  impulse 16000: repeats %d %d %d %d %d (double: 8000 4000 2000 1000 500)\n", imp[DL], imp[2 * DL], imp[3 * DL], imp[4 * DL], imp[5 * DL]);
    CHECK(worst <= 2.0 && imp[0] == 16000, "echo repeats match the double reference within 2 LSB (worst %.1f)", worst);
    /* the chain through the DAC8311 frame code */
    ba_gate_init(&g, 800, 400, 400, 2, 30, RATE); ba_echo_init(&e, line, DL, 12000, 14000);
    double mxerr = 0;
    for (int n = 0; n < N; n++) { outp[n] = ba_echo_process(&e, ba_gate_process(&g, in[n])); dacout[n] = through_dac(outp[n]); double er = fabs((double)dacout[n] - outp[n]); if (er > mxerr) mxerr = er; }
    CHECK(mxerr <= 8, "gate then echo through the DAC8311 model: output within 8 LSB of the digital value (worst %.0f, 14-bit quantisation)", mxerr);
    char path[600]; snprintf(path, sizeof path, "%s/algos_gate_echo.wav", capfile ? "out" : "."); wav(path, dacout, N);
    /* gate alone through the DAC */
    for (int n = 0; n < N; n++) dacout[n] = through_dac(gated[n]); wav("out/algos_gate_only.wav", dacout, N);
    ba_gate_t g2; ba_gate_init(&g2, 800, 400, 400, 2, 30, RATE); ba_echo_t e2; ba_echo_init(&e2, line, DL, 12000, 14000);
    double t0 = now_ns(); volatile int16_t sink = 0; for (int r = 0; r < 200; r++) for (int n = 0; n < RATE; n++) sink = ba_echo_process(&e2, ba_gate_process(&g2, in[n])); double ns = (now_ns() - t0) / (200.0 * RATE);
    printf("  gate + echo: %.1f ns per sample on this host; echo line %d bytes of RAM for 250 ms\n", ns, DL * 2); (void)sink;
}

int main(int argc, char **argv)
{
    capfile = argc > 1 ? argv[1] : "../results/am13e230-20261020/console-run3.log";
    test_tuner(); test_spectrum(); test_dtmf(); test_gate_echo();
    printf(fails ? "\nFAILED %d\n" : "\nall checks pass\n", fails);
    return fails ? 1 : 0;
}
