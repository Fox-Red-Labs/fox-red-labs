/* Platform layer for the LP-EM-CC35X1 build of rp2040-doom (target cc35x1-whx). GPL-2.0, same terms as the engine.
 * Derived from ../m33-whx/plat/platform.c. Engine side only: the console, the real clock and the FreeRTOS thread
 * are in hal_cc35x1.c. The game clock is virtual (35 ms per read, as in the emulator run that gives the reference
 * checksums), so the frames drawn do not depend on how fast the chip runs. The frame sink keeps a CRC-32 chain over
 * each finished frame buffer (320 x 168 bytes), the same bytes the emulator checksums.
 * No sound, no keys, no panel yet. */
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "doomtype.h"
#include "d_event.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_system.h"
#include "pico/sem.h"
#include "pico/time.h"
#include "doom/sounds.h"
#include "doom/r_defs.h"

extern unsigned long fox_frames;
int fox_printf(const char *fmt, ...);
/* hal_cc35x1.c */
void hal_write(const char *s, int n);
uint32_t hal_ms(void);
void hal_park(void);
#ifdef FOX_PROF
uint64_t hal_cyc(void);
uint32_t hal_stack_used(void);
int fox_printf(const char *fmt, ...);
#define IC(o) (*(volatile uint32_t *)(0x41902000u + (o)))
#define DC(o) (*(volatile uint32_t *)(0x41902400u + (o)))
static uint64_t pf_total, pf_g, pf_r, pf_c1, pf_t0, pf_frames, pf_e, pf_b;
static uint64_t pf_ih, pf_im, pf_dh, pf_dm;
static uint32_t pf_dlast;
extern uint64_t fox_pt[8];
void G_Ticker(void); void R_RenderPlayerView(void *p); void pd_core1_loop(void);
void __wrap_G_Ticker(void) { uint64_t t = hal_cyc(); G_Ticker(); pf_g += hal_cyc() - t; }
void __wrap_R_RenderPlayerView(void *p) { uint64_t t = hal_cyc(); R_RenderPlayerView(p); pf_r += hal_cyc() - t; }
void __wrap_pd_core1_loop(void) { uint64_t t = hal_cyc(); pd_core1_loop(); pf_c1 += hal_cyc() - t; }
void pd_end_frame(int w); void pd_begin_frame(void);
void __wrap_pd_end_frame(int w) { uint64_t t = hal_cyc(); pd_end_frame(w); pf_e += hal_cyc() - t; }
void __wrap_pd_begin_frame(void) { uint64_t t = hal_cyc(); pd_begin_frame(); pf_b += hal_cyc() - t; }
#define pd_core1_loop __wrap_pd_core1_loop
static void pf_frame(void)
{
    uint64_t now = hal_cyc(); uint32_t d = DC(0x40);
    if (pf_t0) { pf_total += now - pf_t0; pf_frames++; }
    pf_t0 = now;
    pf_dm += (d - pf_dlast) & 0xfffu; pf_dh += ((d >> 12) - (pf_dlast >> 12)) & 0xfffffu;
    pf_dlast = d;
}
static void pf_report(int k)
{
    uint32_t ih = IC(0x40), im = IC(0x44);
    unsigned long n = pf_frames ? (unsigned long)pf_frames : 1;
    fox_printf("PROF demo %d frames=%lu kcyc/frame total=%lu gtick=%lu render=%lu core1=%lu endframe=%lu beginframe=%lu ihit=%lu imiss=%lu dhit=%lu dmiss=%lu\n", k, (unsigned long)pf_frames,
        (unsigned long)(pf_total / n / 1000), (unsigned long)(pf_g / n / 1000), (unsigned long)(pf_r / n / 1000), (unsigned long)(pf_c1 / n / 1000), (unsigned long)(pf_e / n / 1000), (unsigned long)(pf_b / n / 1000),
        (unsigned long)(ih / n), (unsigned long)(im / n), (unsigned long)(pf_dh / n), (unsigned long)(pf_dm / n));
    fox_printf("PROFSTEP reclip=%lu predraw=%lu flats=%lu resort=%lu columns=%lu fuzz=%lu kcyc/frame\n", (unsigned long)(fox_pt[0]/n/1000), (unsigned long)(fox_pt[1]/n/1000), (unsigned long)(fox_pt[2]/n/1000), (unsigned long)(fox_pt[3]/n/1000), (unsigned long)(fox_pt[4]/n/1000), (unsigned long)(fox_pt[5]/n/1000));
    for (int i = 0; i < 8; i++) fox_pt[i] = 0;
    fox_printf("STACK used about %lu bytes\n", (unsigned long)hal_stack_used());
    (void)ih; pf_total = pf_g = pf_r = pf_c1 = pf_e = pf_b = pf_frames = 0; pf_dh = pf_dm = 0; IC(0x40) = 0; IC(0x44) = 0;
}
#endif
static uint32_t vclock;
#ifndef FOX_VCLOCK_STEP
#define FOX_VCLOCK_STEP 35
#endif
#define EMU_TIMER_MS (vclock += FOX_VCLOCK_STEP)
#ifndef CRC_FRAMES
#define CRC_FRAMES 400
#endif

void D_DoomMain(void);
void pd_core1_loop(void);
void pd_init(void);


#ifdef PAD16
static uint8_t fox_pad[16384] __attribute__((section(".bss.fox_arena_d")));
volatile uint8_t *fox_padp = fox_pad;
#endif
#ifdef FOX_LC
/* ---- lump cache: the converted game data is read with plain loads, and a load from the external flash costs about 44 cycles
 * (no data cache covers it). Lumps are copied to RAM the first time the engine asks for them and stay there; level lumps
 * go in their own arena that restarts when another level's lump is asked for. When an arena is full the lump is read in place. ---- */
extern const uint8_t *whd_map_base;
extern const uint32_t *lump_offsets;
extern unsigned int numlumps;
#define LC_MAX 1300
#define LC_MAXLUMP (40 * 1024)
static const uint8_t *lc_ptr[LC_MAX] __attribute__((section(".bss.fox_arena_d")));
static uint8_t lc_t[LCT_KB * 1024] __attribute__((aligned(256), section(".bss.fox_arena_t")));
#ifndef LCC_KB
#define LCC_KB 0
#endif
static uint8_t lc_c[LCC_KB * 1024 + 1] __attribute__((aligned(256), section(".bss.fox_arena_c")));
static uint32_t lc_cpos;
static uint8_t lc_d[LCD_KB * 1024] __attribute__((aligned(256), section(".bss.fox_arena_d")));
static uint8_t lc_l[LCL_KB * 1024 + 1] __attribute__((aligned(256), section(".bss.fox_arena_d")));
static uint32_t lc_tpos, lc_dpos, lc_lpos, lc_lmap = 0xffff;
static uint32_t lc_n, lc_bytes, lc_refused_n, lc_refused_bytes, lc_resets, lc_skipped;
#define LC_LVL_FIRST 6
#define LC_LVL_LAST 104
static uint8_t *lc_alloc(uint8_t *base, uint32_t size, uint32_t *pos, uint32_t cap, const uint8_t *src, uint32_t mask)
{
    uint32_t a = *pos, want = (uint32_t)(uintptr_t)src & mask, cur = (uint32_t)(uintptr_t)(base + a) & mask;
    a += (want - cur) & mask;   /* keep the lump's alignment (the colormap needs 256) */
    if (a + size > cap) return 0;
    *pos = a + size;
    return base + a;
}
const uint8_t *fox_lump_data(const uint32_t *l)
{
    uint32_t idx = (uint32_t)(l - lump_offsets);
    const uint8_t *src = whd_map_base + (*l & 0xffffffu);
    const uint8_t *c;
    if (idx >= LC_MAX) return src;
    c = lc_ptr[idx];
    if (c) return c == (const uint8_t *)1 ? src : c;
    {
        uint32_t size = ((l[1] - l[0]) & 0xffffffu) - (l[0] >> 30);
        uint8_t *dst = 0;
        int dm = numlumps == 1264;
        if (size == 0 || size > LC_MAXLUMP || (dm && ((idx >= 2 && idx < 6) || (idx >= 107 && idx < 232)))) { lc_ptr[idx] = (const uint8_t *)1; lc_skipped++; return src; }
        if (dm && idx >= LC_LVL_FIRST && idx <= LC_LVL_LAST) {
            uint32_t map = (idx - LC_LVL_FIRST) / 11;
            if (map != lc_lmap) {   /* another level: forget the old level's lumps */
                uint32_t i;
                for (i = LC_LVL_FIRST; i <= LC_LVL_LAST; i++) lc_ptr[i] = 0;
                lc_lmap = map; lc_lpos = 0; lc_resets++;
            }
            dst = lc_alloc(lc_l, size, &lc_lpos, LCL_KB * 1024, src, 7u);
        } else {
            uint32_t statcap_t = LCT_KB * 1024;
            dst = lc_alloc(lc_t, size, &lc_tpos, statcap_t, src, idx == 1 ? 255u : 7u);
            if (!dst && LCC_KB) dst = lc_alloc(lc_c, size, &lc_cpos, LCC_KB * 1024, src, idx == 1 ? 255u : 7u);
            if (!dst) dst = lc_alloc(lc_d, size, &lc_dpos, LCD_KB * 1024, src, idx == 1 ? 255u : 7u);
        }
        if (!dst) { lc_ptr[idx] = (const uint8_t *)1; lc_refused_n++; lc_refused_bytes += size; return src; }
        memcpy(dst, src, size);
        lc_ptr[idx] = dst; lc_n++; lc_bytes += size;
        return dst;
    }
}
static void lc_init(void) { memset(lc_ptr, 0, sizeof lc_ptr); }   /* the arena sections are not cleared at start-up */
static void lc_report(void)
{
    fox_printf("LC cached lumps=%lu bytes=%lu refused=%lu (%lu bytes) skipped=%lu levelresets=%lu tcm=%lu cram=%lu dram=%lu level=%lu\n", (unsigned long)lc_n, (unsigned long)lc_bytes,
        (unsigned long)lc_refused_n, (unsigned long)lc_refused_bytes, (unsigned long)lc_skipped, (unsigned long)lc_resets, (unsigned long)lc_tpos, (unsigned long)lc_cpos, (unsigned long)lc_dpos, (unsigned long)lc_lpos);
}
#endif

/* ---- zone: a fixed array in RAM; ZONE_BYTES comes from the build ---- */
#ifndef ZONE_BYTES
#define ZONE_BYTES (80 * 1024)
#endif
static uint8_t fox_zone[ZONE_BYTES] __attribute__((aligned(8), section(".bss.fox_zone")));
byte *I_ZoneBase(int *size) { *size = ZONE_BYTES; return fox_zone; }
uint32_t fox_zone_bytes = ZONE_BYTES;
uint8_t *fox_zone_base(void) { return fox_zone; }

/* ---- console, log ---- */
int fox_vprintf(const char *fmt, va_list ap)
{
    char buf[160], o[200]; int n = vsnprintf(buf, sizeof buf, fmt, ap), i, k = 0;
    if (n > (int)sizeof buf - 1) n = sizeof buf - 1;
    for (i = 0; i < n; i++) { if (buf[i] == '\n') o[k++] = '\r'; o[k++] = buf[i]; }
    if (k) hal_write(o, k);
    return n;
}
int fox_printf(const char *fmt, ...) { va_list ap; int n; va_start(ap, fmt); n = fox_vprintf(fmt, ap); va_end(ap); return n; }
void panic(const char *fmt, ...)
{
    va_list ap; fox_printf("PANIC "); va_start(ap, fmt); fox_vprintf(fmt, ap); va_end(ap);
    fox_printf("\nframes=%lu\n", fox_frames); for (;;) hal_park();
}
void panic_unsupported(void) { fox_printf("PANIC unsupported\n"); for (;;) hal_park(); }

/* ---- end of run: the harness hooks this symbol ---- */
void fox_exit(int code) { (void)code; for (;;) hal_park(); }
void __attribute__((noreturn)) I_Quit(void) { fox_exit(0); for (;;) {} }
void I_AtExit(atexit_func_t func, boolean run_if_error) { (void)func; (void)run_if_error; }
void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }
void I_PrintBanner(const char *msg) { (void)msg; }
void I_PrintDivider(void) {}
void I_PrintStartupBanner(const char *d) { (void)d; }
void I_BindVariables(void) {}
void *I_Realloc(void *ptr, size_t size) { (void)ptr; (void)size; fox_printf("PANIC realloc\n"); for (;;) hal_park(); }
boolean I_GetMemoryValue(unsigned int offset, void *value, int size) { (void)offset; (void)value; (void)size; return false; }
void I_Init(void) {}

/* ---- time ---- */
static uint64_t last_us;
uint64_t time_us_64(void) { uint64_t t = (uint64_t)EMU_TIMER_MS * 1000u; if (t < last_us) t = last_us; last_us = t; return t; }
uint32_t time_us_32(void) { return (uint32_t)time_us_64(); }
absolute_time_t get_absolute_time(void) { return time_us_64(); }
void sleep_ms(uint32_t ms) { uint32_t t = EMU_TIMER_MS; while ((uint32_t)(EMU_TIMER_MS - t) < ms) {} }
void sleep_us(uint64_t us) { sleep_ms((uint32_t)(us / 1000u) + 1); }
int I_GetTime(void) { return (int)(((uint64_t)EMU_TIMER_MS * TICRATE) / 1000u); }
int I_GetTimeMS(void) { return (int)EMU_TIMER_MS; }
void I_Sleep(int ms) { sleep_ms((uint32_t)ms); }
void I_WaitVBL(int count) { I_Sleep((count * 1000) / 70); }
void I_InitTimer(void) {}

/* ---- semaphores and the second core ---- */
semaphore_t render_frame_ready, display_frame_freed;
volatile uint8_t interp_in_use;
spin_lock_t shim_spin_lock;
uint8_t next_video_type, next_frame_index, next_overlay_index, *next_video_scroll;
int16_t *wipe_yoffsets_raw; uint8_t *wipe_yoffsets; uint32_t *wipe_linelookup;
volatile uint8_t wipe_min;
uint8_t __attribute__((aligned(4), section(".bss.fox_fb"))) frame_buffer[2][SCREENWIDTH * MAIN_VIEWHEIGHT];
volatile const uint8_t *fox_last_fb; volatile int fox_last_type;
unsigned long fox_frames;
static uint32_t crc_tab[256], fox_crc;
static unsigned long t_first, t_400, t_last;
static void crc_init(void)
{
    uint32_t i, k, c;
    for (i = 0; i < 256; i++) { c = i; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); crc_tab[i] = c; }
}
static void crc_update(const uint8_t *p, size_t n)
{
    uint32_t c = ~fox_crc;
    while (n--) c = crc_tab[(c ^ *p++) & 0xff] ^ (c >> 8);
    fox_crc = ~c;
}
void __attribute__((noinline)) fox_frame_done(const uint8_t *fb, int video_type)
{
    fox_last_fb = fb; fox_last_type = video_type;
#ifdef FOX_PROF
    pf_frame();
#endif
    if (fox_frames == 0) t_first = hal_ms();
    if (fox_frames < CRC_FRAMES) {
        crc_update(fb, SCREENWIDTH * MAIN_VIEWHEIGHT);
        if ((fox_frames + 1) % 100 == 0) fox_printf("CRC %lu %08lx\n", fox_frames + 1, (unsigned long)fox_crc);
    }
    if (fox_frames + 1 == CRC_FRAMES) t_400 = hal_ms();
    t_last = hal_ms();
    if ((fox_frames + 1) % 500 == 0) fox_printf("TICK frames=%lu ms=%lu\n", fox_frames + 1, t_last - t_first);
}
/* called at the top of G_CheckDemoStatus (build-time patch, patches/0001-demo-end-hook.patch) */
extern uint32_t fox_zone_hw;
static int demo_ends;
void fox_demo_end(void)
{
    unsigned long now = hal_ms();
    demo_ends++;
#ifdef FOX_LC
    lc_report();
#endif
#ifdef FOX_PROF
    pf_report(demo_ends);
#endif
    fox_printf("DEMOEND %d frames=%lu ms=%lu zonehw=%lu crc=%08lx\n", demo_ends, fox_frames, now - t_first, (unsigned long)fox_zone_hw, (unsigned long)fox_crc);
    if (demo_ends == 3) {
        fox_printf("EXIT demos=3 frames=%lu total_ms=%lu\n", fox_frames, now - t_first);
        if (fox_frames > CRC_FRAMES && t_400)
            fox_printf("FPS after%d frames=%lu ms=%lu x1000=%lu\n", CRC_FRAMES, fox_frames - CRC_FRAMES, now - t_400, (fox_frames - CRC_FRAMES) * 1000000ul / (now - t_400));
        fox_printf("DONE\n");
        for (;;) hal_park();
    }
}
void shim_core1_loop_once(void) { pd_core1_loop(); }
extern semaphore_t core1_done, core1_wake, core0_done;
static int in_core1;
volatile void *fox_stuck_sem;
void shim_sem_stuck(semaphore_t *s)
{
    fox_stuck_sem = s;
    if (s == &core1_done && !in_core1) {   /* core 0 waits for core 1: run core 1's loop here, once */
        in_core1 = 1; if (!sem_available(&core1_wake)) core1_wake.permits = 1; if (!sem_available(&core0_done)) core0_done.permits = 1; pd_core1_loop(); in_core1 = 0;
    } else {
        fox_printf("PANIC stuck semaphore\n"); for (;;) hal_park();   /* any other wait would deadlock a single core */
    }
}
#ifdef FOX_PANEL
/* ---- Waveshare 3.5 inch panel (PANEL=1 build, panel_cc35x1.c): palette and frame to the screen ---- */
#include "w_wad.h"
#include "z_zone.h"
void panel_init(void);
void panel_palette(const uint8_t *rgb);
void panel_present(const uint8_t *f0, const uint8_t *f1, int single);
static int pal_req = 0, pal_cur = -1;
/* The panel is dim next to a monitor: a brightness lift of 255 * (v / 255) ^ 0.75 on every colour value (the same table as m33-whx). */
static const uint8_t pan_lift[256] = {
    0, 4, 7, 9, 11, 13, 15, 17, 19, 21, 22, 24, 26, 27, 29, 30,
    32, 33, 35, 36, 38, 39, 41, 42, 43, 45, 46, 47, 49, 50, 51, 52,
    54, 55, 56, 58, 59, 60, 61, 62, 64, 65, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    90, 91, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,
    123, 124, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
    138, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 157, 158, 159, 160, 161, 162, 163, 164, 164, 165,
    166, 167, 168, 169, 170, 170, 171, 172, 173, 174, 175, 176, 176, 177, 178, 179,
    180, 181, 181, 182, 183, 184, 185, 186, 186, 187, 188, 189, 190, 191, 191, 192,
    193, 194, 195, 196, 196, 197, 198, 199, 200, 200, 201, 202, 203, 204, 205, 205,
    206, 207, 208, 209, 209, 210, 211, 212, 213, 213, 214, 215, 216, 216, 217, 218,
    219, 220, 220, 221, 222, 223, 224, 224, 225, 226, 227, 227, 228, 229, 230, 231,
    231, 232, 233, 234, 234, 235, 236, 237, 238, 238, 239, 240, 241, 241, 242, 243,
    244, 244, 245, 246, 247, 247, 248, 249, 250, 250, 251, 252, 253, 253, 254, 255,
};
static void pan_palette(int n)
{
    static uint8_t rgb[768];
    int l = W_GetNumForName("PLAYPAL"), len = W_LumpLength(l);
    const uint8_t *pp = (const uint8_t *)W_CacheLumpNum(l, PU_STATIC);
    if (len != 768 || n == 0) memcpy(rgb, pp + (len != 768 ? n * 768 : 0), 768);
    else {                       /* the converted file keeps one palette; the tints are computed as upstream's i_video.c does */
        int mul, r0, g0, b0;
        if (n < 9) { mul = n * 65536 / 9; r0 = 255; g0 = b0 = 0; }
        else if (n < 13) { mul = (n - 8) * 65536 / 8; r0 = 215; g0 = 186; b0 = 69; }
        else { mul = 65536 / 8; r0 = b0 = 0; g0 = 256; }
        memcpy(rgb, pp, 768);
        for (int i = 0; i < 256; i++) {
            int r = rgb[3 * i], g = rgb[3 * i + 1], b = rgb[3 * i + 2];
            r += ((r0 - r) * mul) >> 16; g += ((g0 - g) * mul) >> 16; b += ((b0 - b) * mul) >> 16;
            rgb[3 * i] = (uint8_t)r; rgb[3 * i + 1] = (uint8_t)g; rgb[3 * i + 2] = (uint8_t)b;
        }
    }
    for (int i = 0; i < 768; i++) rgb[i] = pan_lift[rgb[i]];
    panel_palette(rgb);
}
static void pan_show(const uint8_t *f0, const uint8_t *f1, int type)
{
    if (type == VIDEO_TYPE_TEXT || type == VIDEO_TYPE_WIPE || type == VIDEO_TYPE_NONE) return;
    if (pal_req != pal_cur) { pan_palette(pal_req); pal_cur = pal_req; }
    panel_present(f0, f1, type != VIDEO_TYPE_DOUBLE);
}
#endif
void shim_sem_released(semaphore_t *s)
{
    if (s == &render_frame_ready) {
        /* the display side takes the frame, then gives the buffer back */
        if (sem_try_acquire(&render_frame_ready)) {
            fox_frame_done(frame_buffer[next_frame_index], next_video_type);
#ifdef FOX_PANEL
            pan_show(frame_buffer[next_frame_index], frame_buffer[next_frame_index ^ 1], next_video_type);
#endif
            if (next_video_type == VIDEO_TYPE_WIPE) wipe_min = 200;   /* no panel: the screen melt is treated as finished at once */
            fox_frames++;
            sem_release(&display_frame_freed);
        }
    }
}

/* ---- video (no panel yet) ---- */
boolean screenvisible = true, screensaver_mode = false;
isb_int8_t usegamma = 0;
unsigned int joywait = 0;
pixel_t *I_VideoBuffer;
void I_InitGraphics(void)
{
    sem_init(&render_frame_ready, 0, 1); sem_init(&display_frame_freed, 1, 1); pd_init();
#ifdef FOX_PANEL
    panel_init();
#endif
}
void I_ShutdownGraphics(void) {}
void I_StartFrame(void) {}
void I_StartTic(void) {}
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) {}
#ifdef FOX_PANEL
void I_SetPaletteNum(int n) { pal_req = n < 0 ? 0 : n; }
#else
void I_SetPaletteNum(int n) { (void)n; }
#endif
void I_SetWindowTitle(const char *t) { (void)t; }
void I_BindVideoVariables(void) {}
void I_BindInputVariables(void) {}
void I_GraphicsCheckCommandLine(void) {}
void I_CheckIsScreensaver(void) {}
void I_DisplayFPSDots(boolean d) { (void)d; }
void I_Endoom(byte *e) { (void)e; }

/* ---- sound (none) ---- */
isb_int8_t snd_pitchshift;
void I_BindSoundVariables(void) {}
void I_InitSound(boolean s) { (void)s; }
void I_InitMusic(void) {}
void I_SetOPLDriverVer(opl_driver_ver_t v) { (void)v; }
int I_GetSfxLumpNum(should_be_const sfxinfo_t *s) { (void)s; return 0; }
int I_StartSound(should_be_const sfxinfo_t *s, int c, int v, int sep, int p) { (void)s; (void)c; (void)v; (void)sep; (void)p; return 0; }
void I_StopSound(int c) { (void)c; }
boolean I_SoundIsPlaying(int c) { (void)c; return false; }
void I_UpdateSound(void) {}
void I_UpdateSoundParams(int c, int v, int s) { (void)c; (void)v; (void)s; }
void I_PrecacheSounds(should_be_const sfxinfo_t *s, int n) { (void)s; (void)n; }
void I_SetMusicVolume(int v) { (void)v; }
void I_PauseSong(void) {}
void I_ResumeSong(void) {}
void *I_RegisterSong(should_be_const void *d, int l) { (void)d; (void)l; return (void *)1; }
void I_UnRegisterSong(void *h) { (void)h; }
void I_PlaySong(void *h, boolean l) { (void)h; (void)l; }
void I_StopSong(void) {}

/* ---- net (single player): the piconet API as no-ops ---- */
boolean net_client_connected, drone;
void piconet_init(void) {}
void piconet_start_host(int8_t d, int8_t e, int8_t s) { (void)d; (void)e; (void)s; }
void piconet_start_client(void) {}
void piconet_stop(void) {}
bool piconet_client_check_for_dropped_connection(void) { return false; }
void piconet_start_game(void) {}
struct lobby_state_s;
int piconet_get_lobby_state(void *s) { (void)s; return 0; }
void piconet_new_local_tic(int t) { (void)t; }
void piconet_new_local_tic(int t);
int piconet_maybe_recv_tic(int f) { (void)f; return 0; }

/* ---- finale and menu pieces not built for the tiny target ---- */
void F_BunnyDrawPatches(void) {}
int F_BunnyScrollPos(void) { return 0; }
void F_CastDrawer(void) {}
int F_CastSprite(void) { return 0; }
void M_DrawSaveLoadBorder(int x, int y) { (void)x; (void)y; }

void fox_doom_main(void)
{
    crc_init();
#ifdef FOX_LC
    lc_init();
#endif
    I_InitGraphics();
    D_DoomMain();
}
uint8_t restart_song_state;
uint32_t fox_zone_hw;
