/* Static arena behind SpeexDSP's allocator (speex_cfg/os_support_custom.h). Allocations are 8-byte aligned and zeroed;
 * nothing is freed, so fe_arena_used after set-up is the front end's heap high-water mark. */
#include <stdint.h>
#include <string.h>
#include "fe_run.h"
#ifndef FE_ARENA_BYTES
#define FE_ARENA_BYTES (160 * 1024)
#endif
static uint64_t arena[FE_ARENA_BYTES / 8];
volatile unsigned fe_arena_used, fe_arena_fail;
volatile unsigned fe_speex_msgs, fe_speex_fatal;
const unsigned fe_arena_size = FE_ARENA_BYTES;
void *fe_arena_alloc(int size)
{
	unsigned s = ((unsigned)size + 7u) & ~7u;
	if (size < 0 || fe_arena_used + s > FE_ARENA_BYTES) { fe_arena_fail++; return 0; }
	void *p = (char *)arena + fe_arena_used;
	fe_arena_used += s;
	memset(p, 0, s);
	return p;
}
