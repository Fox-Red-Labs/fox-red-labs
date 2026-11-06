/*
 * Headless host runner for the Peanut-GB front end (Fox Red Labs, BSD-3-Clause).
 *
 *   pgb_host --rom FILE [--frames N] [--script FILE] [--png-dir DIR] [--name NAME] [--dump 100,400]
 *            [--scale 3] [--stream] [--fmt rgb565|1bpp] [--expect-differ A:B] [--expect-lit A] [--bench N]
 *
 * Script lines: "<frame> <keys>", keys = NONE or UP DOWN LEFT RIGHT A B joined by +; held until the next line.
 * --bench N runs N frames three times with the emulator alone and three times with the 128 x 128 scaling and
 * display write, and prints frames per second for each (process CPU time, median of three).
 */
#include <frl/abstract.h>
#include "pgb_frontend.h"
#include "../../arduboy/host/png_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

static uint32_t g_ms = 0, g_flushes = 0;
static uint8_t g_rgb[FRL_SCREEN_W * FRL_SCREEN_H * 3];
static frl_fmt_t g_fmt = FRL_FMT_RGB565;
static int g_keys = 0;
static bool g_sink_only = false;

static uint32_t h_millis(void *) { return g_ms; }
static void h_delay(void *, uint32_t ms) { g_ms += ms; }
static void h_read(void *, frl_input_raw_t *o)
{
    int x = 1816, y = 2159;
    if (g_keys & 1) y += 1700;
    if (g_keys & 2) y -= 1700;
    if (g_keys & 4) x -= 1500;
    if (g_keys & 8) x += 1500;
    o->joy_x = (uint16_t)x; o->joy_y = (uint16_t)y;
    o->buttons = (uint8_t)(((g_keys & 16) ? FRL_BTN_1 : 0) | ((g_keys & 32) ? FRL_BTN_2 : 0));
}
static volatile uint32_t g_sink;
static void h_lines(void *, uint16_t y0, uint16_t n, const void *pix)
{
    if (g_sink_only) { g_sink += ((const uint8_t *)pix)[0] + y0 + n; return; }
    for (uint16_t k = 0; k < n; k++) {
        uint8_t *dst = g_rgb + (size_t)(y0 + k) * FRL_SCREEN_W * 3;
        for (uint16_t x = 0; x < FRL_SCREEN_W; x++) {
            uint8_t r, g, b;
            if (g_fmt == FRL_FMT_RGB565) {
                uint16_t v = ((const uint16_t *)pix)[k * FRL_SCREEN_W + x];
                r = (uint8_t)(((v >> 11) & 31) * 255 / 31); g = (uint8_t)(((v >> 5) & 63) * 255 / 63); b = (uint8_t)((v & 31) * 255 / 31);
            } else {
                bool on = (((const uint8_t *)pix)[k * (FRL_SCREEN_W / 8) + (x >> 3)] >> (x & 7)) & 1;
                r = g = b = on ? 255 : 0;
            }
            dst[x * 3] = r; dst[x * 3 + 1] = g; dst[x * 3 + 2] = b;
        }
    }
}
static void h_flush(void *) { g_flushes++; }

/* --stream: the streaming path a board driver uses (one window per frame, 128 rows of 256 bytes in wire order). Rows are copied
 * into the same picture buffer, so the frames can be compared byte for byte with the write_lines path. */
static bool g_stream_on = false;
static uint32_t g_srow = 0, g_sbad = 0;
static void h_sbegin(void *) { g_srow = 0; }
static void h_srow(void *, const void *row, uint16_t bytes)
{
    if (bytes != FRL_SCREEN_W * 2 || g_srow >= FRL_SCREEN_H) { g_sbad++; return; }
    const uint8_t *p = (const uint8_t *)row;
    uint8_t *dst = g_rgb + (size_t)g_srow * FRL_SCREEN_W * 3;
    for (uint16_t x = 0; x < FRL_SCREEN_W; x++) {
        uint16_t v = (uint16_t)((p[2 * x] << 8) | p[2 * x + 1]);
        dst[x * 3] = (uint8_t)(((v >> 11) & 31) * 255 / 31); dst[x * 3 + 1] = (uint8_t)(((v >> 5) & 63) * 255 / 63); dst[x * 3 + 2] = (uint8_t)((v & 31) * 255 / 31);
    }
    g_srow++;
}
static void h_send(void *) { if (g_srow != FRL_SCREEN_H) g_sbad++; }

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

static double now_s() { struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

struct Snap { uint32_t lit; uint64_t hash; std::vector<uint8_t> rgb; };

int main(int argc, char **argv)
{
    std::string rom_path, script, pngdir = ".", name = "gb";
    uint32_t nframes = 600, bench = 0; int scale = 3;
    std::vector<uint32_t> dumps; std::vector<std::pair<uint32_t, uint32_t>> differ; std::vector<uint32_t> lit;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
        if (a == "--rom") rom_path = next();
        else if (a == "--frames") nframes = (uint32_t)atoi(next().c_str());
        else if (a == "--script") script = next();
        else if (a == "--png-dir") pngdir = next();
        else if (a == "--name") name = next();
        else if (a == "--scale") scale = atoi(next().c_str());
        else if (a == "--stream") g_stream_on = true;
        else if (a == "--bench") bench = (uint32_t)atoi(next().c_str());
        else if (a == "--fmt") g_fmt = next() == "1bpp" ? FRL_FMT_1BPP : FRL_FMT_RGB565;
        else if (a == "--dump") { std::string v = next(); size_t p = 0; while (p < v.size()) { size_t q = v.find(',', p); if (q == std::string::npos) q = v.size(); dumps.push_back((uint32_t)atoi(v.substr(p, q - p).c_str())); p = q + 1; } }
        else if (a == "--expect-differ") { std::string v = next(); size_t c = v.find(':'); differ.push_back({ (uint32_t)atoi(v.c_str()), (uint32_t)atoi(v.c_str() + c + 1) }); }
        else if (a == "--expect-lit") lit.push_back((uint32_t)atoi(next().c_str()));
    }
    for (auto &d : differ) { dumps.push_back(d.first); dumps.push_back(d.second); }
    for (auto l : lit) dumps.push_back(l);

    FILE *rf = fopen(rom_path.c_str(), "rb");
    if (!rf) { fprintf(stderr, "cannot open ROM %s\n", rom_path.c_str()); return 2; }
    std::vector<uint8_t> rom; { fseek(rf, 0, SEEK_END); long n = ftell(rf); rewind(rf); rom.resize((size_t)n); if (fread(rom.data(), 1, rom.size(), rf) != rom.size()) return 2; fclose(rf); }

    std::vector<std::pair<uint32_t, int>> sc;
    if (!script.empty()) {
        FILE *f = fopen(script.c_str(), "r"); if (!f) { fprintf(stderr, "cannot open %s\n", script.c_str()); return 2; }
        char line[256];
        while (fgets(line, sizeof line, f)) { if (line[0] == '#' || line[0] == '\n') continue; char keys[128] = "NONE"; unsigned fr; if (sscanf(line, "%u %127s", &fr, keys) >= 1) sc.push_back({ fr, parse_keys(keys) }); }
        fclose(f);
    }

    frl_platform_t pf; memset(&pf, 0, sizeof pf);
    pf.display.fmt = g_fmt; pf.display.fg = 0xFFFF; pf.display.write_lines = h_lines; pf.display.flush = h_flush;
    if (g_stream_on) { pf.display.stream_begin = h_sbegin; pf.display.stream_row = h_srow; pf.display.stream_end = h_send; pf.display.stream_be16 = 1; }
    pf.input.read = h_read; pf.millis = h_millis; pf.delay_ms = h_delay;
    frl_set_platform(&pf);

    static uint8_t cram[32768];
    auto start = [&]() { int r = pgb_init(rom.data(), rom.size(), cram, sizeof cram); if (r) { fprintf(stderr, "pgb_init failed (%d): %s\n", r, pgb_error_text(r)); exit(2); } g_keys = 0; g_ms = 0; g_flushes = 0; };

    if (bench) {
        std::vector<double> emu, full;
        for (int mode = 0; mode < 2; mode++) {
            for (int rep = 0; rep < 3; rep++) {
                start(); g_sink_only = true; size_t si = 0;
                double t0 = now_s();
                for (uint32_t f = 0; f < bench; f++) {
                    while (si < sc.size() && sc[si].first <= f) g_keys = sc[si++].second;
                    pgb_run_frame();
                    if (mode == 1) pgb_present();
                }
                double dt = now_s() - t0;
                (mode ? full : emu).push_back(bench / dt);
            }
        }
        std::sort(emu.begin(), emu.end()); std::sort(full.begin(), full.end());
        printf("bench %u frames x3: emulator alone %.0f fps (runs %.0f %.0f %.0f), with 128x128 scaling and display write %.0f fps (runs %.0f %.0f %.0f)\n",
               bench, emu[1], emu[0], emu[1], emu[2], full[1], full[0], full[1], full[2]);
        printf("bench: scaling and write cost %.1f percent of the frame time; game speed is 59.73 fps so the emulator alone is %.0f times real time\n",
               100.0 * (1.0 / full[1] - 1.0 / emu[1]) / (1.0 / full[1]), emu[1] / 59.73);
        return 0;
    }

    start();
    std::map<uint32_t, Snap> snaps; size_t si = 0;
    for (uint32_t f = 0; f < nframes; f++) {
        while (si < sc.size() && sc[si].first <= f) g_keys = sc[si++].second;
        pgb_run_frame();
        pgb_present();
        uint32_t idx = f + 1;
        for (uint32_t d : dumps) if (d == idx && !snaps.count(d)) {
            Snap s; s.hash = 1469598103934665603ull; s.lit = 0;
            for (size_t i = 0; i < sizeof g_rgb; i++) { s.hash ^= g_rgb[i]; s.hash *= 1099511628211ull; if (i % 3 == 0 && g_rgb[i] < 128) s.lit++; }
            s.rgb.assign(g_rgb, g_rgb + sizeof g_rgb);
            snaps[d] = s;
        }
    }
    int fails = 0;
    if (g_stream_on) { printf("%s stream: %u bad row or window calls\n", g_sbad ? "FAIL" : "OK", g_sbad); if (g_sbad) fails++; }
    printf("%s: %u frames run (%.1f s of game time), %u presented\n", name.c_str(), nframes, nframes / 59.73, g_flushes);
    for (auto &kv : snaps) {
        char path[512]; snprintf(path, sizeof path, "%s/%s-f%05u.png", pngdir.c_str(), name.c_str(), kv.first);
        std::vector<uint8_t> big((size_t)FRL_SCREEN_W * scale * FRL_SCREEN_H * scale * 3);
        for (int y = 0; y < FRL_SCREEN_H * scale; y++) for (int x = 0; x < FRL_SCREEN_W * scale; x++)
            memcpy(&big[((size_t)y * FRL_SCREEN_W * scale + x) * 3], &kv.second.rgb[((size_t)(y / scale) * FRL_SCREEN_W + x / scale) * 3], 3);
        png_write_rgb(path, big.data(), FRL_SCREEN_W * scale, FRL_SCREEN_H * scale);
        printf("frame %u: hash %016llx, dark pixels %u -> %s\n", kv.first, (unsigned long long)kv.second.hash, kv.second.lit, path);
    }
    for (auto &d : differ) {
        uint32_t n = 0; auto &a = snaps[d.first], &b = snaps[d.second];
        for (size_t i = 0; i < a.rgb.size(); i += 3) if (a.rgb[i] != b.rgb[i]) n++;
        printf("%s differ %u:%u (%u pixels differ)\n", n ? "OK" : "FAIL", d.first, d.second, n); if (!n) fails++;
    }
    for (auto l : lit) { bool ok = snaps.count(l) && snaps[l].lit > 0; printf("%s lit %u\n", ok ? "OK" : "FAIL", l); if (!ok) fails++; }
    return fails ? 1 : 0;
}
