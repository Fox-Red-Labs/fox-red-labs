/* Headless host platform layer for doomgeneric (GPL-2.0, see ../LICENSE).
 * Writes every Nth frame as a PPM at 320x200 and as a 128x128 box-filtered copy.
 * Keys: none (demo playback only). Clock: the host monotonic clock.
 * Env: DOOM_FRAMES_DIR (output folder), DOOM_FRAME_EVERY (default 35 frames). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "doomgeneric.h"

static long frame_no;
static uint32_t crc;
static long max_frames;

/* CRC-32 (zlib polynomial) over each frame's 64,000 index bytes and 768 palette bytes, the same sum the emulator harness makes */
static uint32_t crc32_update(uint32_t c, const uint8_t *p, size_t n)
{
    c = ~c;
    while (n--) { int k; c ^= *p++; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return ~c;
}
static void report(void) { printf("HOSTFRAMES frames=%ld framecrc=%08x\n", frame_no, crc); }
static long every = 35;
static const char *dir = "frames";

void DG_Init(void)
{
    const char *e = getenv("DOOM_FRAME_EVERY");
    const char *d = getenv("DOOM_FRAMES_DIR");
    if (e) every = atol(e);
    if (getenv("DOOM_MAX_FRAMES")) max_frames = atol(getenv("DOOM_MAX_FRAMES"));
    atexit(report);
    if (d) dir = d;
}

static void write_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 3, (size_t)w * h, f);
    fclose(f);
}

void DG_DrawFrame8(const uint8_t *idx, const uint8_t *pal)
{
    static uint8_t full[320 * 200 * 3], small[128 * 128 * 3];
    char path[512];
    int x, y;
    crc = crc32_update(crc, idx, 64000);
    crc = crc32_update(crc, pal, 768);
    if (max_frames && frame_no + 1 >= max_frames) exit(0);
    if (every > 0 && frame_no % every == 0)
    {
        for (y = 0; y < 200; y++)
            for (x = 0; x < 320; x++)
                memcpy(&full[(y * 320 + x) * 3], &pal[idx[y * 320 + x] * 3], 3);
        snprintf(path, sizeof path, "%s/f%05ld-320x200.ppm", dir, frame_no);
        write_ppm(path, full, 320, 200);
        /* 128x128: area average of the source rectangle behind each output pixel */
        for (y = 0; y < 128; y++)
            for (x = 0; x < 128; x++)
            {
                int x0 = x * 320 / 128, x1 = (x + 1) * 320 / 128, y0 = y * 200 / 128, y1 = (y + 1) * 200 / 128;
                int sx, sy, n = 0, r = 0, g = 0, b = 0;
                for (sy = y0; sy < y1 || sy == y0; sy++)
                    for (sx = x0; sx < x1 || sx == x0; sx++)
                    {
                        const uint8_t *p = &pal[idx[sy * 320 + sx] * 3];
                        r += p[0]; g += p[1]; b += p[2]; n++;
                    }
                small[(y * 128 + x) * 3] = r / n;
                small[(y * 128 + x) * 3 + 1] = g / n;
                small[(y * 128 + x) * 3 + 2] = b / n;
            }
        snprintf(path, sizeof path, "%s/f%05ld-128x128.ppm", dir, frame_no);
        write_ppm(path, small, 128, 128);
    }
    frame_no++;
}

void DG_DrawFrame(void) {}
void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }
uint32_t DG_GetTicksMs(void)
{
    struct timespec ts;
    static int fake = -1;
    static uint32_t fake_ms;
    if (fake < 0) fake = getenv("DOOM_FAKE_CLOCK") != NULL;
    if (fake) return ++fake_ms;   /* one millisecond per call: same fake clock as the M33 emulator harness */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
int DG_GetKey(int *pressed, unsigned char *key) { (void)pressed; (void)key; return 0; }
void DG_SetWindowTitle(const char *title) { (void)title; }

#ifdef DG_HEADLESS_MAIN
void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick(void);
int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    for (;;) doomgeneric_Tick();
}
#endif
