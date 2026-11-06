/* Minimal PNG writer: 8-bit RGB, zlib-compressed. Fox Red Labs, BSD-3-Clause. */
#ifndef FRL_PNG_WRITE_H
#define FRL_PNG_WRITE_H
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <zlib.h>

static inline uint32_t png_crc(const uint8_t *d, size_t n, uint32_t crc = 0xFFFFFFFFu)
{
    static uint32_t table[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}
static inline void png_chunk(FILE *f, const char *type, const std::vector<uint8_t> &data)
{
    uint8_t len[4] = { (uint8_t)(data.size() >> 24), (uint8_t)(data.size() >> 16), (uint8_t)(data.size() >> 8), (uint8_t)data.size() };
    fwrite(len, 1, 4, f);
    std::vector<uint8_t> buf(type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    fwrite(buf.data(), 1, buf.size(), f);
    uint32_t c = ~png_crc(buf.data(), buf.size());
    uint8_t cb[4] = { (uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
    fwrite(cb, 1, 4, f);
}
/* rgb: w*h*3 bytes */
static inline bool png_write_rgb(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb"); if (!f) return false;
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr = { (uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                                  (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0 };
    png_chunk(f, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb + (size_t)y * w * 3, rgb + (size_t)(y + 1) * w * 3); }
    uLongf zl = compressBound((uLong)raw.size());
    std::vector<uint8_t> z(zl);
    compress2(z.data(), &zl, raw.data(), (uLong)raw.size(), 9);
    z.resize(zl);
    png_chunk(f, "IDAT", z);
    png_chunk(f, "IEND", {});
    fclose(f);
    return true;
}
#endif
