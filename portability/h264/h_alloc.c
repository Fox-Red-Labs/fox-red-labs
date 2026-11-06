/* Bump allocator for the decoder: no fragmentation, exact peak. h264bsd frees only at shutdown, so h_free is a no-op and h_reset() gives it all back.
   ARENA_BYTES comes from the build (-DARENA_BYTES=...). Optional second pool for boards whose RAM is in two pieces (-DARENA2_BYTES=..., placed in section .arena2
   by the linker file): allocations of up to ARENA2_MAX bytes go there first, the rest to the main pool, and the peak counts both. */
#include <stddef.h>
#include <stdint.h>
#ifndef ARENA_BYTES
#error "define ARENA_BYTES"
#endif
size_t h_cur, h_peak, h_nallocs;
static uint8_t h_arena[ARENA_BYTES] __attribute__((aligned(16)));
#ifdef ARENA2_BYTES
#ifndef ARENA2_MAX
#define ARENA2_MAX 30000
#endif
static uint8_t h_arena2[ARENA2_BYTES] __attribute__((aligned(16), section(".arena2")));
static size_t h_cur2;
#endif
void *h_malloc(size_t n)
{
	size_t a = (n + 15u) & ~(size_t)15u;
#ifdef ARENA2_BYTES
	if (a <= ARENA2_MAX && h_cur2 + a <= ARENA2_BYTES) { void *p = h_arena2 + h_cur2; h_cur2 += a; h_nallocs++; h_cur += a; if (h_cur > h_peak) h_peak = h_cur; return p; }
	if (h_cur - h_cur2 + a > ARENA_BYTES) return 0;
	{ void *p = h_arena + (h_cur - h_cur2); h_cur += a; h_nallocs++; if (h_cur > h_peak) h_peak = h_cur; return p; }
#else
	if (h_cur + a > ARENA_BYTES) return 0;
	{ void *p = h_arena + h_cur; h_cur += a; h_nallocs++; if (h_cur > h_peak) h_peak = h_cur; return p; }
#endif
}
void h_free(void *q) { (void)q; }
void h_reset(void)
{
	h_cur = h_peak = h_nallocs = 0;
#ifdef ARENA2_BYTES
	h_cur2 = 0;
#endif
}
