#pragma once
#include <stdint.h>
// Results block. Boards keep it at a fixed RAM address so a debugger can read it without a serial port.
struct frl_result {
    uint32_t magic;        // 0x4D335746 ("FW3M") once the run has finished
    uint32_t version;      // layout version
    int32_t  status;       // 0 = all checks passed, otherwise the step that failed (1 load, 2 link, 3 run, 4 fib, 5 bench)
    uint32_t cpu_hz;
    uint32_t blinks;       // value returned by run()
    uint32_t load_us;      // parse and link
    uint32_t first_call_us;// run() including its 200 ms delays, minus the delays
    uint32_t fib_n, fib_val, fib_wasm_us, fib_native_us;
    uint32_t bench_n, bench_val, bench_wasm_us, bench_native_us;
    uint32_t heap_used;    // bytes of the wasm3 fixed heap in use at the end
    uint32_t heap_size;
    uint32_t wasm_bytes;   // size of the .wasm file
};
extern volatile struct frl_result frl_result;
int frl_wasm_main(void);   // runs everything, prints a report over hal_putc, returns status
uint32_t frl_native_fib(uint32_t n);
uint32_t frl_native_bench(uint32_t n);
