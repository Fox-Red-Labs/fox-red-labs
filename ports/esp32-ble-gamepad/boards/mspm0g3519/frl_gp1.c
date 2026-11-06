/* Board glue: LP-MSPM0G3519, UART0 (PA10 TX) to the XDS110 backchannel at 115200, SysTick cycle counter.
 * Everything gamepad-related is in ../frl_gamepad.c. */
#include "ti_msp_dl_config.h"
#include "frl_gamepad.h"
#include <stdio.h>
#include <string.h>

#define CPU_HZ (CPUCLK_FREQ)
#define STACK_SIZE 256u

extern unsigned long __STACK_END; /* top of the stack, end of SRAM bank 0 */
/* .data is linked right after .bss; the last .data item marks the end of static RAM (within 4 bytes) */
static volatile uint32_t ram_end_marker = 1;

static volatile uint32_t wraps;
void SysTick_Handler(void) { wraps++; }

static void timer_start(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0xFFFFFFu;
    SysTick->VAL = 0;
    wraps = 0;
    NVIC_SetPriority(SysTick_IRQn, 0);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

static uint64_t cycles(void)
{
    uint32_t hi, lo;
    do { hi = wraps; lo = SysTick->VAL; } while (hi != wraps);
    return ((uint64_t)hi << 24) + (0xFFFFFFu - lo);
}

static void uart_write(const char *s)
{
    while (*s) DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)*s++);
}

static void hex(char *dst, const uint8_t *b, size_t n)
{
    static const char d[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { *dst++ = d[b[i] >> 4]; *dst++ = d[b[i] & 15]; }
    *dst = 0;
}

static uint8_t map[FRL_GP_MAX_DESC];
static size_t map_len;
static uint8_t rpt[4][32];
static size_t rpt_len[4];
static char line[2 * FRL_GP_MAX_DESC + 48];
static uint32_t desc_us, pack_us;
static volatile uint8_t sink;

int main(void)
{
    frl_gp_config_t cfg;
    frl_gp_t g;

    SYSCFG_DL_init();
    timer_start();

    frl_gp_default_config(&cfg);
    frl_gp_init(&g, &cfg);
    map_len = frl_gp_build_descriptor(&g, map, sizeof(map));

    rpt_len[0] = frl_gp_pack(&g, rpt[0]);

    frl_gp_press(&g, BUTTON_5); frl_gp_press(&g, BUTTON_16); frl_gp_pressStart(&g);
    frl_gp_setAxes(&g, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);
    frl_gp_setHat1(&g, HAT_DOWN_RIGHT);
    rpt_len[1] = frl_gp_pack(&g, rpt[1]);

    frl_gp_release(&g, BUTTON_5); frl_gp_releaseStart(&g); frl_gp_setHat1(&g, HAT_CENTERED);
    frl_gp_setAxes(&g, 0, 0, 0, 0, 0, 0, 0, 0);
    rpt_len[2] = frl_gp_pack(&g, rpt[2]);

    frl_gp_press(&g, BUTTON_1); frl_gp_press(&g, BUTTON_8); frl_gp_press(&g, BUTTON_9); frl_gp_pressSelect(&g);
    frl_gp_setAxes(&g, -32768, -1, 1, 255, 256, -256, 12345, -12345);
    frl_gp_setHat1(&g, HAT_UP_LEFT);
    rpt_len[3] = frl_gp_pack(&g, rpt[3]);

    /* Timing: 200 descriptor builds and 1000 packs, averaged, in microseconds. */
    uint64_t t0 = cycles();
    for (int i = 0; i < 200; i++) { sink = (uint8_t)frl_gp_build_descriptor(&g, map, sizeof(map)); }
    uint64_t t1 = cycles();
    uint8_t scratch[32];
    for (int i = 0; i < 1000; i++) { sink = (uint8_t)frl_gp_pack(&g, scratch); }
    uint64_t t2 = cycles();
    desc_us = (uint32_t)(((t1 - t0) * 1000000ull) / ((uint64_t)CPU_HZ * 200u));
    pack_us = (uint32_t)(((t2 - t1) * 1000000ull) / ((uint64_t)CPU_HZ * 1000u));
    map_len = frl_gp_build_descriptor(&g, map, sizeof(map));

    uint32_t heap_free = (uint32_t)(uintptr_t)&__STACK_END - STACK_SIZE - ((uint32_t)(uintptr_t)&ram_end_marker + 4u);

    while (1) {
        snprintf(line, sizeof(line), "FRL-GP1 begin\r\nINFO heap_free=%lu uptime_ms=%lu\r\nTIME desc_us=%lu pack_us=%lu\r\n",
                 (unsigned long)heap_free, (unsigned long)(cycles() / (CPU_HZ / 1000u)),
                 (unsigned long)desc_us, (unsigned long)pack_us);
        uart_write(line);
        int n = snprintf(line, sizeof(line), "MAP len=%u ", (unsigned)map_len);
        hex(line + n, map, map_len);
        uart_write(line); uart_write("\r\n");
        for (int i = 0; i < 4; i++) {
            n = snprintf(line, sizeof(line), "RPT%d len=%u ", i + 1, (unsigned)rpt_len[i]);
            hex(line + n, rpt[i], rpt_len[i]);
            uart_write(line); uart_write("\r\n");
        }
        uart_write("FRL-GP1 end\r\n");
        uint64_t until = cycles() + 3ull * CPU_HZ;
        while (cycles() < until) { }
    }
}
