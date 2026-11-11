/*
 * RAM routine for the browser bench: CRC-32 (zlib: reflected, 0xEDB88320, start and end 0xFFFFFFFF) of each sector of a
 * flash range, so the page can compare a whole flash with a saved copy by reading back one word per sector. Parameter
 * block: start address, sector count, sector size, output array (one word per sector; a 1 kB table follows it).
 * r0 = pointer to the block. Thumb-1 code. Build: tools/webflash/loader/build_m33.sh
 */
#include <stdint.h>
typedef struct { uint32_t addr, count, size, out; } P;

__attribute__((noreturn, used, section(".text.entry"))) void entry(P *p)
{
    __asm volatile("cpsid i");
    uint32_t *out = (uint32_t *) p->out, *tbl = out + p->count;
    for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); tbl[i] = c; }
    const uint8_t *q = (const uint8_t *) p->addr;
    for (uint32_t i = 0; i < p->count; i++) {
        uint32_t c = 0xFFFFFFFFu;
        for (uint32_t n = p->size; n; n--) c = tbl[(c ^ *q++) & 0xFF] ^ (c >> 8);
        out[i] = ~c;
    }
    __asm volatile("1: bkpt #0\n b 1b");
    __builtin_unreachable();
}
