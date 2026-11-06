/* Software page cache for the WHX game-data file (GPL-2.0, same terms as the engine it is linked with).
 * The LP-MSPM33C321A has no memory-mapped QSPI window (SDK 1.04.00.00), so the engine cannot read the 1.7 MB file through
 * pointers. Every engine pointer into the file keeps its value (TINY_WAD_ADDR + file offset); only the dereference changes:
 * FW_U8 / FW_U16 / FW_U32 / FW_COPY read through a cache of 256-byte pages filled by QSPI reads.
 * A pointer outside the file's address range is read directly, so code that sees both kinds of data needs no branches of its own. */
#ifndef FOX_WHX_H
#define FOX_WHX_H
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef FOX_WHX_CACHE
#define FOX_WHX_BASE   ((uint32_t)TINY_WAD_ADDR)
#define FOX_WHX_SIZE   0x001C0000u          /* upper bound of the file (1,800,312 bytes on the shareware conversion) */
#ifndef FOX_WHX_PAGE_SHIFT
#define FOX_WHX_PAGE_SHIFT 8               /* 256-byte pages; 7 and 6 were tried (README section 6) */
#endif
#define FOX_WHX_PAGE   (1u << FOX_WHX_PAGE_SHIFT)

extern uint32_t fox_whx_mru_page;           /* page number of the page the last access used */
extern const uint8_t *fox_whx_mru_ptr;      /* its place in the cache */
const uint8_t *fox_whx_page_slow(uint32_t page);
#ifdef FOX_WHX_STATS
extern uint32_t whx_st_fast;
#define FOX_WHX_COUNT_FAST() (whx_st_fast++)
#else
#define FOX_WHX_COUNT_FAST() ((void)0)
#endif

static inline const uint8_t *fox_whx_at(const void *p, uint32_t n)   /* pointer to n bytes (n <= 4, may not cross a page) in the cache, or p itself */
{
    uint32_t off = (uint32_t)(uintptr_t)p - FOX_WHX_BASE;
    if (off >= FOX_WHX_SIZE) return (const uint8_t *)p;
    uint32_t pg = off >> FOX_WHX_PAGE_SHIFT;
    if (pg == fox_whx_mru_page) { FOX_WHX_COUNT_FAST(); return fox_whx_mru_ptr + (off & (FOX_WHX_PAGE - 1)); }
    return fox_whx_page_slow(pg) + (off & (FOX_WHX_PAGE - 1));
}
static inline int fox_whx_in(const void *p) { return ((uint32_t)(uintptr_t)p - FOX_WHX_BASE) < FOX_WHX_SIZE; }
void fox_whx_copy(void *dst, const void *src, uint32_t n);          /* any length, any alignment */
uint32_t fox_whx_u16_x(const void *p);   /* page-crossing cases */
uint32_t fox_whx_u32_x(const void *p);

static inline uint8_t FW_U8(const void *p) { return *fox_whx_at(p, 1); }
static inline uint16_t FW_U16(const void *p)
{
    uint32_t off = (uint32_t)(uintptr_t)p - FOX_WHX_BASE;
    if (off < FOX_WHX_SIZE && (off & (FOX_WHX_PAGE - 1)) > FOX_WHX_PAGE - 2) return (uint16_t)fox_whx_u16_x(p);
    uint16_t v; memcpy(&v, fox_whx_at(p, 2), 2); return v;
}
static inline uint32_t FW_U32(const void *p)
{
    uint32_t off = (uint32_t)(uintptr_t)p - FOX_WHX_BASE;
    if (off < FOX_WHX_SIZE && (off & (FOX_WHX_PAGE - 1)) > FOX_WHX_PAGE - 4) return fox_whx_u32_x(p);
    uint32_t v; memcpy(&v, fox_whx_at(p, 4), 4); return v;
}
#define FW_S8(p)  ((int8_t)FW_U8(p))
#define FW_S16(p) ((int16_t)FW_U16(p))
#define FW_S32(p) ((int32_t)FW_U32(p))
#define FW_COPY(dst, src, n) fox_whx_copy((dst), (src), (n))
/* FW_VIEW(p, n, tmp): a pointer to n readable bytes at p; the bytes sit in the cache page when they do not cross a page boundary
 * (valid only until the next cache miss, so use it for a few reads and no calls), else they are copied to tmp. */
static inline const void *fox_whx_view(const void *p, uint32_t n, void *tmp)
{
    uint32_t off = (uint32_t)(uintptr_t)p - FOX_WHX_BASE;
    if (off >= FOX_WHX_SIZE) return p;
    if ((off & (FOX_WHX_PAGE - 1)) + n > FOX_WHX_PAGE) { fox_whx_copy(tmp, p, n); return tmp; }
    return fox_whx_at(p, n);
}
#define FW_VIEW(p, n, tmp) fox_whx_view((p), (n), (tmp))
#else
#define FW_U8(p)  (*(const uint8_t *)(p))
#define FW_U16(p) (*(const uint16_t *)(p))
#define FW_U32(p) (*(const uint32_t *)(p))
#define FW_S8(p)  (*(const int8_t *)(p))
#define FW_S16(p) (*(const int16_t *)(p))
#define FW_S32(p) (*(const int32_t *)(p))
#define FW_COPY(dst, src, n) memcpy((dst), (src), (n))
#define FW_VIEW(p, n, tmp) (p)
#endif
#ifdef __cplusplus
}
#endif
#endif
