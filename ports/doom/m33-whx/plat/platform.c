/* Platform layer for the M33 build of rp2040-doom. Replaces src/pico/i_system.c, i_timer.c, i_input.c, i_video.c,
 * i_picosound.c, piconet.c and stubs.c. GPL-2.0 (same terms as the engine it is linked with).
 * Emulator build: console and clock are two registers, the host harness counts frames and demo ends.
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

#ifdef FOX_EDU
#include "fox_whx.h"
#include "doomkeys.h"
#include "doom/doomstat.h"
#include "doom/d_items.h"
#include "doom/g_game.h"
#include "w_wad.h"
#include "z_zone.h"
void edu_set_palette(const uint8_t *rgb);
void edu_present(const uint8_t *fb0, const uint8_t *fb1, int single, const char *l1, const char *l2);
void edu_inputs(int *dx, int *dy, int *s1, int *s2);
uint32_t edu_fps10(void);
uint32_t edu_script_ms(void);
#endif

#ifdef FOX_BOARD
/* On the board the clock is a counter that advances 35 ms on every read, as the emulator's does (emu_qspi.py --ms-per-read 35), so the engine
 * plays the demos with the same tics per frame and the frame checksums match the emulator's. Real time is read from the DWT cycle counter. */
uint32_t fox_board_ms(void);
void fox_board_putc(char c);
void fox_board_frame(const uint8_t *fb);
#define EMU_TIMER_MS fox_board_ms()
#define EMU_CONSOLE_PUT(c) fox_board_putc(c)
#else
#define EMU_TIMER_MS (*(volatile uint32_t *)0x50000000u)
#define EMU_CONSOLE  (*(volatile uint32_t *)0x50000004u)
#define EMU_CONSOLE_PUT(c) (EMU_CONSOLE = (uint32_t)(uint8_t)(c))
#endif

void D_DoomMain(void);
void pd_core1_loop(void);
void pd_init(void);

/* ---- zone: a fixed array in RAM; ZONE_BYTES comes from the build ---- */
#ifndef ZONE_BYTES
#define ZONE_BYTES (80 * 1024)
#endif
static uint8_t fox_zone[ZONE_BYTES] __attribute__((aligned(8)));
byte *I_ZoneBase(int *size) { *size = ZONE_BYTES; return fox_zone; }
uint32_t fox_zone_bytes = ZONE_BYTES;
uint8_t *fox_zone_base(void) { return fox_zone; }

/* ---- console, log ---- */
static void putc_emu(char c) { EMU_CONSOLE_PUT(c); }
int fox_vprintf(const char *fmt, va_list ap)
{
    char buf[160]; int n = vsnprintf(buf, sizeof buf, fmt, ap), i;
    for (i = 0; i < n && i < (int)sizeof buf - 1; i++) putc_emu(buf[i]);
    return n;
}
void panic(const char *fmt, ...) { (void)fmt; for (;;) __breakpoint(); }
void panic_unsupported(void) { for (;;) __breakpoint(); }

/* ---- end of run: the harness hooks this symbol ---- */
void fox_exit(int code) { (void)code; for (;;) {} }
void __attribute__((noreturn)) I_Quit(void) { fox_exit(0); for (;;) {} }
void I_AtExit(atexit_func_t func, boolean run_if_error) { (void)func; (void)run_if_error; }
void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }
void I_PrintBanner(const char *msg) { (void)msg; }
void I_PrintDivider(void) {}
void I_PrintStartupBanner(const char *d) { (void)d; }
void I_BindVariables(void) {}
void *I_Realloc(void *ptr, size_t size) { (void)ptr; (void)size; for (;;) __breakpoint(); }
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
uint8_t __attribute__((aligned(4))) frame_buffer[2][SCREENWIDTH * MAIN_VIEWHEIGHT];
volatile const uint8_t *fox_last_fb; volatile int fox_last_type;
void __attribute__((noinline)) fox_frame_done(const uint8_t *fb, int video_type) { fox_last_fb = fb; fox_last_type = video_type;
#ifdef FOX_BOARD
    fox_board_frame(fb);
#endif
}
#ifdef FOX_EDU
/* ---- Educational BoosterPack: palette, HUD text, frame to the panel (plat/edu.c) ---- */
static int pal_req = 0, pal_cur = -1, playing;
volatile int32_t fox_edu_dbg[8];   /* read over the probe: gamestate, player x and y (map units), angle (top byte), health, ammo, key events posted, gametic */
/* The panel is dim next to a monitor: a brightness lift of 255 * (v / 255) ^ 0.75 on every colour value (about gamma 1.3). */
static const uint8_t edu_lift[256] = {
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
static void edu_palette(int n)
{
    static uint8_t rgb[768];
    int l = W_GetNumForName("PLAYPAL"), len = W_LumpLength(l);
    const uint8_t *pp = (const uint8_t *)W_CacheLumpNum(l, PU_STATIC);
    if (len != 768 || n == 0) { FW_COPY(rgb, pp + (len != 768 ? n * 768 : 0), 768); }
    else {                       /* the converted file keeps one palette; the tints are computed as upstream's i_video.c does */
        int mul, r0, g0, b0;
        if (n < 9) { mul = n * 65536 / 9; r0 = 255; g0 = b0 = 0; }
        else if (n < 13) { mul = (n - 8) * 65536 / 8; r0 = 215; g0 = 186; b0 = 69; }
        else { mul = 65536 / 8; r0 = b0 = 0; g0 = 256; }
        FW_COPY(rgb, pp, 768);
        for (int i = 0; i < 256; i++) {
            int r = rgb[3 * i], g = rgb[3 * i + 1], b = rgb[3 * i + 2];
            r += ((r0 - r) * mul) >> 16; g += ((g0 - g) * mul) >> 16; b += ((b0 - b) * mul) >> 16;
            rgb[3 * i] = (uint8_t)r; rgb[3 * i + 1] = (uint8_t)g; rgb[3 * i + 2] = (uint8_t)b;
        }
    }
    for (int i = 0; i < 768; i++) rgb[i] = edu_lift[rgb[i]];
    edu_set_palette(rgb);
}
static char *put_num(char *o, int v, int w) { char t[8]; int n = 0; if (v < 0) v = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7); while (n < w) t[n++] = ' '; while (n) *o++ = t[--n]; return o; }
static void edu_show(const uint8_t *f0, const uint8_t *f1, int type)
{
    if (type == VIDEO_TYPE_TEXT || type == VIDEO_TYPE_WIPE || type == VIDEO_TYPE_NONE) return;
    if (pal_req != pal_cur) { edu_palette(pal_req); pal_cur = pal_req; }
    char l1[24], l2[24], *o = l1;
    player_t *pl = &players[consoleplayer];
    int ammo = weaponinfo[pl->readyweapon].ammo; ammo = ammo >= NUMAMMO ? -1 : pl->ammo[ammo];
    fox_edu_dbg[0] = gamestate; fox_edu_dbg[4] = pl->health; fox_edu_dbg[5] = ammo; fox_edu_dbg[7] = gametic;
    if (pl->mo) { fox_edu_dbg[1] = pl->mo->xy.x >> 16; fox_edu_dbg[2] = pl->mo->xy.y >> 16; fox_edu_dbg[3] = (int32_t)(mobj_angle(pl->mo) >> 24); }
    *o++ = 'H'; *o++ = 'P'; *o++ = ' '; o = put_num(o, pl->health, 3);
    *o++ = ' '; *o++ = 'A'; *o++ = 'R'; *o++ = ' '; o = put_num(o, pl->armorpoints, 3);
    *o++ = ' '; *o++ = 'A'; *o++ = 'M'; *o++ = ' '; if (ammo < 0) { *o++ = '-'; *o++ = '-'; *o++ = '-'; } else o = put_num(o, ammo, 3);
    *o = 0;
    o = l2; const char *m = playing ? "PLAY" : "DEMO"; while (*m) *o++ = *m++;
    *o++ = ' '; *o++ = ' '; uint32_t f = edu_fps10(); o = put_num(o, (int)(f / 10), 2); *o++ = '.'; *o++ = (char)('0' + f % 10); *o++ = ' '; *o++ = 'F'; *o++ = 'P'; *o++ = 'S'; *o = 0;
    edu_present(f0, f1, type != VIDEO_TYPE_DOUBLE, l1, l2);
#ifdef EDU_HALT_ON_PAGE
    if (type != VIDEO_TYPE_DOUBLE) for (;;) {}    /* a check image: stop on the first page (the title) so the panel and the frame buffers can be read */
#endif
}
#endif
unsigned long fox_frames;
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
        for (;;) __breakpoint();           /* any other wait would deadlock a single core */
    }
}
void shim_sem_released(semaphore_t *s)
{
    if (s == &render_frame_ready) {
        /* the display side takes the frame, then gives the buffer back */
        if (sem_try_acquire(&render_frame_ready)) {
            fox_frame_done(frame_buffer[next_frame_index], next_video_type);
#ifdef FOX_EDU
            edu_show(frame_buffer[next_frame_index], frame_buffer[next_frame_index ^ 1], next_video_type);
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
void I_InitGraphics(void) { sem_init(&render_frame_ready, 0, 1); sem_init(&display_frame_freed, 1, 1); pd_init(); }
void I_ShutdownGraphics(void) {}
void I_StartFrame(void) {}
#ifdef FOX_EDU
/* Stick up or down is forward and back, left and right turn; S1 fires, S2 uses. In the demo loop S1 or S2 starts a game (episode 1, map 1,
 * medium); after that the keys go to the engine as keyboard events. Right is the high X reading and down the high Y reading (edumkii.h). */
#ifndef EDU_STICK
#define EDU_STICK 700
#endif
enum { W_UP = 1, W_DOWN = 2, W_LEFT = 4, W_RIGHT = 8, W_FIRE = 16, W_USE = 32 };
static const uint8_t edu_keys[6] = { KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_RCTRL, ' ' };
void I_StartTic(void)
{
    static int have;
    int dx, dy, s1, s2, want = 0;
    edu_inputs(&dx, &dy, &s1, &s2);
    if (dy > EDU_STICK) want |= W_UP; if (dy < -EDU_STICK) want |= W_DOWN;   /* raw Y rises as the stick is pushed away from the player (by hand, 2026-10-23) */
    if (dx < -EDU_STICK) want |= W_LEFT; if (dx > EDU_STICK) want |= W_RIGHT;
    if (s1) want |= W_FIRE; if (s2) want |= W_USE;
#ifdef EDU_SCRIPT
    { uint32_t t = edu_script_ms();              /* a scripted player for the camera check: start at 8 s, then walk, turn and fire in a 6 s loop */
      if (t >= 8000u) { want |= (t < 8300u) ? W_FIRE : 0; uint32_t c = (t - 8300u) % 6000u;
          if (t >= 12000u) { if (c < 3000u) want |= W_UP; else if (c < 4000u) want |= W_UP | W_RIGHT; else if (c < 5000u) want |= W_FIRE; else want |= W_LEFT; } } }
#endif
    if (!playing) {
        if (want & (W_FIRE | W_USE)) { playing = 1; have = 0; G_DeferedInitNew(sk_medium, 1, 1, false); }
        return;
    }
    for (int k = 0; k < 6; k++) {
        int now = (want >> k) & 1, was = (have >> k) & 1;
        if (now != was) { event_t e; e.type = now ? ev_keydown : ev_keyup; e.data1 = edu_keys[k]; e.data2 = edu_keys[k]; e.data3 = edu_keys[k]; e.data4 = 0; e.data5 = 0; D_PostEvent(&e); fox_edu_dbg[6]++; }
    }
    have = want;
}
#else
void I_StartTic(void) {}
#endif
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) {}
#ifdef FOX_EDU
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

#ifdef FOX_BOARD
void fox_board_init(void);
#endif
int main(void)
{
#ifdef FOX_BOARD
    fox_board_init();
#endif
    I_InitGraphics();
    D_DoomMain();
    return 0;
}
uint8_t restart_song_state;
uint32_t fox_zone_hw;
