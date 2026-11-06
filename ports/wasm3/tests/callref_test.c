// Host test: a module that recurses forever through call_ref must trap with "stack overflow", not crash.
// Build with -Dd_m3HasTypedRefs=1 (call_ref is off in wasm3 by default) and a small -Dd_m3MaxNativeStack.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wasm3.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); if (!f) { puts("no module"); return 2; }
    static unsigned char buf[4096]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    IM3Environment env = m3_NewEnvironment();
    IM3Runtime rt = m3_NewRuntime(env, 4 * 1024 * 1024, NULL);   // a big Wasm value stack so the native stack is the one that runs out
    IM3Module m; M3Result r = m3_ParseModule(env, &m, buf, (uint32_t)n); if (r) { printf("parse: %s\n", r); return 2; }
    r = m3_LoadModule(rt, m); if (r) { printf("load: %s\n", r); return 2; }
    IM3Function fn; r = m3_FindFunction(&fn, rt, "rec"); if (r) { printf("find: %s\n", r); return 2; }
    r = m3_CallV(fn, 0);
    printf("result: %s\n", r ? r : "(none)");
    return (r && strstr(r, "stack overflow")) ? 0 : 1;
}
