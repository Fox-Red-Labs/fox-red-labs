/*
 * RAM flash loader for the Cortex-M33 parts (LP-MSPM33C321A, LP-AM13E230), run by the browser bench through the debugger
 * (site/flash/mspm0-dap.js). The page writes this code, a parameter block and a buffer into SRAM, starts the core at the
 * first instruction and waits for the breakpoint. Parameter block (word 0 first): destination flash address, source
 * address in SRAM, byte count (a multiple of 16), CMDBYTEN, flash controller base, result (0, or the failing address).
 * Same register sequence as the page's debug-side programWord (NVMNW, hw_flashctl.h / hw_nvmnw.h): clear status,
 * unprotect main flash, program one 16-byte word, wait for done. The flash semaphore stays with the debugger (the
 * core is secure and privileged at reset, the attributes the semaphore matches on). NONMAIN is never touched: the page
 * passes only main-flash addresses. Thumb-1 code. Build: tools/webflash/loader/build_m33.sh
 */
#include <stdint.h>
typedef struct { uint32_t dst, src, n, byten, base, res; } P;
#define R(o) (*(volatile uint32_t *) (b + (o)))

__attribute__((noreturn, used, section(".text.entry"))) void entry(P *p)
{
    __asm volatile("cpsid i");
    uint32_t b = p->base, d = p->dst, res = 0;
    const uint32_t *s = (const uint32_t *) p->src;
    for (uint32_t o = 0; o < p->n; o += 16, s += 4) {
        uint32_t a = d + o;
        R(0x1104) = 0x05u; R(0x1100) = 1u;
        while (R(0x13D0) & 0x4u) {}
        R(0x11D0) = 0u; R(0x11D4) = 0u;
        R(0x1104) = 0x01u; R(0x1124) = p->byten; R(0x1120) = a; R(0x112C) = (a >> 4) & 3u;
        R(0x1130) = s[0]; R(0x1134) = s[1]; R(0x1138) = s[2]; R(0x113C) = s[3];
        R(0x1100) = 1u;
        uint32_t st;
        do { st = R(0x13D0); } while (!(st & 0x1u));
        if (!(st & 0x2u)) { res = a; break; }
    }
    p->res = res;
    __asm volatile("mov r0, %0\n1: bkpt #0\n b 1b" : : "r"(res) : "r0");
    __builtin_unreachable();
}
