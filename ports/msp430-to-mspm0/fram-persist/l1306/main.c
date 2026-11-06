/* Port of msp430fr243x_framwrite_persistent (MSP430FR2433) to the LP-MSPM0L1306.
 * Original: a variable marked #pragma PERSISTENT lives in FRAM; each button wake from LPM4.5 adds one to it with plain C
 * (FRAM write-protect opened for the one statement); at 5 or more the LED blinks forever. MSPM0 has no FRAM, so the count
 * lives in an append-only log in one 1 kB flash sector: 128 slots of 8 bytes, each written once (the flash word and its ECC
 * allow one program), newest valid slot wins, the sector is erased only when full. That makes one erase per 128 counts.
 * LPM4.5 becomes STANDBY0 (default build, button interrupt) or SHUTDOWN (-DSLEEP_SHUTDOWN=1, button wakes the chip with a reset).
 * Written fresh from the MSPM0 DriverLib. See PORT.md. Build switches: COUNT_ANY_RESET, ERASE_AT_BOOT, BURST, BLINK_AT, STORM, SLEEP_SHUTDOWN. */
#include "ti_msp_dl_config.h"

#define LOG_PUTC(c) DL_UART_Main_transmitDataBlocking(UART_0_INST, (c))
#define LOG_IDLE()  while (DL_UART_Main_isBusy(UART_0_INST)) { }
#include "log.h"

#define DATA_ADDR 0x0000F800u       /* the sector build_l1306.sh keeps out of the linker's FLASH region */
#define SLOTS     128u
#ifndef COUNT_ANY_RESET
#define COUNT_ANY_RESET 1           /* 1: every reset except power-on counts, so a probe reset stands in for the button (no hands on the bench) */
#endif
#ifndef ERASE_AT_BOOT
#define ERASE_AT_BOOT 0
#endif
#ifndef BURST
#define BURST 0                     /* >0: add this many counts at boot and report flash timing */
#endif
#ifndef RECAP
#define RECAP 1                     /* 1: repeat the log state 3 s after boot, so a serial port opened after a power-up still sees it */
#endif
#ifndef BLINK_AT
#define BLINK_AT 5
#endif
#ifndef STORM
#define STORM 0                     /* >0: reset the chip from software until the count reaches this value (a reset test with no probe in the loop) */
#endif
#ifndef SLEEP_SHUTDOWN
#define SLEEP_SHUTDOWN 0
#endif

typedef struct { uint32_t count; uint32_t inv; } slot_t;
#define LOG_SLOT(i) ((const volatile slot_t *) (DATA_ADDR + 8u * (i)))

static uint32_t gCount, gUsed, gErases;
static volatile uint8_t gButton;

static int flash_erase_sector(void)
{
    __disable_irq();
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, DATA_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);
    int ok = (DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, DATA_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR) == DL_FLASHCTL_COMMAND_STATUS_PASSED);
    __enable_irq();
    return ok ? 0 : -1;
}

static int flash_program_slot(uint32_t idx, uint32_t count)
{
    uint32_t w[2] = { count, ~count };
    uint32_t addr = DATA_ADDR + 8u * idx;
    __disable_irq();
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, addr, DL_FLASHCTL_REGION_SELECT_MAIN);
    int ok = (DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, addr, w) == DL_FLASHCTL_COMMAND_STATUS_PASSED);
    __enable_irq();
    return ok ? 0 : -1;
}

/* Scan the log: gCount = newest valid count, gUsed = index after the last slot that is not blank. */
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

static uint32_t ticks_us(uint32_t t0) { return ((t0 - SysTick->VAL) & 0x00FFFFFFu) / 32u; }   /* SysTick at the 32 MHz CPU clock */

static void burst(uint32_t n)
{
    uint32_t psum = 0, pmax = 0, emax = 0, e0 = gErases;
    for (uint32_t i = 0; i < n; i++) {
        if (gUsed >= SLOTS) {
            uint32_t t = SysTick->VAL; flash_erase_sector(); uint32_t e = ticks_us(t); gUsed = 0; gErases++;
            if (e > emax) { emax = e; }
        }
        gCount++;
        uint32_t t = SysTick->VAL; flash_program_slot(gUsed++, gCount); uint32_t p = ticks_us(t);
        psum += p; if (p > pmax) { pmax = p; }
    }
    log_s("A1 burst"); log_kv("n", n); log_kv("prog_avg_us", psum / n); log_kv("prog_max_us", pmax);
    log_kv("erase_us", emax); log_kv("erases", gErases - e0); log_nl();
}

void GROUP1_IRQHandler(void)
{
    if (DL_GPIO_getEnabledInterruptStatus(GPIO_SWITCH_PORT, GPIO_SWITCH_S2_PIN)) {
        DL_GPIO_clearInterruptStatus(GPIO_SWITCH_PORT, GPIO_SWITCH_S2_PIN);
        gButton = 1;
    }
}

int main(void)
{
    SYSCFG_DL_init();
    SysTick->LOAD = 0x00FFFFFFu; SysTick->VAL = 0; SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();
    uint32_t cause_n = (uint32_t) cause;
#if SLEEP_SHUTDOWN
    if (cause == DL_SYSCTL_RESET_CAUSE_BOR_WAKE_FROM_SHUTDOWN) { SYSCFG_DL_GPIO_init(); DL_SYSCTL_releaseShutdownIO(); }
#endif
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

    if (RECAP) { delay_cycles(96000000);                 /* 3 s */ log_s("A1 recap"); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("erases", gErases); log_nl(); }

    if (STORM && gCount < STORM) { delay_cycles(3200000); NVIC_SystemReset(); }

    if (gCount >= BLINK_AT) {                /* the original blinks forever once the count reaches 5 */
        log_s("A1 blinking forever"); log_nl();
        for (;;) { DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN); delay_cycles(3200000); }
    }
#if SLEEP_SHUTDOWN
    DL_GPIO_initDigitalInputFeatures(GPIO_SWITCH_S2_IOMUX, DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_ON_0);
    log_s("A1 entering SHUTDOWN, press S2 to wake"); log_nl();
    delay_cycles(160000000);                 /* 5 s awake first, so a debugger can attach after every reset */
    DL_SYSCTL_setPowerPolicySHUTDOWN();
    for (;;) { __WFI(); }
#else
    NVIC_EnableIRQ(GPIO_SWITCH_INT_IRQN);
    DL_SYSCTL_setPowerPolicySTANDBY0();
    log_s("A1 sleeping policy=STANDBY0, press S2 to count"); log_nl();
    for (;;) {
        __WFI();
        if (gButton) { gButton = 0; fault = count_up(); log_s("A1 button"); log_kv("count", gCount); log_kv("fault", (uint32_t) fault); log_nl(); }
    }
#endif
}
