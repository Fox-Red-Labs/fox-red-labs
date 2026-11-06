/* Platform layer for LP-MSPM33C321A (GPL-2.0, see ../LICENSE).
 * Stubs for this wave: no screen, no keys, WAD reads from a test hook.
 * What is left is listed in ../README.md (QSPI WAD read, panel, joystick). */
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "doomgeneric.h"

#ifdef FOX_EMU
/* Emulator build: a read-only timer register and a console port that the host harness provides. */
#define EMU_TIMER_MS (*(volatile uint32_t *)0x50000000u)
#define EMU_CONSOLE  (*(volatile uint32_t *)0x50000004u)
#define EMU_WADBASE  0x60000000u   /* the WAD, mapped by the harness, copied from by fox_wad_read below */
static uint32_t ticks_ms(void) { return EMU_TIMER_MS; }
static void console_putc(char c) { EMU_CONSOLE = (uint32_t)(uint8_t)c; }
#else
#include "ti_msp_dl_config.h"
static volatile uint32_t ms_count;
void SysTick_Handler(void) { ms_count++; }
static uint32_t ticks_ms(void) { return ms_count; }
static void console_putc(char c) { (void)c; }
#endif

/* ---- log hook: vsnprintf into a 128-byte stack buffer, then out one character at a time ---- */
int vsnprintf(char *, size_t, const char *, va_list);
int fox_vprintf(const char *fmt, va_list ap)
{
    char buf[128];
    int n = vsnprintf(buf, sizeof buf, fmt, ap), i;
    for (i = 0; i < n && i < (int)sizeof buf - 1; i++) console_putc(buf[i]);
    return n;
}
int fox_printf(const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt); n = fox_vprintf(fmt, ap); va_end(ap);
    return n;
}

/* ---- WAD read: replaced by the QSPI read command (see README, "What is left") ---- */
size_t fox_wad_read(unsigned int offset, void *buffer, size_t len)
{
#ifdef FOX_EMU
    memcpy(buffer, (const void *)(EMU_WADBASE + offset), len);
    return len;
#else
    (void)offset; memset(buffer, 0, len);
    return 0;   /* no WAD yet: the engine stops at startup with a clear message */
#endif
}

int system(const char *c) { (void)c; return -1; }   /* i_system.c error box */

unsigned long fox_frames;
void DG_Init(void) {}
void DG_DrawFrame(void) {}
void DG_DrawFrame8(const uint8_t *idx, const uint8_t *pal) { (void)idx; (void)pal; fox_frames++; }
void DG_SleepMs(uint32_t ms) { uint32_t t = ticks_ms(); while ((uint32_t)(ticks_ms() - t) < ms) {} }
uint32_t DG_GetTicksMs(void) { return ticks_ms(); }
int DG_GetKey(int *pressed, unsigned char *key) { (void)pressed; (void)key; return 0; }
void DG_SetWindowTitle(const char *title) { (void)title; }
