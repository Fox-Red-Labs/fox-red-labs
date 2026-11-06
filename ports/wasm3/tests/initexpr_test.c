// A constant expression (global initialiser) that contains i32.load must be rejected when the module loads, not run.
#include <stdio.h>
#include "wasm3.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); static unsigned char buf[512]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    IM3Environment env = m3_NewEnvironment(); IM3Runtime rt = m3_NewRuntime(env, 64 * 1024, NULL);
    IM3Module m; M3Result r = m3_ParseModule(env, &m, buf, (uint32_t)n);
    if (!r) r = m3_LoadModule(rt, m);
    printf("result: %s\n", r ? r : "(accepted)");
    return r ? 0 : 1;
}
