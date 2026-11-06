/* Platform layer for the LP-EM-CC35X1 (CC3551E, Cortex-M33, FreeRTOS), GPL-2.0, see ../LICENSE.
 * Ticks: FreeRTOS tick (1 ms). Sleep: vTaskDelay. Frame sink: CRC-32 chain over the first 400 frames, a downscaled
 * frame (80 x 50, 3-3-2 colour) out of the probe's serial port every 50th frame up to frame 400, no work after that
 * (so the frame rate measured after frame 400 has nothing but the engine in it). Keys: none, the engine plays a built-in demo.
 * The WAD is mapped flash (execute in place region of the external flash): lumps are used where they are. */
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include "doomgeneric.h"
#include "sha1.h"

extern const unsigned char fox_wad[];       /* DOOM1.WAD, included unchanged by wad_blob.S */
extern const unsigned int fox_wad_len;
#define CRC_FRAMES 400
#define SEND_EVERY 50

static UART2_Handle uart;
static uint32_t crc_tab[256], crc;
static unsigned long frames, t_first, t_400;
static const char *demo_name = "demo1";

static void uart_write(const void *b, size_t n) { size_t w; if (uart) UART2_write(uart, b, n, &w); }
int vsnprintf(char *, size_t, const char *, va_list);
int fox_vprintf(const char *fmt, va_list ap)
{
    char buf[160];
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    if (n > (int)sizeof buf - 1) n = sizeof buf - 1;
    if (n > 0) { /* the UART wants CRLF */
        char o[200]; int i, k = 0;
        for (i = 0; i < n; i++) { if (buf[i] == '\n') o[k++] = '\r'; o[k++] = buf[i]; }
        uart_write(o, k);
    }
    return n;
}
int fox_printf(const char *fmt, ...) { va_list ap; int n; va_start(ap, fmt); n = fox_vprintf(fmt, ap); va_end(ap); return n; }

/* WAD glue: same job as ../target/w_file_mem.c, with the mapped address a linker symbol */
#include "w_file.h"
extern wad_file_class_t fox_wad_file;
static wad_file_t the_wad;
static wad_file_t *W_Mem_OpenFile(char *path)
{
    if (strcmp(path, "doom1.wad") != 0) return NULL;
    the_wad.file_class = &fox_wad_file; the_wad.mapped = (byte *)fox_wad; the_wad.length = fox_wad_len;
    return &the_wad;
}
static void W_Mem_CloseFile(wad_file_t *w) { (void)w; }
static size_t W_Mem_Read(wad_file_t *w, unsigned int off, void *buf, size_t len)
{
    (void)w;
    if (off >= fox_wad_len) return 0;
    if (off + len > fox_wad_len) len = fox_wad_len - off;
    memcpy(buf, fox_wad + off, len);
    return len;
}
wad_file_class_t fox_wad_file = { W_Mem_OpenFile, W_Mem_CloseFile, W_Mem_Read };
size_t fox_wad_read(unsigned int off, void *buf, size_t len) { return W_Mem_Read(0, off, buf, len); }

int system(const char *c) { (void)c; return -1; }

static void crc_init(void)
{
    uint32_t i, k, c;
    for (i = 0; i < 256; i++) { c = i; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); crc_tab[i] = c; }
    crc = 0;
}
static uint32_t crc_update(uint32_t c, const uint8_t *p, size_t n)
{
    c = ~c;
    while (n--) c = crc_tab[(c ^ *p++) & 0xff] ^ (c >> 8);
    return ~c;
}

static void send_small(const uint8_t *idx, const uint8_t *pal)
{
    static uint8_t small[80 * 50];
    char hdr[48]; int x, y, sx, sy, h;
    for (y = 0; y < 50; y++)
        for (x = 0; x < 80; x++) {
            int r = 0, g = 0, b = 0;
            for (sy = 0; sy < 4; sy++) for (sx = 0; sx < 4; sx++) {
                const uint8_t *p = &pal[idx[(y * 4 + sy) * 320 + x * 4 + sx] * 3];
                r += p[0]; g += p[1]; b += p[2];
            }
            small[y * 80 + x] = (uint8_t)(((r >> 4) & 0xe0) | (((g >> 4) >> 3) & 0x1c) | ((b >> 4) >> 6));
        }
    h = snprintf(hdr, sizeof hdr, "\r\nFRM %lu 80 50\r\n", frames);
    uart_write(hdr, h); uart_write(small, sizeof small); uart_write("\r\nEND\r\n", 7);
}

void DG_DrawFrame8(const uint8_t *idx, const uint8_t *pal)
{
    if (frames == 0) t_first = xTaskGetTickCount();
    if (frames < CRC_FRAMES) {
        crc = crc_update(crc, idx, 64000);
        crc = crc_update(crc, pal, 768);
        if (frames % SEND_EVERY == 0) send_small(idx, pal);
        if ((frames + 1) % 100 == 0) fox_printf("CRC %lu %08lx\n", frames + 1, (unsigned long)crc);
    }
    frames++;
    if (frames == CRC_FRAMES) t_400 = xTaskGetTickCount();
}
void DG_DrawFrame(void) {}
void DG_Init(void) {}
void DG_SleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
uint32_t DG_GetTicksMs(void) { return xTaskGetTickCount(); }
int DG_GetKey(int *pressed, unsigned char *key) { (void)pressed; (void)key; return 0; }
void DG_SetWindowTitle(const char *t) { (void)t; }

static void report(void)
{
    unsigned long now = xTaskGetTickCount();
    fox_printf("EXIT demo=%s frames=%lu total_ms=%lu\n", demo_name, frames, now - t_first);
    if (frames > CRC_FRAMES)
        fox_printf("FPS after400 frames=%lu ms=%lu x1000=%lu\n", frames - CRC_FRAMES, now - t_400, (frames - CRC_FRAMES) * 1000000ul / (now - t_400));
    fox_printf("DONE\n");
}

void *mainThread(void *arg0)
{
    UART2_Params p; uint8_t c = 0; size_t got = 0; int i;
    sha1_context_t ctx; sha1_digest_t dg; unsigned long t0;
    static char name[] = "demoN";
    static char *argv[] = { "doom", "-iwad", "doom1.wad", "-nosound", "-nomusic", "-timedemo", name, NULL };
    (void)arg0;
    UART2_Params_init(&p); p.baudRate = 921600; p.readMode = UART2_Mode_BLOCKING; p.writeMode = UART2_Mode_BLOCKING;
    uart = UART2_open(CONFIG_UART2_0, &p);
    crc_init();
    fox_printf("\nDOOM cc35x1 build: wad at %p len %u, free heap %u\n", (const void *)fox_wad, fox_wad_len, (unsigned)xPortGetFreeHeapSize());
    t0 = xTaskGetTickCount();
    SHA1_Init(&ctx); SHA1_Update(&ctx, (byte *)fox_wad, fox_wad_len); SHA1_Final(dg, &ctx);
    fox_printf("WADSHA1 "); for (i = 0; i < 20; i++) fox_printf("%02x", dg[i]); fox_printf(" in %lu ms\n", xTaskGetTickCount() - t0);
#ifdef FOX_DEMO
    name[4] = '0' + FOX_DEMO;                 /* fixed at build time (DEMO=n ./build.sh) */
#else
    fox_printf("PICK demo 1, 2 or 3 within 4 s (default 1)\n");
    UART2_readTimeout(uart, &c, 1, &got, 4000 / portTICK_PERIOD_MS);   /* the host sends one character */
    fox_printf("PICKGOT %u c=%u\n", (unsigned)got, (unsigned)c);
    if (got == 1 && c >= '1' && c <= '3') name[4] = (char)c; else name[4] = '1';
#endif
    demo_name = name;
    fox_printf("RUN %s clock 160 MHz tick %lu Hz\n", name, (unsigned long)configTICK_RATE_HZ);
    atexit(report);
    doomgeneric_Create(7, argv);
    for (;;) doomgeneric_Tick();
}
