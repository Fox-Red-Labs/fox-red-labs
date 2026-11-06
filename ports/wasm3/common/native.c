// The same two algorithms as app.wat, in C. A mismatch between these and the Wasm results is a failed check.
#include "frl_wasm.h"
uint32_t frl_native_fib(uint32_t n) {
    int32_t m = (int32_t)n;
    return m < 2 ? (uint32_t)m : frl_native_fib(n - 1) + frl_native_fib(n - 2);
}
uint32_t frl_native_bench(uint32_t n) {
    uint32_t acc = 0x1234, x = 1;
    for (uint32_t i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        uint32_t t = acc ^ x;
        acc = ((t << 5) | (t >> 27)) + (x >> 3);
        acc += x & i;
    }
    return acc;
}
