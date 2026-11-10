/* Port of msp430fr243x_framwrite_persistent (MSP430FR2433) to the LP-AM13E230. Same design as the LP-MSPM0L1306 port
 * (fram-persist/l1306/main.c): the count lives in an append-only log of 8-byte slots in one flash sector, newest valid slot wins, the
 * sector is erased only when full. The AM13 sector is 2 kB, so one erase covers 256 counts. The log sits in the last sector of flash
 * bank 1; the program lives in bank 0, because a flash operation must never touch the bank the program runs from, and a program load
 * that erases only bank 0 leaves the log alone. LPM4.5 becomes a wait loop (default, SLEEP_MODE 0), STANDBY0 (1) or SHUTDOWN (2); 1 and 2 are build-only here, because on
 * this board a debugger cannot connect to a chip in STANDBY0 (PORT.md).
 * Written fresh from the AM13 driver library. See PORT.md. */
#include "device.h"
#include "dl_flash.h"
#include "ti_sdk_dl_config.h"

#define LOG_PUTC(c) DL_UART_transmitDataBlocking(APP_UART_0_INST, (c))
#define LOG_IDLE()  DEVICE_DELAY_MS(2)
#include "log.h"

#define DATA_ADDR (0x00040000u + 0x40000u - DL_FLASH_SECTOR_SIZE)     /* last sector of bank 1 */
#define SLOT_BYTES 16u              /* DL_Flash_program rejects an address that is not 16-byte aligned (DL_FLASH_ADDR_INVALID_ALIGN), though dl_flash.h says 8 */
#define SLOTS     (DL_FLASH_SECTOR_SIZE / SLOT_BYTES)
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
#define RECAP 1                     /* 1: repeat the log state 3 s after boot, so a serial port opened after a power-up still sees it */
#endif
#ifndef BLINK_AT
#define BLINK_AT 5
#endif
#ifndef STORM
#define STORM 0
#endif
#ifndef SLEEP_MODE
#define SLEEP_MODE 0                /* 0: wait in a loop (the board stays debuggable), 1: STANDBY0, 2: SHUTDOWN. See PORT.md before using 1 or 2 */
#endif
#ifndef CPU_MHZ
#define CPU_MHZ 200u                /* SysTick runs at the CPU clock */
#endif

typedef struct { uint32_t count; uint32_t inv; uint32_t pad[2]; } slot_t;
#define LOG_SLOT(i) ((const volatile slot_t *) (DATA_ADDR + SLOT_BYTES * (i)))

static uint32_t gCount, gUsed, gErases;
static volatile uint8_t gButton;

static int flash_erase_sector(void) { return DL_Flash_eraseSector(DATA_ADDR) == DL_FLASH_SUCCESS ? 0 : -1; }

static int flash_program_slot(uint32_t idx, uint32_t count)
{
    uint32_t w[4] = { count, ~count, 0xFFFFFFFFu, 0xFFFFFFFFu };
    return DL_Flash_program(DATA_ADDR + SLOT_BYTES * idx, (uint8_t *) w, SLOT_BYTES) == DL_FLASH_SUCCESS ? 0 : -1;
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

static uint32_t ticks_us(uint32_t t0) { return ((t0 - SysTick->VAL) & 0x00FFFFFFu) / CPU_MHZ; }

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

void GPIO_PORT1_INT_Handler(void)
{
    if (DL_GPIO_getEnabledInterruptStatus(GPIO_SWITCH_PORT, GPIO_SWITCH_WAKE_PIN)) {
        DL_GPIO_clearInterruptStatus(GPIO_SWITCH_PORT, GPIO_SWITCH_WAKE_PIN);
        gButton = 1;
    }
}

int main(void)
{
    Device_Init();
    SYSCFG_DL_init();
    SysTick->LOAD = 0x00FFFFFFu; SysTick->VAL = 0; SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();
    uint32_t cause_n = (uint32_t) cause;
#if ERASE_AT_BOOT
    flash_erase_sector();
#endif
    log_scan();
    int counted = 0, fault = 0;
    if (cause == DL_SYSCTL_RESET_CAUSE_BORWAKESHUTDN || (COUNT_ANY_RESET && cause != DL_SYSCTL_RESET_CAUSE_PORHWFAIL && cause != DL_SYSCTL_RESET_CAUSE_POREXNRST)) {
        fault = count_up(); counted = 1;
    }
    log_s("A1 boot"); log_kv("cause", cause_n); log_kv("counted", (uint32_t) counted); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("fault", (uint32_t) fault); log_nl();
    if (BURST) { burst(BURST); log_s("A1 after_burst"); log_kv("count", gCount); log_nl(); }

    if (RECAP) { DEVICE_DELAY_MS(3000); log_s("A1 recap"); log_kv("count", gCount); log_kv("slot", gUsed ? gUsed - 1 : 0); log_kv("erases", gErases); log_nl(); }

    if (STORM && gCount < STORM) { DEVICE_DELAY_MS(100); NVIC_SystemReset(); }

    if (gCount >= BLINK_AT) {                /* the original blinks forever once the count reaches 5 */
        log_s("A1 blinking forever"); log_nl();
        for (;;) { DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN); DEVICE_DELAY_MS(100); }
    }
#if SLEEP_MODE == 2
    log_s("A1 entering SHUTDOWN, press the wake button"); log_nl();
    DEVICE_DELAY_MS(5000);                   /* awake first, so a debugger can attach after every reset */
    DL_SYSCTL_setPowerPolicySHUTDOWN();
    for (;;) { __WFI(); }
#else
#if SLEEP_MODE == 1
    DL_SYSCTL_setPowerPolicySTANDBY0();
    log_s("A1 sleeping policy=STANDBY0, press the wake button to count"); log_nl();
#else
    log_s("A1 waiting in a loop (no sleep), press the wake button to count"); log_nl();
#endif
    for (;;) {
#if SLEEP_MODE == 1
        __WFI();
#endif
        if (gButton) { gButton = 0; fault = count_up(); log_s("A1 button"); log_kv("count", gCount); log_kv("fault", (uint32_t) fault); log_nl(); }
    }
#endif
}
