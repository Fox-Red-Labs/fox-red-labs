/* The drawing grid and the drawing-to-input step, shared by the board demo and the Mac test. GPL-3.0. */
#ifndef PREPARE_H
#define PREPARE_H
#include <stdint.h>
/* drawing box: 288 px square, kept as a 72 x 72 grid of 4-pixel cells */
#define BX 16
#define BY 56
#define BS 288
#define CELL 4
#define N (BS / CELL)
#define BRUSH 3                 /* cells: a 28-pixel-wide stroke */
static uint8_t grid[N * N / 8];
#define GET(x, y) (grid[((y) * N + (x)) >> 3] & (1u << (((y) * N + (x)) & 7)))
#define SET(x, y) (grid[((y) * N + (x)) >> 3] |= (1u << (((y) * N + (x)) & 7)))

/* the drawing as BitNetMCU's 16 x 16 input; returns 0 if the box is empty */
static int prepare(int8_t *out)
{
    int minx = N, miny = N, maxx = -1, maxy = -1; float sx = 0, sy = 0; int cnt = 0;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            if (GET(x, y)) {
                if (x < minx) minx = x; if (x > maxx) maxx = x;
                if (y < miny) miny = y; if (y > maxy) maxy = y;
                sx += x + 0.5f; sy += y + 0.5f; cnt++;
            }
    if (!cnt) return 0;
    int side = (maxx - minx > maxy - miny ? maxx - minx : maxy - miny) + 1;
    float s = 20.0f / side, comx = sx / cnt, comy = sy / cnt;   /* frame pixels per cell, center of mass */
    float v[256], vmax = 0;
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int hit = 0;                          /* 4 x 4 samples per output pixel: area coverage */
            for (int b = 0; b < 4; b++)
                for (int a = 0; a < 4; a++) {
                    float fx = (i + (a + 0.5f) / 4) * 1.75f, fy = (j + (b + 0.5f) / 4) * 1.75f;   /* 28-pixel frame */
                    int cx = (int) ((fx - 14.0f) / s + comx), cy = (int) ((fy - 14.0f) / s + comy);
                    if (cx >= 0 && cy >= 0 && cx < N && cy < N && GET(cx, cy)) hit++;
                }
            float p = (hit / 16.0f - 0.1307f) / 0.3081f;   /* torchvision Normalize, MNIST mean and std */
            v[j * 16 + i] = p;
            if ((p < 0 ? -p : p) > vmax) vmax = p < 0 ? -p : p;
        }
    for (int k = 0; k < 256; k++) {               /* per-image scale to +-127, as test_inference.py exports */
        float q = v[k] * 127.0f / vmax;
        out[k] = (int8_t) (q < 0 ? q - 0.5f : q + 0.5f);
    }
    return 1;
}

#endif
