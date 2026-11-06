// Ten passes of the app loop on the Mac with a fixed heap, AddressSanitizer on: heap_used must be identical every pass.
#include <stdio.h>
#include <stdint.h>
#include "../common/frl_hal.h"
#include "../common/frl_wasm.h"
void hal_init(void) {}
void hal_putc(int c) { (void)c; }
void hal_led(int on) { (void)on; }
void hal_delay_ms(uint32_t ms) { (void)ms; }
uint32_t hal_us(void) { static uint32_t t; return t += 7; }
uint32_t hal_cpu_hz(void) { return 0; }
int main(void) {
    uint32_t first = 0; int bad = 0;
    for (int i = 0; i < 10; i++) {
        int r = frl_wasm_main();
        printf("pass %d: status %d heap_used %u\n", i + 1, r, frl_result.heap_used);
        if (r) bad = 1;
        if (i == 0) first = frl_result.heap_used; else if (frl_result.heap_used != first) bad = 1;
    }
    puts(bad ? "FAIL: heap not flat" : "PASS: heap flat over 10 passes");
    return bad;
}
