/*
 * RAM checksum routine for the browser bench (site/flash/mspm0-dap.js): CRC-32 (the zlib one: reflected, polynomial
 * 0xEDB88320, start and end value 0xFFFFFFFF) of r1 bytes of flash or memory at r0. The page writes this code to
 * SRAM, sets r0 and r1, runs the core here and reads r0 at the breakpoint. Thumb-1 code, so it runs on the
 * Cortex-M0+ and Cortex-M33 parts. Build: tools/webflash/loader/build_crc.sh
 */
#include <stdint.h>

__attribute__((noreturn, used, section(".text.entry"))) void entry(const uint8_t *p, uint32_t n)
{
    __asm volatile("cpsid i");
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    c = ~c;
    __asm volatile("mov r0, %0\n1: bkpt #0\n b 1b" : : "r"(c) : "r0");
    __builtin_unreachable();
}
