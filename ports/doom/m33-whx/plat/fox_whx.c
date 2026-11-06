/* Software page cache for the WHX file: 256-byte pages, 2 ways, least recently used, filled by QSPI reads. GPL-2.0.
 * Emulator build: the QSPI is a device model at 0x50001000 (address, length, destination, go). On the board the same call
 * becomes the SDK QSPI read path (quad I/O read, DMA into the page); that part is written in the E27 plan, not run. */
#include <stdint.h>
#include <string.h>
#include "fox_whx.h"

#ifdef FOX_WHX_CACHE
#ifndef FOX_WHX_KB
#define FOX_WHX_KB 16
#endif
#define WAYS 2
#define SETS ((FOX_WHX_KB * 1024) / (FOX_WHX_PAGE * WAYS))
#define FILE_BYTES 1800312u                 /* last page is short: do not read past the end of the file */

static uint8_t pages[SETS][WAYS][FOX_WHX_PAGE] __attribute__((aligned(4)));
static uint16_t tags[SETS][WAYS];           /* page number + 1, 0 = empty */
static uint8_t lru[SETS];                   /* way to replace next */
uint32_t fox_whx_mru_page = 0xFFFFFFFFu;
const uint8_t *fox_whx_mru_ptr;
uint32_t whx_st_fast, whx_st_set, whx_st_miss, whx_st_slow, whx_st_bytes;

#ifdef FOX_BOARD
void fox_board_qspi_read(uint32_t off, void *dst, uint32_t n);   /* plat/board.c: the SDK read path, quad I/O read at 40 MHz */
#define qspi_read fox_board_qspi_read
#else
#define QSPI_ADDR (*(volatile uint32_t *)0x50001000u)
#define QSPI_LEN  (*(volatile uint32_t *)0x50001004u)
#define QSPI_DST  (*(volatile uint32_t *)0x50001008u)
#define QSPI_GO   (*(volatile uint32_t *)0x5000100Cu)
static void qspi_read(uint32_t off, void *dst, uint32_t n) { QSPI_ADDR = off; QSPI_LEN = n; QSPI_DST = (uint32_t)(uintptr_t)dst; QSPI_GO = 1; }
#endif

const uint8_t *fox_whx_page_slow(uint32_t pg)
{
    uint32_t s = pg % SETS; uint16_t t = (uint16_t)(pg + 1);   /* SETS is a build constant: a mask when it is a power of two */
#ifdef FOX_WHX_STATS
    whx_st_slow++;
#endif
    for (int w = 0; w < WAYS; w++) if (tags[s][w] == t) {
#ifdef FOX_WHX_STATS
        whx_st_set++;
#endif
        lru[s] = (uint8_t)(w ^ 1); fox_whx_mru_page = pg; fox_whx_mru_ptr = pages[s][w]; return pages[s][w];
    }
    int v = lru[s]; uint32_t base = pg << FOX_WHX_PAGE_SHIFT, n = FILE_BYTES - base; if (n > FOX_WHX_PAGE) n = FOX_WHX_PAGE;
    qspi_read(base, pages[s][v], n);
#ifdef FOX_WHX_STATS
    whx_st_miss++; whx_st_bytes += n;
#endif
    tags[s][v] = t; lru[s] = (uint8_t)(v ^ 1);
    fox_whx_mru_page = pg; fox_whx_mru_ptr = pages[s][v]; return pages[s][v];
}

uint32_t fox_whx_u16_x(const void *p)
{
    uint32_t a = (uint32_t)(uintptr_t)p; uint32_t lo = *fox_whx_at((const void *)a, 1); uint32_t hi = *fox_whx_at((const void *)(a + 1), 1); return lo | (hi << 8);
}
uint32_t fox_whx_u32_x(const void *p)
{
    uint32_t a = (uint32_t)(uintptr_t)p, v = 0;
    for (int i = 3; i >= 0; i--) v = (v << 8) | *fox_whx_at((const void *)(a + i), 1);
    return v;
}
void fox_whx_copy(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst; uint32_t a = (uint32_t)(uintptr_t)src;
    if (!fox_whx_in(src)) { memcpy(dst, src, n); return; }
    while (n) {
        uint32_t room = FOX_WHX_PAGE - (a & (FOX_WHX_PAGE - 1)), k = n < room ? n : room;
        memcpy(d, fox_whx_at((const void *)a, 1), k); d += k; a += k; n -= k;
    }
}
#endif
