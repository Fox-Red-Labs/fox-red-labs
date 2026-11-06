/*
 * Headless host backend for the Arduboy2 layer (Fox Red Labs, BSD-3-Clause).
 *
 * Runs a game's setup()/loop() against a virtual clock, feeds scripted input through the
 * same joystick-plus-two-buttons path a board uses, renders each frame through the
 * abstract display (RGB565 or 1BPP) and writes chosen frames as PNG files.
 *
 *   ab_host --frames N [--script FILE] [--png-dir DIR] [--dump 30,90] [--scale 3]
 *           [--fit center|2xv] [--fmt rgb565|1bpp] [--expect-differ A:B] [--expect-lit A]
 *           [--expect-tone]
 *
 * Script lines: "<frame> <keys>"; keys = NONE or UP DOWN LEFT RIGHT A B joined by +.
 * The keys stay held from that frame until the next script line.
 * One loop() call is one virtual millisecond, so a 60 fps game shows a frame every 16 calls.
 * Exit code 0 only if every --expect-* check passes.
 */
#include <frl/abstract.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include "png_write.h"

extern void setup();
extern void loop();

static uint32_t g_ms = 0;
static uint32_t g_frames = 0;
static uint8_t g_fb_rgb[FRL_SCREEN_W * FRL_SCREEN_H * 3];
static frl_fmt_t g_fmt = FRL_FMT_RGB565;

static int g_keys = 0;   /* bit0 UP 1 DOWN 2 LEFT 3 RIGHT 4 A 5 B */
static uint32_t g_tones = 0;
static uint16_t g_last_hz = 0, g_last_ms = 0;

static uint32_t h_millis(void *) { return g_ms; }
static void h_delay(void *, uint32_t ms) { g_ms += ms; }
static uint32_t h_entropy(void *) { return 0x1234ABCDu; }

static void h_read(void *, frl_input_raw_t *o)
{
    /* a stick that rests off centre, like the real one: X 1816, Y 2159 */
    int x = 1816, y = 2159;
    if (g_keys & 1) y += 1700;      /* UP    */
    if (g_keys & 2) y -= 1700;      /* DOWN  */
    if (g_keys & 4) x -= 1500;      /* LEFT  */
    if (g_keys & 8) x += 1500;      /* RIGHT */
    o->joy_x = (uint16_t)x; o->joy_y = (uint16_t)y;
    o->buttons = (uint8_t)(((g_keys & 16) ? FRL_BTN_1 : 0) | ((g_keys & 32) ? FRL_BTN_2 : 0));
}

static void h_tone(void *, uint16_t hz, uint16_t ms) { if (hz) { g_tones++; g_last_hz = hz; g_last_ms = ms; } }

static void h_write_lines(void *, uint16_t y0, uint16_t n, const void *pix)
{
    for (uint16_t k = 0; k < n; k++) {
        uint8_t *dst = g_fb_rgb + (size_t)(y0 + k) * FRL_SCREEN_W * 3;
        for (uint16_t x = 0; x < FRL_SCREEN_W; x++) {
            uint8_t r, g, b;
            if (g_fmt == FRL_FMT_RGB565) {
                uint16_t v = ((const uint16_t *)pix)[k * FRL_SCREEN_W + x];
                r = (uint8_t)(((v >> 11) & 31) * 255 / 31); g = (uint8_t)(((v >> 5) & 63) * 255 / 63); b = (uint8_t)((v & 31) * 255 / 31);
            } else {
                const uint8_t *row = (const uint8_t *)pix + k * (FRL_SCREEN_W / 8);
                bool on = (row[x >> 3] >> (x & 7)) & 1;
                r = g = b = on ? 255 : 0;
            }
            dst[x * 3] = r; dst[x * 3 + 1] = g; dst[x * 3 + 2] = b;
        }
    }
}
static void h_flush(void *) { g_frames++; }

struct Snap { uint64_t hash; uint32_t lit; std::vector<uint8_t> rgb; };
static std::map<uint32_t, Snap> g_snaps;

static Snap snap_now()
{
    Snap s; s.hash = 1469598103934665603ull; s.lit = 0;
    for (size_t i = 0; i < sizeof g_fb_rgb; i++) { s.hash ^= g_fb_rgb[i]; s.hash *= 1099511628211ull; if (i % 3 == 0 && g_fb_rgb[i] > 127) s.lit++; }
    s.rgb.assign(g_fb_rgb, g_fb_rgb + sizeof g_fb_rgb);
    return s;
}

static int parse_keys(const std::string &s)
{
    int k = 0; size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find('+', i); if (j == std::string::npos) j = s.size();
        std::string t = s.substr(i, j - i);
        if (t == "UP") k |= 1; else if (t == "DOWN") k |= 2; else if (t == "LEFT") k |= 4; else if (t == "RIGHT") k |= 8;
        else if (t == "A") k |= 16; else if (t == "B") k |= 32;
        i = j + 1;
    }
    return k;
}

static uint32_t diff_px(const Snap &a, const Snap &b)
{
    uint32_t d = 0;
    for (size_t i = 0; i < a.rgb.size(); i += 3) if (a.rgb[i] != b.rgb[i]) d++;
    return d;
}

int main(int argc, char **argv)
{
    uint32_t nframes = 300; std::string script, pngdir = "."; std::vector<uint32_t> dumps; int scale = 3;
    frl_fit_t fit = FRL_FIT_CENTER; std::vector<std::pair<uint32_t, uint32_t>> differ; std::vector<uint32_t> lit; bool want_tone = false;
    std::string name = "game";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
        if (a == "--frames") nframes = (uint32_t)atoi(next().c_str());
        else if (a == "--script") script = next();
        else if (a == "--png-dir") pngdir = next();
        else if (a == "--name") name = next();
        else if (a == "--scale") scale = atoi(next().c_str());
        else if (a == "--dump") { std::string v = next(); size_t p = 0; while (p < v.size()) { size_t q = v.find(',', p); if (q == std::string::npos) q = v.size(); dumps.push_back((uint32_t)atoi(v.substr(p, q - p).c_str())); p = q + 1; } }
        else if (a == "--fit") fit = next() == "2xv" ? FRL_FIT_2X_V : FRL_FIT_CENTER;
        else if (a == "--fmt") g_fmt = next() == "1bpp" ? FRL_FMT_1BPP : FRL_FMT_RGB565;
        else if (a == "--expect-differ") { std::string v = next(); size_t c = v.find(':'); differ.push_back({ (uint32_t)atoi(v.c_str()), (uint32_t)atoi(v.c_str() + c + 1) }); }
        else if (a == "--expect-lit") lit.push_back((uint32_t)atoi(next().c_str()));
        else if (a == "--expect-tone") want_tone = true;
    }
    for (auto &d : differ) { dumps.push_back(d.first); dumps.push_back(d.second); }
    for (auto l : lit) dumps.push_back(l);

    std::vector<std::pair<uint32_t, int>> sc;
    if (!script.empty()) {
        FILE *f = fopen(script.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", script.c_str()); return 2; }
        char line[256];
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char keys[128] = "NONE"; unsigned fr;
            if (sscanf(line, "%u %127s", &fr, keys) >= 1) sc.push_back({ fr, parse_keys(keys) });
        }
        fclose(f);
    }

    frl_platform_t pf; memset(&pf, 0, sizeof pf);
    pf.display.fmt = g_fmt; pf.display.fg = 0xFFFF; pf.display.bg = 0x0000; pf.display.invert = 0;
    pf.display.write_lines = h_write_lines; pf.display.flush = h_flush;
    pf.input.read = h_read; pf.audio.tone = h_tone;
    pf.millis = h_millis; pf.delay_ms = h_delay; pf.entropy = h_entropy;
    frl_set_platform(&pf);
    frl_set_fit(fit);

    setup();
    uint32_t seen = 0, loops = 0, last_dump_frame = (uint32_t)-1;
    size_t si = 0;
    (void)last_dump_frame;
    while (g_frames < nframes && loops < nframes * 400u) {
        while (si < sc.size() && sc[si].first <= g_frames) g_keys = sc[si++].second;
        loop();
        g_ms++; loops++;
        if (g_frames != seen) {
            seen = g_frames;
            for (uint32_t d : dumps) if (d == seen && !g_snaps.count(d)) g_snaps[d] = snap_now();
        }
    }

    int fails = 0;
    printf("%s: %u frames presented, %u loop calls, %u ms virtual, %u tones (last %u Hz %u ms)\n", name.c_str(), g_frames, loops, g_ms, g_tones, g_last_hz, g_last_ms);
    for (auto &kv : g_snaps) {
        char path[512]; snprintf(path, sizeof path, "%s/%s-f%04u.png", pngdir.c_str(), name.c_str(), kv.first);
        std::vector<uint8_t> big((size_t)FRL_SCREEN_W * scale * FRL_SCREEN_H * scale * 3);
        for (int y = 0; y < FRL_SCREEN_H * scale; y++) for (int x = 0; x < FRL_SCREEN_W * scale; x++)
            memcpy(&big[((size_t)y * FRL_SCREEN_W * scale + x) * 3], &kv.second.rgb[((size_t)(y / scale) * FRL_SCREEN_W + x / scale) * 3], 3);
        png_write_rgb(path, big.data(), FRL_SCREEN_W * scale, FRL_SCREEN_H * scale);
        printf("frame %u: hash %016llx, lit pixels %u -> %s\n", kv.first, (unsigned long long)kv.second.hash, kv.second.lit, path);
    }
    for (auto &d : differ) {
        if (!g_snaps.count(d.first) || !g_snaps.count(d.second)) { printf("FAIL differ %u:%u (frame not reached)\n", d.first, d.second); fails++; continue; }
        uint32_t n = diff_px(g_snaps[d.first], g_snaps[d.second]);
        printf("%s differ %u:%u (%u pixels differ)\n", n ? "OK" : "FAIL", d.first, d.second, n); if (!n) fails++;
    }
    for (auto l : lit) {
        bool ok = g_snaps.count(l) && g_snaps[l].lit > 0;
        printf("%s lit %u\n", ok ? "OK" : "FAIL", l); if (!ok) fails++;
    }
    if (want_tone) { printf("%s tone (%u events)\n", g_tones ? "OK" : "FAIL", g_tones); if (!g_tones) fails++; }
    return fails ? 1 : 0;
}
