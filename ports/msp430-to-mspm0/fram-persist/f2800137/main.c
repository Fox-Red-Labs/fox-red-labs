/* Port of msp430fr243x_framwrite_persistent (MSP430FR2433) to the LAUNCHXL-F2800137 (C2000). Same design as the AM13 and L1306 ports: the
 * count lives in an append-only log in one flash sector, newest valid slot wins, the sector is erased only when full.
 * C28x specifics, each checked in C2000Ware 26.02: a char, short and int are 16 bits and flash addresses count 16-bit words; the sector is
 * 1,024 words (2 kB); the Flash API (FAPI) programs 64 bits at a time (4 words, 64-bit aligned, once per erase) and computes the ECC itself
 * (Fapi_AutoEccGeneration); the chip has one flash bank, so every erase and program function runs from RAM (.TI.ramfunc) with interrupts off.
 * The log sector is 100 (0x99000): not in the shipped demo, not in this image, and every other group of 8 sectors is write-erase protected
 * through CMDWEPROTA/B before each command. No OTP, DCSM or security zone is touched. LPM4.5 becomes a wait loop; there is no button wake here.
 * Written fresh from the Flash API example and header. See PORT.md. */
#include "driverlib.h"
#include "device.h"
#include "FlashTech_F280013x_C28x.h"

#define LOG_PUTC(c) SCI_writeCharBlockingNonFIFO(SCIA_BASE, (uint16_t) (c))
#define LOG_IDLE()  DEVICE_DELAY_US(1000)
#include "log.h"

#define FLASH_WRAPPER_PROGRAM_BASE 0x51000U           /* FLASH0CMD_BASE - 0x1000, from flash_programming_f280013x.h */
#define LOG_SECTOR 100u
#define DATA_ADDR  (0x80000UL + (uint32_t) LOG_SECTOR * 0x400UL)
#define SECTOR_WORDS 1024u
#define SLOT_WORDS 4u               /* 64 bits: count (2 words), inverted count (2 words); the FAPI program unit */
#define SLOTS      (SECTOR_WORDS / SLOT_WORDS)
#ifndef COUNT_ANY_RESET
#define COUNT_ANY_RESET 1           /* 1: every reset except power-on counts, so a probe or software reset stands in for the button */
#endif
#ifndef ERASE_AT_BOOT
#define ERASE_AT_BOOT 0
#endif
#ifndef BURST
#define BURST 0
#endif
#ifndef RECAP
#define RECAP 1
#endif
#ifndef BLINK_AT
#define BLINK_AT 5
#endif
#ifndef STORM
#define STORM 0
#endif
#define CPU_MHZ 120u

static uint32_t gCount, gUsed, gErases;

typedef struct { uint32_t count; uint32_t inv; } slot_t;
#define LOG_SLOT(i) ((const volatile slot_t *) (DATA_ADDR + (uint32_t) SLOT_WORDS * (i)))

/* Write-erase protection: a set bit protects. CMDWEPROTA holds sectors 0 to 31 one bit each; CMDWEPROTB bit n holds sectors 32+8n to 39+8n.
 * Sector 100 is in group (100-32)/8 = 8, so everything except B bit 8 stays protected. */
#define PROT_A 0xFFFFFFFFUL
#define PROT_B (0x00000FFFUL & ~(1UL << 8))

#pragma CODE_SECTION(fapi_prepare, ".TI.ramfunc");
static int fapi_prepare(void)
{
    Fapi_StatusType r;
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) { }
    if (Fapi_getFsmStatus() != 0) {
        r = Fapi_issueAsyncCommand(Fapi_ClearStatus);
        while (Fapi_getFsmStatus() != 0) { }
        if (r != Fapi_Status_Success) { return 3; }
    }
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, PROT_A);
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, PROT_B);
    return 0;
}

#pragma CODE_SECTION(flash_api_init, ".TI.ramfunc");
static int flash_api_init(void)
{
    if (Fapi_initializeAPI((Fapi_FmcRegistersType *) FLASH_WRAPPER_PROGRAM_BASE, DEVICE_SYSCLK_FREQ / 1000000U) != Fapi_Status_Success) { return 1; }
    if (Fapi_setActiveFlashBank(Fapi_FlashBank0) != Fapi_Status_Success) { return 2; }
    return 0;
}

#pragma CODE_SECTION(flash_erase_sector, ".TI.ramfunc");
static int flash_erase_sector(void)
{
    Fapi_FlashStatusWordType sw;
    int p = fapi_prepare();
    if (p) { return p; }
    if (Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32_t *) DATA_ADDR) != Fapi_Status_Success) { return 4; }
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) { }
    if (Fapi_getFsmStatus() != 3) { return 5; }
    if (Fapi_doBlankCheck((uint32_t *) DATA_ADDR, SECTOR_WORDS / 2u, &sw) != Fapi_Status_Success) { return 6; }
    return 0;
}

#pragma CODE_SECTION(flash_program_slot, ".TI.ramfunc");
static int flash_program_slot(uint32_t idx, uint32_t count)
{
    Fapi_FlashStatusWordType sw;
    uint16_t w[4];
    uint32_t a = DATA_ADDR + (uint32_t) SLOT_WORDS * idx;
    int p;
    w[0] = (uint16_t) (count & 0xFFFFu); w[1] = (uint16_t) (count >> 16);
    w[2] = (uint16_t) (~count & 0xFFFFu); w[3] = (uint16_t) ((~count) >> 16);
    p = fapi_prepare();
    if (p) { return p; }
    if (Fapi_issueProgrammingCommand((uint32_t *) a, w, SLOT_WORDS, 0, 0, Fapi_AutoEccGeneration) != Fapi_Status_Success) { return 7; }
    while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy) { }
    if (Fapi_getFsmStatus() != 3) { return 8; }
    if (Fapi_doVerify((uint32_t *) a, 2, (uint32_t *) w, &sw) != Fapi_Status_Success) { return 9; }
    return 0;
}

static void log_scan(void)
{
    gCount = 0; gUsed = 0;
    for (uint32_t i = 0; i < SLOTS; i++) {
        uint32_t a = LOG_SLOT(i)->count, b = LOG_SLOT(i)->inv;
        if (a == 0xFFFFFFFFUL && b == 0xFFFFFFFFUL) { continue; }
        gUsed = i + 1;
        if (b == ~a) { gCount = a; }
    }
}

static int count_up(void)
{
    int r;
    gCount++;
    DINT;
    if (gUsed >= SLOTS) { r = flash_erase_sector(); if (r) { EINT; return r; } gUsed = 0; gErases++; }
    r = flash_program_slot(gUsed++, gCount);
    EINT;
    return r;
}

static uint32_t tick(void) { return CPUTimer_getTimerCount(CPUTIMER0_BASE); }
static uint32_t us_since(uint32_t t0) { return (t0 - tick()) / CPU_MHZ; }      /* the timer counts down at the CPU clock */

static void burst(uint32_t n)
{
    uint32_t psum = 0, pmax = 0, emax = 0, e0 = gErases;
    for (uint32_t i = 0; i < n; i++) {
        DINT;
        if (gUsed >= SLOTS) {
            uint32_t t = tick(); flash_erase_sector(); uint32_t e = us_since(t); gUsed = 0; gErases++;
            if (e > emax) { emax = e; }
        }
        gCount++;
        uint32_t t = tick(); flash_program_slot(gUsed++, gCount); uint32_t p = us_since(t);
        EINT;
        psum += p; if (p > pmax) { pmax = p; }
    }
    log_s("A1 burst"); log_kv("n", n); log_kv("prog_avg_us", psum / n); log_kv("prog_max_us", pmax);
    log_kv("erase_us", emax); log_kv("erases", gErases - e0); log_nl();
}

static void scia_init(void)
{
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCIRXDA); GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD); GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCITXDA); GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD); GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);
    SCI_performSoftwareReset(SCIA_BASE);
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, 115200, (SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE | SCI_CONFIG_PAR_NONE));
    SCI_resetChannels(SCIA_BASE);
    SCI_disableInterrupt(SCIA_BASE, SCI_INT_RXRDY_BRKDT | SCI_INT_TXRDY);
    SCI_enableModule(SCIA_BASE);
    SCI_performSoftwareReset(SCIA_BASE);
}

void main(void)
{
    Device_init();                      /* clock, flash wait states and ECC, copies .TI.ramfunc to RAM */
    Device_initGPIO();
    scia_init();
    CPUTimer_stopTimer(CPUTIMER0_BASE); CPUTimer_setPeriod(CPUTIMER0_BASE, 0xFFFFFFFFUL); CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE); CPUTimer_startTimer(CPUTIMER0_BASE);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD); GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    uint32_t cause = SysCtl_getResetCause();
    SysCtl_clearResetCause(cause);
    int fault = flash_api_init();
    int counted = 0;
#if ERASE_AT_BOOT
    DINT; fault = fault ? fault : flash_erase_sector(); EINT;
#endif
    log_scan();
    if (!fault && (COUNT_ANY_RESET ? !(cause & SYSCTL_CAUSE_POR) : 0)) { fault = count_up(); counted = 1; }
    log_s("A1 boot"); log_kv("cause", cause); log_kv("counted", (uint32_t) counted); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("fault", (uint32_t) fault); log_nl();
    if (BURST) {                        /* check the timer against the delay loop first: 100 ms of SysCtl_delay should read about 100000 us */
        uint32_t t = tick(); DEVICE_DELAY_US(100000UL); log_s("A1 timer_check"); log_kv("delay_100000us_reads", us_since(t)); log_nl();
        burst(BURST); log_s("A1 after_burst"); log_kv("count", gCount); log_nl(); }

    if (RECAP) { DEVICE_DELAY_US(3000000UL); log_s("A1 recap"); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("erases", gErases); log_nl(); }

    if (STORM && gCount < STORM) { DEVICE_DELAY_US(100000UL); SysCtl_resetDevice(); }

    if (gCount >= BLINK_AT) {           /* the original blinks forever once the count reaches 5 */
        log_s("A1 blinking forever"); log_nl();
        for (;;) { GPIO_togglePin(DEVICE_GPIO_PIN_LED1); DEVICE_DELAY_US(100000UL); }
    }
    log_s("A1 waiting in a loop (no sleep, no button wake on this port)"); log_nl();
    for (;;) { }
}
