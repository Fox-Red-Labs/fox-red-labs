/* Port of msp430fr243x_framwrite_persistent (MSP430FR2433) to the LP-MSPM33C321A. Same design as the LP-MSPM0L1306 and LP-AM13E230 ports:
 * the count lives in an append-only log of slots in one flash sector, newest valid slot wins, the sector is erased only when full.
 * The MSPM33 flash word is 128 bits (FLASHCTL_SYS_DATAWIDTH in mspm33c321x.h) and the sector is 2 kB (DL_FLASHCTL_SECTOR_SIZE), so a slot
 * is 16 bytes (count, inverted count, two blank words) and one erase covers 128 counts. The log sits in the last sector of main flash
 * (CPU address 0x100FF800, controller address 0x000FF800, bank 1); the linker is told the code ends below it and the program runs from
 * bank 0. LPM4.5 becomes a wait loop: sleep modes and a button wake are not part of this run (PORT.md). The burst timings are also kept in
 * gBurst, so they can be read over the probe when the console pins are covered by a BoosterPack.
 * Written fresh from the MSPM33 driver library. See PORT.md. */
#include <stdint.h>
#include "ti_msp_dl_config.h"

#define LOG_PUTC(c) DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) (c))
#define LOG_IDLE()  delay_cycles(64000)
#include "log.h"

#define DATA_CPU_ADDR  0x100FF800u   /* last sector of main flash, as the CPU reads it */
#define DATA_FLASH_ADDR 0x000FF800u  /* the same sector as the flash controller addresses it */
#ifndef SLOT_BYTES
#define SLOT_BYTES 16u               /* one 128-bit flash word per slot */
#endif
#define SLOTS     (DL_FLASHCTL_SECTOR_SIZE / SLOT_BYTES)
#ifndef COUNT_ANY_RESET
#define COUNT_ANY_RESET 1            /* 1: every reset except power-on counts, so a probe or software reset stands in for the button */
#endif
#ifndef ERASE_AT_BOOT
#define ERASE_AT_BOOT 0
#endif
#ifndef BURST
#define BURST 0
#endif
#ifndef RECAP
#define RECAP 1                      /* 1: repeat the log state 3 s after boot, so a serial port opened after a power-up still sees it */
#endif
#ifndef BLINK_AT
#define BLINK_AT 5
#endif
#ifndef STORM
#define STORM 0
#endif
#ifndef CPU_MHZ
#define CPU_MHZ 32u
#endif

typedef struct { uint32_t count; uint32_t inv; uint32_t pad[2]; } slot_t;
#define LOG_SLOT(i) ((const volatile slot_t *) (DATA_CPU_ADDR + SLOT_BYTES * (i)))

volatile uint32_t gCount, gUsed, gErases, gMagic;
volatile uint32_t gBurst[6];         /* n, prog_avg_us, prog_max_us, erase_us, erases, done magic: read over the probe */

static int flash_erase_sector(void)
{
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, DATA_FLASH_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);
    return DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, DATA_FLASH_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR) == DL_FLASHCTL_COMMAND_STATUS_FAILED ? -1 : 0;
}

static int flash_program_slot(uint32_t idx, uint32_t count)
{
    uint32_t w[4] = { count, ~count, 0xFFFFFFFFu, 0xFFFFFFFFu };
    uint32_t a = DATA_FLASH_ADDR + SLOT_BYTES * idx;
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, a, DL_FLASHCTL_REGION_SELECT_MAIN);
#if SLOT_BYTES == 16
    return DL_FlashCTL_programMemoryFromRAM128WithECCGenerated(FLASHCTL, a, w) == DL_FLASHCTL_COMMAND_STATUS_FAILED ? -1 : 0;
#else
    return DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, a, w) == DL_FLASHCTL_COMMAND_STATUS_FAILED ? -1 : 0;
#endif
}

static void log_scan(void)
{
    gCount = 0; gUsed = 0;
    for (uint32_t i = 0; i < SLOTS; i++) {
        uint32_t a = LOG_SLOT(i)->count, b = LOG_SLOT(i)->inv;
        if (a == 0xFFFFFFFFu && b == 0xFFFFFFFFu) { continue; }
        gUsed = i + 1;
        if (b == ~a) { gCount = a; }
    }
}

static int count_up(void)
{
    gCount++;
    if (gUsed >= SLOTS) { if (flash_erase_sector()) { return -1; } gUsed = 0; gErases++; }
    return flash_program_slot(gUsed++, gCount);
}

static uint32_t cyc(void) { return DWT->CYCCNT; }

static void burst(uint32_t n)
{
    uint32_t psum = 0, pmax = 0, emax = 0, e0 = gErases;
    for (uint32_t i = 0; i < n; i++) {
        if (gUsed >= SLOTS) {
            uint32_t t = cyc(); flash_erase_sector(); uint32_t e = (cyc() - t) / CPU_MHZ; gUsed = 0; gErases++;
            if (e > emax) { emax = e; }
        }
        gCount++;
        uint32_t t = cyc(); flash_program_slot(gUsed++, gCount); uint32_t p = (cyc() - t) / CPU_MHZ;
        psum += p; if (p > pmax) { pmax = p; }
    }
    gBurst[0] = n; gBurst[1] = psum / n; gBurst[2] = pmax; gBurst[3] = emax; gBurst[4] = gErases - e0; gBurst[5] = 0xB0B0B0B0u;
    log_s("A1 burst"); log_kv("n", n); log_kv("prog_avg_us", psum / n); log_kv("prog_max_us", pmax);
    log_kv("erase_us", emax); log_kv("erases", gErases - e0); log_nl();
}

int main(void)
{
    SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DL_GSC_acquireFlashSemaphore(GSC);
    DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();
    uint32_t cause_n = (uint32_t) cause;
#if ERASE_AT_BOOT
    flash_erase_sector();
#endif
    log_scan();
    int counted = 0, fault = 0;
    if (cause == DL_SYSCTL_RESET_CAUSE_BOR_WAKE_FROM_SHUTDOWN || (COUNT_ANY_RESET && cause != DL_SYSCTL_RESET_CAUSE_POR_HW_FAILURE && cause != DL_SYSCTL_RESET_CAUSE_POR_EXTERNAL_NRST)) {
        fault = count_up(); counted = 1;
    }
    log_s("A1 boot"); log_kv("cause", cause_n); log_kv("counted", (uint32_t) counted); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("fault", (uint32_t) fault); log_nl();
    if (BURST) { burst(BURST); log_s("A1 after_burst"); log_kv("count", gCount); log_nl(); }

    if (RECAP) { delay_cycles(3 * 32000000u); log_s("A1 recap"); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("erases", gErases); log_nl(); }

    if (STORM && gCount < STORM) { delay_cycles(3200000u); NVIC_SystemReset(); }

    gMagic = 0xA1C0DE02u;
    if (gCount >= BLINK_AT) {                /* the original blinks forever once the count reaches 5 */
        log_s("A1 blinking forever"); log_nl();
        for (;;) { DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_PASS_LED_PIN); delay_cycles(3200000u); }
    }
    log_s("A1 waiting in a loop (no sleep, no button)"); log_nl();
    for (;;) { }
}
