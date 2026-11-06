// memory.grow must not wrap. 1-byte pages, 0xC0000000 pages, growing by 0x7FFFFFFF: the 32-bit sum wraps to 0x3FFFFFFF.
// Needs -Dd_m3MaxLinearMemoryPages=0 so the 2 GB size cap is off, and a runtime memoryLimit so no real 3 GB is allocated.
// Pass: grow returns -1 and the size is unchanged.
#include <stdio.h>
#include "wasm3.h"
#include "m3_env.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); static unsigned char buf[512]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    IM3Environment env = m3_NewEnvironment(); IM3Runtime rt = m3_NewRuntime(env, 64 * 1024, NULL);
    rt->memoryLimit = 65536;
    IM3Module m; M3Result r = m3_ParseModule(env, &m, buf, (uint32_t)n); if (r) { printf("parse: %s\n", r); return 2; }
    r = m3_LoadModule(rt, m); if (r) { printf("load: %s\n", r); return 2; }
    IM3Function g, s; m3_FindFunction(&g, rt, "grow"); m3_FindFunction(&s, rt, "size");
    uint32_t before = 0, after = 0; int32_t gr = 0;
    m3_CallV(s, 0); m3_GetResultsV(s, &before);
    r = m3_CallV(g, 0x7FFFFFFF); if (!r) m3_GetResultsV(g, &gr);
    m3_CallV(s, 0); m3_GetResultsV(s, &after);
    printf("size before 0x%x, grow(0x7FFFFFFF) -> %d (%s), size after 0x%x\n", before, gr, r ? r : "ok", after);
    return (gr == -1 && after == before) ? 0 : 1;
}
