// Loads the embedded WebAssembly app into wasm3 and runs it: hello and blink, then two timed loops.
// Written for this repository. Calls wasm3's public API (wasm3.h) only.
#include <stddef.h>
#include "wasm3.h"
#include "m3_env.h"
#include "frl_hal.h"
#include "frl_wasm.h"
#include "../app/app_wasm.h"

#ifndef FRL_WASM_STACK
#define FRL_WASM_STACK 4096     // bytes of Wasm value stack
#endif
#ifndef FRL_FIB_N
#define FRL_FIB_N 20
#endif
#ifndef FRL_BENCH_N
#define FRL_BENCH_N 20000
#endif
#ifndef FRL_BLINKS
#define FRL_BLINKS 3
#endif
#ifndef FRL_RESULT_ATTR
#define FRL_RESULT_ATTR
#endif

volatile struct frl_result frl_result FRL_RESULT_ATTR;
unsigned frl_m3_heap_used(void);
unsigned frl_m3_heap_size(void);
void frl_m3_heap_reset(void);
volatile uint32_t frl_pass;                       // passes completed, counted by frl_wasm_main
static IM3Environment g_env;
static IM3Runtime g_rt;

// Free everything wasm3 allocated, then start the fixed heap over, so every pass sees the same free heap.
static void cleanup(void) {
    if (g_rt) m3_FreeRuntime(g_rt);
    if (g_env) m3_FreeEnvironment(g_env);
    g_rt = 0; g_env = 0;
    frl_m3_heap_reset();
}

static void puts_(const char *s) { while (*s) hal_putc(*s++); }
static void putu(uint32_t v) {
    char b[11]; int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) hal_putc(b[--n]);
}
static void puti(int32_t v) { if (v < 0) { hal_putc('-'); putu((uint32_t)(-v)); } else putu((uint32_t)v); }
static void kv(const char *k, uint32_t v) { puts_(k); hal_putc('='); putu(v); hal_putc('\n'); }

m3ApiRawFunction(h_putc)      { m3ApiGetArg(int32_t, c); hal_putc(c); m3ApiSuccess(); }
m3ApiRawFunction(h_led)       { m3ApiGetArg(int32_t, on); hal_led(on); m3ApiSuccess(); }
m3ApiRawFunction(h_delay_ms)  { m3ApiGetArg(int32_t, ms); hal_delay_ms((uint32_t)ms); m3ApiSuccess(); }
m3ApiRawFunction(h_print_i32) { m3ApiGetArg(int32_t, v); puti(v); hal_putc('\n'); m3ApiSuccess(); }

static int fail(int step, const char *what, const char *err) {
    cleanup();
    puts_("FAIL step "); putu((uint32_t)step); puts_(": "); puts_(what);
    if (err) { puts_(": "); puts_(err); }
    hal_putc('\n');
    frl_result.status = step;
    frl_result.magic = 0x4D335746u;
    return step;
}

// Call an exported function that takes one i32 and returns one i32; time it in microseconds.
static M3Result call1(IM3Runtime rt, const char *name, uint32_t arg, uint32_t *out, uint32_t *us) {
    IM3Function f;
    M3Result r = m3_FindFunction(&f, rt, name);
    if (r) return r;
    uint32_t t0 = hal_us();
    r = m3_CallV(f, arg);
    uint32_t t1 = hal_us();
    if (r) return r;
    int32_t v = 0;
    r = m3_GetResultsV(f, &v);
    *out = (uint32_t)v;
    *us = t1 - t0;
    return r;
}

static int frl_wasm_pass(void) {
    frl_result.version = 1;
    frl_result.cpu_hz = hal_cpu_hz();
    frl_result.wasm_bytes = app_wasm_len;
    frl_result.fib_n = FRL_FIB_N;
    frl_result.bench_n = FRL_BENCH_N;
    puts_("wasm3 on "); kv("cpu_hz", frl_result.cpu_hz);
    kv("wasm_bytes", app_wasm_len);

    uint32_t t0 = hal_us();
    IM3Environment env = g_env = m3_NewEnvironment();
    if (!env) return fail(1, "environment", "out of heap");
    IM3Runtime rt = g_rt = m3_NewRuntime(env, FRL_WASM_STACK, NULL);
    if (!rt) return fail(1, "runtime", "out of heap");
    IM3Module mod;
    M3Result r = m3_ParseModule(env, &mod, app_wasm, app_wasm_len);
    if (r) return fail(1, "parse", r);
    r = m3_LoadModule(rt, mod);
    if (r) return fail(1, "load", r);
    r = m3_LinkRawFunction(mod, "env", "putc", "v(i)", h_putc);           if (r) return fail(2, "link putc", r);
    r = m3_LinkRawFunction(mod, "env", "led", "v(i)", h_led);             if (r) return fail(2, "link led", r);
    r = m3_LinkRawFunction(mod, "env", "delay_ms", "v(i)", h_delay_ms);   if (r) return fail(2, "link delay_ms", r);
    r = m3_LinkRawFunction(mod, "env", "print_i32", "v(i)", h_print_i32); if (r) return fail(2, "link print_i32", r);
    frl_result.load_us = hal_us() - t0;

    // hello and blink (the delays are 4 x 200 ms per blink; subtract them from the time)
    uint32_t blinks = 0, us = 0;
    r = call1(rt, "run", FRL_BLINKS, &blinks, &us);
    if (r) return fail(3, "run", r);
    frl_result.blinks = blinks;
    frl_result.first_call_us = us - 400000u * FRL_BLINKS;
    if (blinks != FRL_BLINKS) return fail(3, "run returned the wrong count", 0);

    uint32_t v, wus, nus, nv;
    r = call1(rt, "fib", FRL_FIB_N, &v, &wus);
    if (r) return fail(4, "fib", r);
    uint32_t t = hal_us(); nv = frl_native_fib(FRL_FIB_N); nus = hal_us() - t;
    frl_result.fib_val = v; frl_result.fib_wasm_us = wus; frl_result.fib_native_us = nus;
    if (v != nv) return fail(4, "fib differs from native C", 0);

    r = call1(rt, "bench", FRL_BENCH_N, &v, &wus);
    if (r) return fail(5, "bench", r);
    t = hal_us(); nv = frl_native_bench(FRL_BENCH_N); nus = hal_us() - t;
    frl_result.bench_val = v; frl_result.bench_wasm_us = wus; frl_result.bench_native_us = nus;
    if (v != nv) return fail(5, "bench differs from native C", 0);

    frl_result.heap_used = frl_m3_heap_used();
    frl_result.heap_size = frl_m3_heap_size();
    frl_result.status = 0;
    frl_result.magic = 0x4D335746u;

    kv("load_us", frl_result.load_us);
    kv("run_overhead_us", frl_result.first_call_us);
    kv("fib_n", FRL_FIB_N); kv("fib_val", frl_result.fib_val);
    kv("fib_wasm_us", frl_result.fib_wasm_us); kv("fib_native_us", frl_result.fib_native_us);
    kv("bench_n", FRL_BENCH_N); kv("bench_val", frl_result.bench_val);
    kv("bench_wasm_us", frl_result.bench_wasm_us); kv("bench_native_us", frl_result.bench_native_us);
    kv("heap_used", frl_result.heap_used); kv("heap_size", frl_result.heap_size);
    kv("pass", frl_pass + 1);
    puts_("PASS\n");
    return 0;
}

int frl_wasm_main(void) {
    int r = frl_wasm_pass();
    cleanup();
    frl_pass++;
    return r;
}
