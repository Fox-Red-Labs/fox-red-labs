/* FreeRTOS side of the LP-EM-CC35X1 target cc35x1-whx (GPL-2.0, see ../LICENSE): serial console at 921,600 baud on the
 * probe's port, millisecond tick, and the thread that runs the engine. Nothing from the engine's headers is used here. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"

extern const uint8_t fox_whx[];
void fox_doom_main(void);
int fox_printf(const char *fmt, ...);

static UART2_Handle uart;
void hal_write(const char *s, int n) { size_t w; if (uart && n > 0) UART2_write(uart, s, (size_t)n, &w); }
uint32_t hal_ms(void) { return (uint32_t)xTaskGetTickCount(); }
#ifdef FOX_PROF
/* cycle clock: FreeRTOS tick count times the SysTick reload, plus how far SysTick has counted down (DWT CYCCNT reads 0 on this chip) */
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(cmsis/device.h)
uint64_t hal_cyc(void)
{
    uint32_t t1, t2, v, l = SysTick->LOAD + 1;
    do { t1 = xTaskGetTickCount(); v = SysTick->VAL; t2 = xTaskGetTickCount(); } while (t1 != t2);
    return (uint64_t)t1 * l + (l - 1 - v);
}
#endif
void hal_park(void) { vTaskDelay(pdMS_TO_TICKS(1000)); }

#ifdef FOX_PROF
#define R32(a) (*(volatile uint32_t *)(a))
static uint32_t rdsum(const volatile uint32_t *p, int n) { uint32_t s = 0; for (int i = 0; i < n; i++) s += p[i]; return s; }
static void micro(const char *nm, const volatile uint32_t *p, int n)
{
    uint64_t t; uint32_t s;
    t = hal_cyc(); s = rdsum(p, n); t = hal_cyc() - t;
    fox_printf("MICRO %s first pass %lu cyc per 100 words (sum %08lx)\n", nm, (unsigned long)(t * 100 / n), (unsigned long)s);
    t = hal_cyc(); s = rdsum(p, n); t = hal_cyc() - t;
    fox_printf("MICRO %s second pass %lu cyc per 100 words\n", nm, (unsigned long)(t * 100 / n));
}
static uint32_t ldm_sum(const volatile uint32_t *p, int n)
{   /* n words, 8 per LDM */
    uint32_t s = 0; const uint32_t *q = (const uint32_t *)p;
    for (int i = 0; i < n; i += 8) {
        uint32_t a, b, c, d, e, f, g, h2;
        __asm volatile("ldm %8, {%0,%1,%2,%3,%4,%5,%6,%7}" : "=&r"(a), "=&r"(b), "=&r"(c), "=&r"(d), "=&r"(e), "=&r"(f), "=&r"(g), "=&r"(h2) : "r"(q) : "memory");
        s += a + b + c + d + e + f + g + h2; q += 8;
    }
    return s;
}
static void microm(const char *nm, const volatile uint32_t *p, int n)
{
    uint64_t t; uint32_t s;
    t = hal_cyc(); s = ldm_sum(p, n); t = hal_cyc() - t;
    fox_printf("MICRO %s LDM8 %lu cyc per 100 words (sum %08lx)\n", nm, (unsigned long)(t * 100 / n), (unsigned long)s);
}
static uint32_t dram_buf[4096] __attribute__((section(".bss.fox_arena_d")));
static void regs(void)
{
    fox_printf("REG icache ctrl %08lx status %08lx lo %08lx hi %08lx\n", (unsigned long)R32(0x41902004), (unsigned long)R32(0x41902008), (unsigned long)R32(0x41902010), (unsigned long)R32(0x41902018));
    fox_printf("REG dcache ctrl %08lx sts %08lx cal %08lx cah %08lx\n", (unsigned long)R32(0x41902404), (unsigned long)R32(0x41902408), (unsigned long)R32(0x41902410), (unsigned long)R32(0x41902418));
    fox_printf("REG ospi config %08lx rdinstr %08lx delay %08lx capture %08lx devsize %08lx phycfg %08lx phymaster %08lx\n", (unsigned long)R32(0x41910000), (unsigned long)R32(0x41910004), (unsigned long)R32(0x4191000C), (unsigned long)R32(0x41910010), (unsigned long)R32(0x41910014), (unsigned long)R32(0x419100B4), (unsigned long)R32(0x419100B8));
    fox_printf("REG ospi remap %08lx modebit %08lx\n", (unsigned long)R32(0x41910024), (unsigned long)R32(0x41910028));
}
/* latency probes: 64 dependent-free loads/stores unrolled so loop overhead is small */
#define U8(x) x x x x x x x x
static uint32_t __attribute__((noinline)) ld64(volatile uint32_t *p, int n)
{ uint32_t s = 0; for (int i = 0; i < n; i++) { U8(U8(s += p[0]; )) } return s; }
static void __attribute__((noinline)) st64(volatile uint32_t *p, int n)
{ for (int i = 0; i < n; i++) { U8(U8(p[0] = i; )) } }
static uint32_t __attribute__((noinline)) alu64(int n)
{ uint32_t s = 1; for (int i = 0; i < n; i++) { U8(U8(s = s * 3 + 1; )) } return s; }
static void lat(const char *nm, volatile uint32_t *p)
{
    uint64_t t; int n = 200;
    t = hal_cyc(); ld64(p, n); t = hal_cyc() - t; fox_printf("LAT %s load  %lu cyc/100 loads\n", nm, (unsigned long)(t * 100 / (n * 64)));
    t = hal_cyc(); st64(p, n); t = hal_cyc() - t; fox_printf("LAT %s store %lu cyc/100 stores\n", nm, (unsigned long)(t * 100 / (n * 64)));
}
static void micros(void)
{
    { uint64_t t = hal_cyc(); alu64(200); t = hal_cyc() - t; fox_printf("LAT alu mul+add %lu cyc/100 ops\n", (unsigned long)(t * 100 / (200 * 64))); }
    lat("dram", dram_buf); lat("tcm", (volatile uint32_t *)0x20010000); lat("cram", (volatile uint32_t *)0x00004000);

    micro("flash whx 16k words", (const volatile uint32_t *)(fox_whx + 0x10000), 16384);
    micro("flash whx 16k words again region", (const volatile uint32_t *)(fox_whx + 0x10000), 16384);
    micro("flash whx 64k words", (const volatile uint32_t *)(fox_whx + 0x40000), 65536);
    micro("dram 4k words", dram_buf, 4096);
    microm("flash whx 16k words", (const volatile uint32_t *)(fox_whx + 0x10000), 16384);
    microm("flash whx 16k words", (const volatile uint32_t *)(fox_whx + 0x10000), 16384);
    { uint64_t t = hal_cyc(); memcpy(dram_buf, fox_whx + 0x20000, 16384); t = hal_cyc() - t; fox_printf("MICRO memcpy 16 kB flash->dram %lu cyc per 100 words\n", (unsigned long)(t * 100 / 4096)); }
    micro("tcm 4k words", (const volatile uint32_t *)0x20000000, 4096);
}
#endif
#ifdef FOX_PROF
static uint32_t *stk_lo, *stk_hi;
static void stack_paint(void)
{
    volatile uint32_t here; uint32_t *top = (uint32_t *)((uintptr_t)&here & ~3u) - 64;
    uint32_t *p = top - (15000 / 4);
    stk_hi = top; stk_lo = p;
    for (; p < top; p++) *p = 0xA5A5A5A5u;
}
uint32_t hal_stack_used(void)
{
    uint32_t *p = stk_lo;
    while (p < stk_hi && *p == 0xA5A5A5A5u) p++;
    return (uint32_t)((uintptr_t)stk_hi - (uintptr_t)p) + 256;   /* bytes used below the start-up frame, plus the 256 bytes skipped */
}
#endif
void *mainThread(void *arg0)
{
    UART2_Params p;
    (void)arg0;
    UART2_Params_init(&p); p.baudRate = 921600; p.readMode = UART2_Mode_BLOCKING; p.writeMode = UART2_Mode_BLOCKING;
    uart = UART2_open(CONFIG_UART2_0, &p);
    vTaskDelay(pdMS_TO_TICKS(1500));   /* time for the host to open the port after a reset */
    fox_printf("\nDOOMWHX cc35x1 build: whx at %p magic %c%c%c%c, free heap %u\n", (const void *)fox_whx,
               fox_whx[0], fox_whx[1], fox_whx[2], fox_whx[3], (unsigned)xPortGetFreeHeapSize());
    fox_printf("RUN attract-mode demos clock 160 MHz tick %lu Hz\n", (unsigned long)configTICK_RATE_HZ);
#ifdef FOX_PROF
#ifndef FOX_NOMICRO
    stack_paint(); regs(); micros();
#endif
#endif
    fox_doom_main();
    for (;;) vTaskDelay(1000);
}
