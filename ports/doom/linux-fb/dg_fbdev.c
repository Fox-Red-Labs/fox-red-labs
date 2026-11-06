/* Linux framebuffer platform layer for doomgeneric (GPL-2.0, see ../LICENSE).
 * Draws the engine's 320x200 8-bit frame into /dev/fb0 (32 bpp) in software, integer-scaled and centered,
 * and reads one evdev game pad. No SDL, no GPU, no DRM master: stop any program that owns the display first.
 * Env: DOOM_FB (default /dev/fb0), DOOM_SCALE "XxY" (default 4x5: 1280x1000, aspect 1.28, nearest to 4:3
 *      with whole numbers inside 1080 lines), DOOM_PAD (default: first /dev/input/event* with ABS_Y and a south
 *      button), DOOM_FPS_EVERY (seconds between fps lines on stderr, default 0 = off). */
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "doomgeneric.h"
#include "doomkeys.h"

static uint8_t *fb;
static uint32_t stride, fbw, fbh;
static int sx = 4, sy = 5, ox, oy, pad = -1;
static double fps_every, t_last;
static long frames, frames_last;

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static int test_bit(const unsigned long *b, int n) { return (b[n / (8 * sizeof(long))] >> (n % (8 * sizeof(long)))) & 1; }

static int generic_ds4; /* set by open_pad, read by action_of */

static int open_pad(void)
{
    char p[64];
    const char *e = getenv("DOOM_PAD");
    if (e) {
        unsigned long key[(KEY_MAX + 64) / 64] = {0};
        int fd = open(e, O_RDONLY | O_NONBLOCK);
        if (fd >= 0 && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key) >= 0) generic_ds4 = test_bit(key, BTN_C) || test_bit(key, BTN_Z);
        return fd;
    }
    for (int i = 0; i < 32; i++) {
        unsigned long abs[(ABS_MAX + 64) / 64] = {0}, key[(KEY_MAX + 64) / 64] = {0};
        snprintf(p, sizeof p, "/dev/input/event%d", i);
        int fd = open(p, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs), abs) >= 0 && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key) >= 0 &&
            test_bit(abs, ABS_Y) && test_bit(key, BTN_SOUTH)) {
            generic_ds4 = test_bit(key, BTN_C) || test_bit(key, BTN_Z);
            fprintf(stderr, "FBDOOM pad %s (%s button table)\n", p, generic_ds4 ? "hid-generic DualShock 4" : "standard");
            return fd;
        }
        close(fd);
    }
    return -1;
}

void DG_Init(void)
{
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    const char *dev = getenv("DOOM_FB") ? getenv("DOOM_FB") : "/dev/fb0";
    int fd = open(dev, O_RDWR);
    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0) { perror(dev); exit(2); }
    if (v.bits_per_pixel != 32) { fprintf(stderr, "need 32 bpp, %s has %u\n", dev, v.bits_per_pixel); exit(2); }
    fbw = v.xres; fbh = v.yres; stride = f.line_length;
    fb = mmap(NULL, (size_t)stride * v.yres_virtual, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); exit(2); }
    if (getenv("DOOM_SCALE")) sscanf(getenv("DOOM_SCALE"), "%dx%d", &sx, &sy);
    if (320u * sx > fbw || 200u * sy > fbh) { fprintf(stderr, "scale %dx%d does not fit %ux%u\n", sx, sy, fbw, fbh); exit(2); }
    ox = (int)(fbw - 320 * sx) / 2; oy = (int)(fbh - 200 * sy) / 2;
    memset(fb, 0, (size_t)stride * fbh);
    pad = open_pad();
    if (getenv("DOOM_FPS_EVERY")) fps_every = atof(getenv("DOOM_FPS_EVERY"));
    t_last = now_s();
    fprintf(stderr, "FBDOOM fb %ux%u stride %u, picture %dx%d at %d,%d (scale %dx%d), pad fd %d\n",
            fbw, fbh, stride, 320 * sx, 200 * sy, ox, oy, sx, sy, pad);
}

void DG_DrawFrame8(const uint8_t *idx, const uint8_t *pal)
{
    static uint32_t lut[256], row[320 * 8];
    for (int i = 0; i < 256; i++) lut[i] = 0xFF000000u | (uint32_t)pal[i * 3] << 16 | (uint32_t)pal[i * 3 + 1] << 8 | pal[i * 3 + 2];
    for (int y = 0; y < 200; y++) {
        const uint8_t *s = idx + y * 320;
        uint32_t *r = row;
        for (int x = 0; x < 320; x++) {
            uint32_t c = lut[s[x]];
            for (int k = 0; k < sx; k++) *r++ = c;
        }
        uint8_t *d = fb + (size_t)(oy + y * sy) * stride + (size_t)ox * 4;
        for (int k = 0; k < sy; k++, d += stride) memcpy(d, row, (size_t)320 * sx * 4);
    }
    frames++;
    if (fps_every > 0) {
        double t = now_s();
        if (t - t_last >= fps_every) {
            fprintf(stderr, "FBDOOM fps %.2f\n", (frames - frames_last) / (t - t_last));
            frames_last = frames; t_last = t;
        }
    }
}

void DG_DrawFrame(void) {}
void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }
uint32_t DG_GetTicksMs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u); }
void DG_SetWindowTitle(const char *title) { (void)title; }

/* Pad to keys, console layout. Stick and d-pad: arrows (stick up is a low ABS_Y). Cross: fire, plus Enter and
 * 'y' for the menus and their yes/no prompts. Square: use (open doors). Circle: run (hold). Triangle: next weapon.
 * L1, R1: strafe left and right. R2: fire. Options: Escape (menu).
 * Two button tables. With no Sony driver in the kernel (CONFIG_HID_PLAYSTATION and CONFIG_HID_SONY not set, as on
 * the AM62L SDK image), hid-generic numbers the DualShock 4 buttons in HID order: square 304 (BTN_SOUTH), cross 305,
 * circle 306 (BTN_C), triangle 307, L1 308, R1 309 (BTN_Z), L2 310, R2 311, share 312, Options 313, L3 314, R3 315,
 * PS 316, touchpad 317. A pad that reports BTN_C or BTN_Z is read that way; any other pad (hid-playstation, xpad)
 * uses the standard names: BTN_SOUTH cross, BTN_EAST circle, BTN_WEST square, BTN_NORTH triangle. */
#define FOX_KEY_NEXTWEAPON 0xa6 /* unused by doomkeys.h; bound to key_nextweapon after start */
enum { A_NONE, A_FIRE, A_USE, A_RUN, A_WEAPON, A_STRAFE_L, A_STRAFE_R, A_MENU };
static int action_of(int code)
{
    if (generic_ds4) switch (code) {
        case 304: return A_USE; case 305: return A_FIRE; case 306: return A_RUN; case 307: return A_WEAPON;
        case 308: return A_STRAFE_L; case 309: return A_STRAFE_R; case 311: return A_FIRE; case 313: return A_MENU;
        default: return A_NONE;
    }
    switch (code) {
    case BTN_SOUTH: return A_FIRE; case BTN_WEST: return A_USE; case BTN_EAST: return A_RUN; case BTN_NORTH: return A_WEAPON;
    case BTN_TL: return A_STRAFE_L; case BTN_TR: return A_STRAFE_R; case BTN_TR2: return A_FIRE; case BTN_START: return A_MENU;
    default: return A_NONE;
    }
}
static struct { unsigned char key; int pressed; } q[64];
static int qh, qt;
static void push(unsigned char k, int p) { int n = (qt + 1) % 64; if (n != qh) { q[qt].key = k; q[qt].pressed = p; qt = n; } }
static int held[4]; /* up, down, left, right */
static void dir(int i, unsigned char k, int on) { if (held[i] != on) { held[i] = on; push(k, on); } }

int DG_GetKey(int *pressed, unsigned char *key)
{
    struct input_event e;
    while (pad >= 0 && read(pad, &e, sizeof e) == (ssize_t)sizeof e) {
        if (e.type == EV_ABS && e.code == ABS_Y) { dir(0, KEY_UPARROW, e.value < 64); dir(1, KEY_DOWNARROW, e.value > 192); }
        else if (e.type == EV_ABS && e.code == ABS_X) { dir(2, KEY_LEFTARROW, e.value < 64); dir(3, KEY_RIGHTARROW, e.value > 192); }
        else if (e.type == EV_ABS && e.code == ABS_HAT0Y) { dir(0, KEY_UPARROW, e.value < 0); dir(1, KEY_DOWNARROW, e.value > 0); }
        else if (e.type == EV_ABS && e.code == ABS_HAT0X) { dir(2, KEY_LEFTARROW, e.value < 0); dir(3, KEY_RIGHTARROW, e.value > 0); }
        else if (e.type == EV_KEY && e.value != 2) {
            int p = e.value;
            switch (action_of(e.code)) {
            case A_FIRE: push(KEY_FIRE, p); push(KEY_ENTER, p); push('y', p); break;
            case A_USE: push(KEY_USE, p); break;
            case A_RUN: push(KEY_RSHIFT, p); break;
            case A_WEAPON: push(FOX_KEY_NEXTWEAPON, p); break;
            case A_STRAFE_L: push(KEY_STRAFE_L, p); break;
            case A_STRAFE_R: push(KEY_STRAFE_R, p); break;
            case A_MENU: push(KEY_ESCAPE, p); break;
            default: break;
            }
        }
    }
    if (qh == qt) return 0;
    *pressed = q[qh].pressed; *key = q[qh].key; qh = (qh + 1) % 64;
    return 1;
}

int main(int argc, char **argv)
{
    void doomgeneric_Create(int argc, char **argv);
    void doomgeneric_Tick(void);
    doomgeneric_Create(argc, argv);
    { extern int key_nextweapon; key_nextweapon = FOX_KEY_NEXTWEAPON; } /* after the config load in D_DoomMain */
    for (;;) doomgeneric_Tick();
}
