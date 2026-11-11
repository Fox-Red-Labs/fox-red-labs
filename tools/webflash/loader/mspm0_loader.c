/*
 * RAM flash loader for MSPM0 parts, run by the browser bench through the debugger (site/flash/mspm0-dap.js).
 * The page writes this code and a block of data into SRAM, sets r0 to the flash address, r1 to the data in SRAM,
 * r2 to the byte count (a multiple of 8) and r3 to the CMDBYTEN value for 64-bit programming, starts the core
 * here and waits for the breakpoint. r0 comes back 0 when every word programmed, or the failing address.
 * The same register sequence as TI DriverLib's DL_FlashCTL_programMemoryFromRAM64 (MSPM0 SDK 2.11.00.07):
 * clear status, unprotect, program one 64-bit word, wait for done. Protection registers are cleared in full
 * (main flash only); NONMAIN's register is never touched. Build: tools/webflash/loader/build.sh
 */
#include <stdint.h>

#define FC            0x400CD000u
#define REG(o)        (*(volatile uint32_t *) (FC + (o)))
#define CMDEXEC       0x1100u
#define CMDTYPE       0x1104u
#define CMDADDR       0x1120u
#define CMDBYTEN      0x1124u
#define CMDDATA0      0x1130u
#define CMDDATA1      0x1134u
#define CMDWEPROTA    0x11D0u
#define CMDWEPROTB    0x11D4u
#define STATCMD       0x13D0u

static inline __attribute__((always_inline)) uint32_t program(uint32_t dst, const uint32_t *src, uint32_t n, uint32_t byten)
{
    for (uint32_t o = 0; o < n; o += 8, src += 2) {
        REG(CMDTYPE) = 0x05u;                                 /* clear status */
        REG(CMDEXEC) = 1u;
        while (REG(STATCMD) & 0x4u) {}                        /* command in progress */
        REG(CMDWEPROTA) = 0u;
        REG(CMDWEPROTB) = 0u;
        REG(CMDTYPE) = 0x01u;                                 /* program, one word */
        REG(CMDBYTEN) = byten;
        REG(CMDADDR) = dst + o;
        REG(CMDDATA0) = src[0];
        REG(CMDDATA1) = src[1];
        REG(CMDEXEC) = 1u;
        uint32_t s;
        do { s = REG(STATCMD); } while (!(s & 0x1u));          /* done */
        if (!(s & 0x2u)) return dst + o;                       /* not passed */
    }
    return 0u;
}

/* The page sets r0 to r3 and starts the core here; r0 holds the result when the core stops at the breakpoint. */
__attribute__((noreturn, used, section(".text.entry"))) void entry(uint32_t dst, const uint32_t *src, uint32_t n, uint32_t byten)
{
    __asm volatile("cpsid i");
    uint32_t r = program(dst, src, n, byten);
    __asm volatile("mov r0, %0\n1: bkpt #0\n b 1b" : : "r"(r) : "r0");
    __builtin_unreachable();
}
