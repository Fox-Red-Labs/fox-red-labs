/* lean_fe.c: a minimal libretro frontend for the AM62L EVM. No X, no container, no OpenGL.
 * It loads a libretro core (.so), runs it, and draws straight onto the HDMI connector through
 * DRM/KMS "dumb" buffers (tidss has no GPU). Video: integer nearest scaling in software into a
 * mode-sized buffer ("sw"), or the core's frame on the plane with the plane doing the scaling ("hw").
 * Optional: video on a second thread (-t), audio through ALSA on a third (-a), no vblank wait (-n).
 * It prints one result line: frames per second, per-frame time, vblanks missed, CPU use per core.
 *
 * Build on the board:  gcc -O3 -o lean_fe lean_fe.c -I<dir with libretro.h> -I/usr/include/libdrm -ldl -ldrm -lasound -lpthread -lm
 * Usage: lean_fe -c core.so -g content [-m WxH] [-s sw|hw] [-t] [-a] [-n] [-d seconds] [-p] [-D frame.ppm] [-V key=val]
 *   -m   HDMI mode, default 640x480     -s sw|hw   scaling path, default sw
 *   -t   video thread                   -a         audio to ALSA (-A device, default plughw:0,0)
 *   -n   no vblank wait (free run)      -d         run seconds (default 30), the first -W (2) s are warm-up; -B id -N n pulse a button n times; -T dump time
 *   -p   press Start and A now and then to get past title screens   -V  core option override
 * License: MIT, Fox Red Labs. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <math.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/input.h>
#include "font8x8_basic.h"
#include "libretro.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* ---------- options ---------- */
static const char *o_core, *o_game, *o_ppm, *o_adev = "plughw:0,0";
static int o_w = 640, o_h = 480, o_hw, o_thread, o_audio, o_nosync, o_press, o_auto, o_play, o_lat;
static const char *o_menu;
static double t_lat_print, lat_ev_t, lat_sum, lat_max, lat_min = 1e9; static long lat_n;   /* pad event to flip latency */
static double o_secs = 30, o_warm = 2, o_dumpt = 6; static int o_B = -1, o_N;
static const char *o_var[32][2]; static int o_nvar;

/* ---------- DRM ---------- */
struct fbuf { uint32_t handle, id, pitch; size_t size; uint8_t *map; int w, h; };
static int drm = -1; static uint32_t crtc_id, conn_id, plane_id; static drmModeModeInfo mode;
static struct fbuf big[2], small[2]; static int bi, si;
static int fb_bpp = 16;                 /* 16 = RGB565, 32 = XRGB8888, follows the core */
static int pending_flip;

static int mkfb(struct fbuf *f, int w, int h, int bpp) {
  struct drm_mode_create_dumb c = { .width = w, .height = h, .bpp = bpp };
  if (drmIoctl(drm, DRM_IOCTL_MODE_CREATE_DUMB, &c)) return -1;
  f->handle = c.handle; f->pitch = c.pitch; f->size = c.size; f->w = w; f->h = h;
  uint32_t h4[4] = { c.handle }, p4[4] = { c.pitch }, o4[4] = { 0 };
  if (drmModeAddFB2(drm, w, h, bpp == 16 ? DRM_FORMAT_RGB565 : DRM_FORMAT_XRGB8888, h4, p4, o4, &f->id, 0)) return -2;
  struct drm_mode_map_dumb m = { .handle = c.handle };
  if (drmIoctl(drm, DRM_IOCTL_MODE_MAP_DUMB, &m)) return -3;
  f->map = mmap(0, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm, m.offset);
  if (f->map == MAP_FAILED) return -4;
  memset(f->map, 0, c.size);
  return 0;
}

static int drm_open(void) {
  drm = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (drm < 0) { perror("card0"); return -1; }
  drmSetClientCap(drm, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  drmModeRes *r = drmModeGetResources(drm);
  drmModeConnector *c = 0;
  for (int i = 0; i < r->count_connectors; i++) {
    c = drmModeGetConnector(drm, r->connectors[i]);
    if (c->connection == DRM_MODE_CONNECTED) break;
    drmModeFreeConnector(c); c = 0;
  }
  if (!c) { fprintf(stderr, "no connected connector\n"); return -1; }
  conn_id = c->connector_id; int found = 0;
  static const int pref[][2] = { { 1920, 1080 }, { 1280, 720 }, { 1024, 768 }, { 800, 600 }, { 640, 480 } };
  for (int p = o_auto ? 0 : -1; !found && p < (o_auto ? 5 : 0); p++) {
    int w = p < 0 ? o_w : pref[p][0], h = p < 0 ? o_h : pref[p][1];
    for (int i = 0; i < c->count_modes && !found; i++)
      if (c->modes[i].hdisplay == w && c->modes[i].vdisplay == h && c->modes[i].vrefresh >= 59) { mode = c->modes[i]; found = 1; }
    if (p < 0) break;
  }
  if (!found) { fprintf(stderr, "no %dx%d mode\n", o_w, o_h); return -1; }
  drmModeEncoder *e = drmModeGetEncoder(drm, c->encoder_id);
  crtc_id = e ? e->crtc_id : r->crtcs[0];
  drmModePlaneRes *pr = drmModeGetPlaneResources(drm);
  plane_id = pr->planes[0];
  return 0;
}

static void drm_alloc(int cw, int ch) {
  for (int i = 0; i < 2; i++) {
    int e = mkfb(&big[i], mode.hdisplay, mode.vdisplay, fb_bpp);
    if (e) { fprintf(stderr, "big fb %d\n", e); exit(1); }
    if (o_hw) { e = mkfb(&small[i], cw, ch, fb_bpp); if (e) { fprintf(stderr, "small fb %d\n", e); exit(1); } }
  }
  if (drmModeSetCrtc(drm, crtc_id, big[0].id, 0, 0, &conn_id, 1, &mode)) { perror("SetCrtc"); exit(1); }
}

static void flip_handler(int fd, unsigned seq, unsigned s, unsigned us, void *d) { (void)fd; (void)seq; (void)s; (void)us; (void)d; pending_flip = 0; }
static double t_last_flip; static long vb_miss, vb_total; static double vb_interval;
static void wait_flip(void) {
  drmEventContext ev = { .version = 2, .page_flip_handler = flip_handler };
  while (pending_flip) {
    struct pollfd p = { drm, POLLIN, 0 };
    if (poll(&p, 1, 200) > 0) drmHandleEvent(drm, &ev); else break;
  }
}

/* integer nearest scaling of one source line, k times wider */
static inline void line16(uint16_t *d, const uint16_t *s, int w, int k) {
  if (k == 1) memcpy(d, s, w * 2);
  else if (k == 2) { uint32_t *d2 = (uint32_t *)d; for (int x = 0; x < w; x++) d2[x] = s[x] * 0x10001u; }
  else for (int x = 0; x < w; x++) { uint16_t v = s[x]; for (int j = 0; j < k; j++) *d++ = v; }
}
static inline void line32(uint32_t *d, const uint32_t *s, int w, int k) {
  if (k == 1) memcpy(d, s, w * 4);
  else for (int x = 0; x < w; x++) { uint32_t v = s[x]; for (int j = 0; j < k; j++) *d++ = v; }
}

static uint8_t *linebuf;
static double t_scale_acc, t_wait_acc; static long n_pres;
static int last_k;

static void present(const uint8_t *src, int w, int h, size_t pitch) {
  double t0 = now();
  if (!o_hw) {
    struct fbuf *f = &big[bi ^ 1]; int bytes = fb_bpp / 8;
    int k = mode.hdisplay / w; int k2 = mode.vdisplay / h; if (k2 < k) k = k2; if (k < 1) k = 1;
    last_k = k;
    int ow = w * k, oh = h * k; if (ow > f->w) ow = f->w; if (oh > f->h) oh = f->h;
    int x0 = (f->w - ow) / 2, y0 = (f->h - oh) / 2;
    for (int y = 0; y < oh / k; y++) {
      const uint8_t *s = src + y * pitch;
      if (fb_bpp == 16) line16((uint16_t *)linebuf, (const uint16_t *)s, ow / k, k);
      else line32((uint32_t *)linebuf, (const uint32_t *)s, ow / k, k);
      for (int j = 0; j < k; j++) memcpy(f->map + (size_t)(y0 + y * k + j) * f->pitch + x0 * bytes, linebuf, ow * bytes);
    }
    t_scale_acc += now() - t0;
    if (!o_nosync) {
      double t1 = now();
      wait_flip();
      double tw = now();
      if (drmModePageFlip(drm, crtc_id, f->id, DRM_MODE_PAGE_FLIP_EVENT, 0)) perror("PageFlip"); else pending_flip = 1;
      wait_flip();
      double t2 = now();
      t_wait_acc += t2 - t1 - 0; (void)tw;
      if (t_last_flip > 0) { vb_total++; if (t2 - t_last_flip > 1.5 * vb_interval) vb_miss++; }
      t_last_flip = t2; bi ^= 1;
      if (lat_ev_t > 0) { double l = t2 - lat_ev_t; lat_ev_t = 0; if (l > 0 && l < 1) { lat_sum += l; lat_n++; if (l > lat_max) lat_max = l; if (l < lat_min) lat_min = l; } }
    }
  } else {
    struct fbuf *f = &small[si ^ 1]; int bytes = fb_bpp / 8;
    for (int y = 0; y < h; y++) memcpy(f->map + (size_t)y * f->pitch, src + y * pitch, w * bytes);
    int k = mode.hdisplay / w; int k2 = mode.vdisplay / h; if (k2 < k) k = k2; if (k < 1) k = 1;
    if (getenv("LEAN_K")) k = atoi(getenv("LEAN_K"));   /* test: force the plane scale */
    last_k = k;
    t_scale_acc += now() - t0;
    double t1 = now();
    int dw = w * k, dh = h * k;
    int r = drmModeSetPlane(drm, plane_id, crtc_id, f->id, 0, (mode.hdisplay - dw) / 2, (mode.vdisplay - dh) / 2, dw, dh, 0, 0, w << 16, h << 16);
    if (r) { static int once; if (!once++) fprintf(stderr, "SetPlane %dx%d -> %dx%d failed: %s\n", w, h, dw, dh, strerror(errno)); }
    double t2 = now();
    t_wait_acc += t2 - t1;
    if (t_last_flip > 0) { vb_total++; if (t2 - t_last_flip > 1.5 * vb_interval) vb_miss++; }
    t_last_flip = t2; si ^= 1;
  }
  n_pres++;
}

/* ---------- video thread ---------- */
static pthread_t vth; static pthread_mutex_t vmx = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t vcv = PTHREAD_COND_INITIALIZER;
static uint8_t *stage; static int sw_, sh_; static size_t sp_; static int vjob, vquit, vstarted;
static void *vthread(void *a) {
  (void)a;
  pthread_mutex_lock(&vmx);
  for (;;) {
    while (!vjob && !vquit) pthread_cond_wait(&vcv, &vmx);
    if (vquit && !vjob) break;
    pthread_mutex_unlock(&vmx);
    present(stage, sw_, sh_, sp_);
    pthread_mutex_lock(&vmx); vjob = 0; pthread_cond_broadcast(&vcv);
  }
  pthread_mutex_unlock(&vmx);
  return 0;
}

/* ---------- audio ---------- */
#define ARING (1 << 16)
static int16_t aring[ARING * 2]; static volatile unsigned a_w, a_r; static long a_drop, a_xrun, a_frames; static double a_rate = 32040;
static pthread_t ath; static volatile int aquit;
static void *athread(void *x) {
  (void)x; snd_pcm_t *p;
  int e = snd_pcm_open(&p, o_adev, SND_PCM_STREAM_PLAYBACK, 0);
  if (e < 0) { fprintf(stderr, "alsa open %s: %s\n", o_adev, snd_strerror(e)); return 0; }
  e = snd_pcm_set_params(p, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, (unsigned)(a_rate + 0.5), 1, 100000);
  if (e < 0) { fprintf(stderr, "alsa params: %s\n", snd_strerror(e)); return 0; }
  int16_t buf[512 * 2];
  while (!aquit) {
    unsigned avail = a_w - a_r;
    if (avail < 256) { usleep(2000); continue; }
    unsigned n = avail > 512 ? 512 : avail;
    for (unsigned i = 0; i < n; i++) { unsigned j = (a_r + i) & (ARING - 1); buf[2 * i] = aring[2 * j]; buf[2 * i + 1] = aring[2 * j + 1]; }
    a_r += n;
    snd_pcm_sframes_t r = snd_pcm_writei(p, buf, n);
    if (r < 0) { a_xrun++; snd_pcm_recover(p, (int)r, 1); } else a_frames += r;
  }
  snd_pcm_close(p);
  return 0;
}
static size_t cb_audio_batch(const int16_t *d, size_t n) {
  if (!o_audio) return n;
  for (size_t i = 0; i < n; i++) {
    if (a_w - a_r >= ARING) { a_drop++; continue; }
    unsigned j = a_w & (ARING - 1); aring[2 * j] = d[2 * i]; aring[2 * j + 1] = d[2 * i + 1]; a_w++;
  }
  return n;
}
static void cb_audio(int16_t l, int16_t r) { int16_t b[2] = { l, r }; cb_audio_batch(b, 1); }

/* ---------- gamepads (evdev) ---------- */
/* Up to two pads, port 0 and 1. Any device with a gamepad button (BTN_SOUTH family or the old
 * joystick range) and an X axis is taken; touchpads and motion sensors of the same device are skipped.
 * Pads are rescanned about once a second, so plugging one in later works. */
#define NPAD 2
#define BITS(n) (((n) + 7) / 8)
struct pad { int fd; char name[64], path[32]; uint16_t mask; int xmin, xmax, ymin, ymax, legacy; };
static struct pad pads[NPAD] = { { .fd = -1 }, { .fd = -1 } };
static double t_combo, t_rescan; static volatile int quit_req;
static const char *mapping_line = "D-pad or left stick; bottom button = B, right = A, left = Y, top = X; L1/R1 = L/R; Share or Select = Select; Options or Start = Start; hold Select+Start for 2 s to quit";

static int testbit(const unsigned long *b, int n) { return (b[n / (8 * sizeof(long))] >> (n % (8 * sizeof(long)))) & 1; }

static void pad_try(const char *path) {
  for (int i = 0; i < NPAD; i++) if (pads[i].fd >= 0 && !strcmp(pads[i].path, path)) return;
  int slot = -1; for (int i = 0; i < NPAD; i++) if (pads[i].fd < 0) { slot = i; break; }
  if (slot < 0) return;
  int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC); if (fd < 0) return;
  unsigned long ev[BITS(EV_MAX + 1) / sizeof(long) + 1] = {0}, key[BITS(KEY_MAX + 1) / sizeof(long) + 1] = {0}, abs_[BITS(ABS_MAX + 1) / sizeof(long) + 1] = {0};
  char name[64] = "?"; ioctl(fd, EVIOCGNAME(sizeof name), name);
  ioctl(fd, EVIOCGBIT(0, sizeof ev), ev); ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key); ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs_), abs_);
  if (getenv("LEAN_PAD") && !strstr(name, getenv("LEAN_PAD"))) { close(fd); return; }   /* test: only pads whose name contains this */
  int gp = testbit(key, BTN_SOUTH) || testbit(key, BTN_EAST) || testbit(key, BTN_TRIGGER) || testbit(key, BTN_THUMB);
  int hasabs = testbit(ev, EV_ABS) && (testbit(abs_, ABS_X) || testbit(abs_, ABS_HAT0X));
  if (!gp || !hasabs || strstr(name, "Motion") || strstr(name, "Touchpad") || testbit(key, BTN_TOUCH)) { close(fd); return; }
  struct pad *p = &pads[slot]; memset(p, 0, sizeof *p); p->fd = fd;
  snprintf(p->name, sizeof p->name, "%s", name); snprintf(p->path, sizeof p->path, "%s", path);
  p->legacy = !testbit(key, BTN_SOUTH) && !testbit(key, BTN_EAST);
  struct input_absinfo ai;
  p->xmin = 0; p->xmax = 255; p->ymin = 0; p->ymax = 255;
  if (!ioctl(fd, EVIOCGABS(ABS_X), &ai)) { p->xmin = ai.minimum; p->xmax = ai.maximum; }
  if (!ioctl(fd, EVIOCGABS(ABS_Y), &ai)) { p->ymin = ai.minimum; p->ymax = ai.maximum; }
  int clk = CLOCK_MONOTONIC; ioctl(fd, EVIOCSCLOCKID, &clk);
  fprintf(stderr, "pad%d: %s (%s)%s\npad%d mapping: %s\n", slot, name, path, p->legacy ? " [generic pad layout: X A B Y L R buttons 0-5, Select 8, Start 9]" : "", slot, mapping_line);
}

static void pads_scan(void) {
  DIR *d = opendir("/dev/input"); if (!d) return;
  struct dirent *e; char path[64];
  while ((e = readdir(d))) if (!strncmp(e->d_name, "event", 5)) { snprintf(path, sizeof path, "/dev/input/%s", e->d_name); pad_try(path); }
  closedir(d);
}

static int key_to_retro(const struct pad *p, int code) {
  if (p->legacy && code >= BTN_TRIGGER && code <= BTN_TRIGGER + 15) {
    static const signed char lg[16] = { RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3, -1, -1, -1, -1 };
    return lg[code - BTN_TRIGGER];
  }
  switch (code) {
  case BTN_SOUTH: return RETRO_DEVICE_ID_JOYPAD_B; case BTN_EAST: return RETRO_DEVICE_ID_JOYPAD_A;
  case BTN_NORTH: return RETRO_DEVICE_ID_JOYPAD_X; case BTN_WEST: return RETRO_DEVICE_ID_JOYPAD_Y;
  case BTN_TL: return RETRO_DEVICE_ID_JOYPAD_L; case BTN_TR: return RETRO_DEVICE_ID_JOYPAD_R;
  case BTN_TL2: return RETRO_DEVICE_ID_JOYPAD_L2; case BTN_TR2: return RETRO_DEVICE_ID_JOYPAD_R2;
  case BTN_SELECT: return RETRO_DEVICE_ID_JOYPAD_SELECT; case BTN_START: return RETRO_DEVICE_ID_JOYPAD_START;
  case BTN_THUMBL: return RETRO_DEVICE_ID_JOYPAD_L3; case BTN_THUMBR: return RETRO_DEVICE_ID_JOYPAD_R3;
  case BTN_DPAD_UP: return RETRO_DEVICE_ID_JOYPAD_UP; case BTN_DPAD_DOWN: return RETRO_DEVICE_ID_JOYPAD_DOWN;
  case BTN_DPAD_LEFT: return RETRO_DEVICE_ID_JOYPAD_LEFT; case BTN_DPAD_RIGHT: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
  }
  return -1;
}

/* direction bits kept apart from the buttons so stick, hat and d-pad buttons can overlap */
#define DIRB(id) (1u << (id))

static void pads_poll(void) {
  double t = now();
  if (t - t_rescan > 1.0 && (pads[0].fd < 0 || pads[1].fd < 0)) { t_rescan = t; pads_scan(); }
  for (int i = 0; i < NPAD; i++) {
    struct pad *p = &pads[i]; if (p->fd < 0) continue;
    struct input_event ev[32]; ssize_t n;
    static unsigned btn[NPAD], hat[NPAD], stk[NPAD];
    while ((n = read(p->fd, ev, sizeof ev)) > 0) {
      for (size_t k = 0; k < (size_t)n / sizeof ev[0]; k++) {
        struct input_event *e = &ev[k]; int on = e->value != 0;
        if (e->type == EV_KEY && e->value != 2) {
          int id = key_to_retro(p, e->code);
          if (id >= 0) { if (on) btn[i] |= 1u << id; else btn[i] &= ~(1u << id);
            if (on && lat_ev_t == 0 && i == 0) lat_ev_t = e->time.tv_sec + e->time.tv_usec * 1e-6; }
        } else if (e->type == EV_ABS) {
          int v = e->value, bits = 0;
          if (e->code == ABS_HAT0X) { hat[i] = (hat[i] & ~(DIRB(6) | DIRB(7))) | (v < 0 ? DIRB(6) : v > 0 ? DIRB(7) : 0); bits = 1; }
          else if (e->code == ABS_HAT0Y) { hat[i] = (hat[i] & ~(DIRB(4) | DIRB(5))) | (v < 0 ? DIRB(4) : v > 0 ? DIRB(5) : 0); bits = 1; }
          else if (e->code == ABS_X) { int c = (p->xmin + p->xmax) / 2, th = (p->xmax - p->xmin) * 3 / 10;
            stk[i] = (stk[i] & ~(DIRB(6) | DIRB(7))) | (v < c - th ? DIRB(6) : v > c + th ? DIRB(7) : 0); bits = 1; }
          else if (e->code == ABS_Y) { int c = (p->ymin + p->ymax) / 2, th = (p->ymax - p->ymin) * 3 / 10;
            stk[i] = (stk[i] & ~(DIRB(4) | DIRB(5))) | (v < c - th ? DIRB(4) : v > c + th ? DIRB(5) : 0); bits = 1; }
          if (bits && on && lat_ev_t == 0 && i == 0 && e->code != ABS_X && e->code != ABS_Y) lat_ev_t = e->time.tv_sec + e->time.tv_usec * 1e-6;
        }
      }
    }
    if (n < 0 && errno != EAGAIN && errno != EINTR) { fprintf(stderr, "pad%d %s gone\n", i, p->name); close(p->fd); p->fd = -1; p->mask = 0; btn[i] = hat[i] = stk[i] = 0; continue; }
    p->mask = (uint16_t)(btn[i] | hat[i] | stk[i]);
  }
  /* quit combo: Select + Start on any pad held 2 s */
  int both = 0;
  for (int i = 0; i < NPAD; i++) if (pads[i].fd >= 0 && (pads[i].mask & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT)) && (pads[i].mask & (1 << RETRO_DEVICE_ID_JOYPAD_START))) both = 1;
  if (!both) t_combo = 0; else if (t_combo == 0) t_combo = t; else if (t - t_combo >= 2.0) quit_req = 1;
}

/* ---------- libretro callbacks ---------- */
static int core_fmt = RETRO_PIXEL_FORMAT_RGB565;
static int g_w, g_h; static double t_in_present_cb;
static int frame_dumped; static double t_start_run;
static uint8_t *cur; static int cur_w, cur_h; static size_t cur_pitch; static int got_frame;

static void dump_ppm(const uint8_t *s, int w, int h, size_t p) {
  FILE *f = fopen(o_ppm, "wb"); if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    uint8_t rgb[3];
    if (core_fmt == RETRO_PIXEL_FORMAT_XRGB8888) { uint32_t v = ((const uint32_t *)(s + y * p))[x]; rgb[0] = v >> 16; rgb[1] = v >> 8; rgb[2] = v; }
    else if (core_fmt == RETRO_PIXEL_FORMAT_RGB565) { uint16_t v = ((const uint16_t *)(s + y * p))[x]; rgb[0] = (v >> 11) << 3; rgb[1] = ((v >> 5) & 63) << 2; rgb[2] = (v & 31) << 3; }
    else { uint16_t v = ((const uint16_t *)(s + y * p))[x]; rgb[0] = ((v >> 10) & 31) << 3; rgb[1] = ((v >> 5) & 31) << 3; rgb[2] = (v & 31) << 3; }
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void cb_video(const void *data, unsigned w, unsigned h, size_t pitch) {
  if (!data || !w || !h) return;          /* duplicate frame: the last picture stays up */
  double t0 = now();
  if (o_ppm && !frame_dumped && t0 - t_start_run > 6) { dump_ppm(data, w, h, pitch); frame_dumped = 1; }
  if (!o_thread) present(data, w, h, pitch);
  else {
    pthread_mutex_lock(&vmx);
    while (vjob) pthread_cond_wait(&vcv, &vmx);      /* the previous frame must be on screen first */
    size_t bytes = (size_t)h * pitch; if (bytes > 4u << 20) bytes = 4u << 20;
    memcpy(stage, data, bytes); sw_ = w; sh_ = h; sp_ = pitch; vjob = 1;
    pthread_cond_broadcast(&vcv); pthread_mutex_unlock(&vmx);
  }
  t_in_present_cb += now() - t0;
  g_w = w; g_h = h;
}

static unsigned frame_no;
static int16_t cb_input1(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if ((dev & 0xff) != RETRO_DEVICE_JOYPAD || port >= NPAD) return 0;
  if (pads[port].fd >= 0 && (pads[port].mask >> id & 1)) return 1;
  if (!o_press || port) return 0;
  double t = fmod(now() - t_start_run, 3.0);
  if (o_B >= 0 && (int)id == o_B) { double u = now() - t_start_run; return u > 0.5 && u < 0.5 + 0.5 * o_N && fmod(u, 0.5) < 0.12; }
  if (id == RETRO_DEVICE_ID_JOYPAD_START && t < 0.15) return 1;
  if (id == RETRO_DEVICE_ID_JOYPAD_A && t > 1.5 && t < 1.65) return 1;
  return 0;
}
static int16_t cb_input(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  if (id == RETRO_DEVICE_ID_JOYPAD_MASK) { int16_t m = 0; for (unsigned i = 0; i < 16; i++) if (cb_input1(port, dev, idx, i)) m |= 1 << i; return m; }
  return cb_input1(port, dev, idx, id);
}
static void cb_poll(void) { pads_poll(); }

static void logcb(enum retro_log_level l, const char *fmt, ...) {
  if (l < RETRO_LOG_WARN) return;
  va_list a; va_start(a, fmt); fputs("core: ", stderr); vfprintf(stderr, fmt, a); va_end(a);
}

static bool cb_env(unsigned cmd, void *d) {
  switch (cmd) {
  case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
  case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)d = true; return true;
  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
    int f = *(int *)d;
    if (f == RETRO_PIXEL_FORMAT_RGB565 || f == RETRO_PIXEL_FORMAT_XRGB8888) { core_fmt = f; fb_bpp = f == RETRO_PIXEL_FORMAT_RGB565 ? 16 : 32; return true; }
    return false; }
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)d = "/root/frl/lean/sys"; return true;
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((struct retro_log_callback *)d)->log = logcb; return true;
  case RETRO_ENVIRONMENT_GET_VARIABLE: {
    struct retro_variable *v = d;
    for (int i = 0; i < o_nvar; i++) if (!strcmp(o_var[i][0], v->key)) { v->value = o_var[i][1]; return true; }
    return false; }
  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)d = false; return true;
  case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)d = 0; return true;
  case RETRO_ENVIRONMENT_SET_VARIABLES: case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
  case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: case RETRO_ENVIRONMENT_SET_GEOMETRY:
  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
  case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
  case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    return true;
  default: return false;
  }
}

static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static void cpu_read(unsigned long long *busy, unsigned long long *tot) {
  FILE *f = fopen("/proc/stat", "r"); char l[256]; int n = 0;
  while (fgets(l, sizeof l, f)) {
    unsigned long long u, ni, s, id, io, ir, so, st;
    if (!strncmp(l, "cpu", 3) && l[3] >= '0' && l[3] <= '9' && sscanf(l + 4, "%llu %llu %llu %llu %llu %llu %llu %llu", &u, &ni, &s, &id, &io, &ir, &so, &st) >= 8) {
      tot[n] = u + ni + s + id + io + ir + so + st; busy[n] = tot[n] - id - io; if (++n == 8) break;
    }
  }
  fclose(f);
}

/* ---------- game picker (-M dir) ---------- */
/* Lists the games under dir (any depth, two levels), draws a big text list on the same DRM path, and
 * runs this program again as the player (-P) for the chosen file. Select+Start in a game returns here. */
struct entry { char path[512], title[96]; const char *sys, *core; };
static struct entry *ents; static int nent;
static char base_dir[400];

static const char *core_for(const char *f, const char **sys) {
  const char *x = strrchr(f, '.'); if (!x) return 0; x++;
  static const char *snes[] = { "sfc", "smc", "swc", "fig", 0 }, *psx[] = { "exe", "cue", "iso", "pbp", "chd", "cbn", "m3u", 0 }, *gb[] = { "gb", "gbc", 0 };
  for (int i = 0; snes[i]; i++) if (!strcasecmp(x, snes[i])) { *sys = "SNES"; return "snes9x2005/snes9x2005_libretro.so"; }
  for (int i = 0; psx[i]; i++) if (!strcasecmp(x, psx[i])) { *sys = "PS1"; return "pcsx_rearmed/pcsx_rearmed_libretro.so"; }
  for (int i = 0; gb[i]; i++) if (!strcasecmp(x, gb[i])) { *sys = "GB"; return "gambatte_libretro.so"; }
  return 0;
}

static void add_entry(const char *dir, const char *name, const char *titles) {
  const char *sys; const char *core = core_for(name, &sys); if (!core) return;
  if (nent % 32 == 0) ents = realloc(ents, (nent + 32) * sizeof *ents);
  struct entry *e = &ents[nent++]; memset(e, 0, sizeof *e);
  snprintf(e->path, sizeof e->path, "%s/%s", dir, name); e->sys = sys; e->core = core;
  /* title: from titles.tsv ("file<TAB>title") if listed, else the file name without extension */
  e->title[0] = 0;
  if (titles) { const char *p = titles; size_t nl = strlen(name);
    while (p && *p) { const char *eol = strchr(p, '\n'); size_t ll = eol ? (size_t)(eol - p) : strlen(p);
      if (ll > nl && !strncmp(p, name, nl) && p[nl] == '\t') { size_t tl = ll - nl - 1; if (tl > sizeof e->title - 1) tl = sizeof e->title - 1; memcpy(e->title, p + nl + 1, tl); e->title[tl] = 0; break; }
      p = eol ? eol + 1 : 0; } }
  if (!e->title[0]) { snprintf(e->title, sizeof e->title, "%s", name); char *x = strrchr(e->title, '.'); if (x) *x = 0;
    for (char *c = e->title; *c; c++) if (*c == '_' || *c == '-') *c = ' '; }
}

static int cmp_ent(const void *a, const void *b) {
  const struct entry *x = a, *y = b; int s = strcmp(x->sys, y->sys); return s ? s : strcasecmp(x->title, y->title);
}

static void scan_games(const char *root) {
  nent = 0; char *titles = 0;
  char tp[600]; snprintf(tp, sizeof tp, "%s/titles.tsv", root);
  FILE *f = fopen(tp, "r"); if (f) { fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); titles = calloc(1, n + 1); if (fread(titles, 1, n, f) != (size_t)n) n = 0; fclose(f); }
  DIR *d = opendir(root); struct dirent *e;
  while (d && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[600]; snprintf(p, sizeof p, "%s/%s", root, e->d_name); struct stat st; if (stat(p, &st)) continue;
    if (S_ISDIR(st.st_mode)) { DIR *d2 = opendir(p); struct dirent *e2; while (d2 && (e2 = readdir(d2))) if (e2->d_name[0] != '.') add_entry(p, e2->d_name, titles); if (d2) closedir(d2); }
    else add_entry(root, e->d_name, titles);
  }
  if (d) closedir(d);
  free(titles);
  qsort(ents, nent, sizeof *ents, cmp_ent);
}

static void drm_close(void) {
  if (drm >= 0) { drmDropMaster(drm); close(drm); drm = -1; }
  memset(big, 0, sizeof big); memset(small, 0, sizeof small); bi = si = 0; pending_flip = 0;
}

static void text(struct fbuf *f, int x, int y, const char *s, int sc, uint16_t col) {
  for (; *s; s++, x += 8 * sc) {
    unsigned char ch = (unsigned char)*s; if (ch > 127) ch = '?';
    for (int r = 0; r < 8; r++) { unsigned bits = (unsigned char)font8x8_basic[ch][r];
      for (int c = 0; c < 8; c++) if (bits >> c & 1) {
        for (int j = 0; j < sc; j++) { uint16_t *d = (uint16_t *)(f->map + (size_t)(y + r * sc + j) * f->pitch) + x + c * sc;
          if (x + c * sc + sc <= f->w && y + r * sc + j < f->h) for (int k = 0; k < sc; k++) d[k] = col; } } }
  }
}
static void rect(struct fbuf *f, int x, int y, int w, int h, uint16_t col) {
  for (int j = 0; j < h && y + j < f->h; j++) { uint16_t *d = (uint16_t *)(f->map + (size_t)(y + j) * f->pitch) + x; for (int i = 0; i < w && x + i < f->w; i++) d[i] = col; }
}
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

static void menu_draw(int sel, int top, int rows) {
  struct fbuf *f = &big[bi ^ 1]; int sc = f->w >= 1280 ? 4 : 2, lh = 8 * sc + (sc == 4 ? 14 : 8), x0 = f->w / 16;
  rect(f, 0, 0, f->w, f->h, RGB(24, 10, 10));
  rect(f, 0, 0, f->w, 8 * sc + 40, RGB(150, 40, 20));
  text(f, x0, 20, "FOX RED LABS  -  CHOOSE A GAME", sc, RGB(255, 240, 220));
  int y = 8 * sc + 70;
  if (!nent) text(f, x0, y, "No games in the games folder.", sc, RGB(255, 200, 120));
  for (int i = 0; i < rows && top + i < nent; i++) {
    int n = top + i; struct entry *e = &ents[n]; char line[128];
    if (n == sel) rect(f, x0 - 16, y - 6, f->w - 2 * x0 + 32, lh, RGB(200, 70, 30));
    snprintf(line, sizeof line, "%-5s %.*s", e->sys, (int)((f->w - 2 * x0) / (8 * sc)) - 7, e->title);
    text(f, x0, y, line, sc, n == sel ? RGB(255, 255, 255) : RGB(230, 200, 170));
    y += lh;
  }
  if (nent > rows) { char pg[40]; snprintf(pg, sizeof pg, "%d / %d", sel + 1, nent); text(f, f->w - x0 - (int)strlen(pg) * 8 * sc, 20, pg, sc, RGB(255, 240, 220)); }
  char foot[160];
  if (pads[0].fd >= 0) snprintf(foot, sizeof foot, "Pad: %.40s   D-pad: move   Cross/Circle/Start: play   in game: Select+Start 2 s = back", pads[0].name);
  else snprintf(foot, sizeof foot, "No gamepad found: plug one in (USB, or pair Bluetooth)");
  text(f, x0, f->h - 8 * (sc / 2 > 0 ? sc / 2 : 1) - 24, foot, sc >= 4 ? 2 : 1, pads[0].fd >= 0 ? RGB(200, 170, 140) : RGB(255, 210, 90));
  wait_flip();
  if (drmModePageFlip(drm, crtc_id, f->id, DRM_MODE_PAGE_FLIP_EVENT, 0)) perror("PageFlip"); else pending_flip = 1;
  wait_flip(); bi ^= 1;
}

static void run_game(const struct entry *e) {
  char core[512]; snprintf(core, sizeof core, "%s/%s", base_dir, e->core);
  drm_close();
  pid_t pid = fork();
  if (pid == 0) {
    char self[512]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); self[n > 0 ? n : 0] = 0;
    for (int i = 0; i < NPAD; i++) if (pads[i].fd >= 0) close(pads[i].fd);
    execl(self, self, "-c", core, "-g", e->path, "-m", "auto", "-a", "-P", "-L", (char *)0);
    _exit(127);
  }
  int st; if (pid > 0) waitpid(pid, &st, 0);
  fprintf(stderr, "menu: %s exited\n", e->title);
  for (int i = 0; i < NPAD; i++) if (pads[i].fd >= 0) { close(pads[i].fd); pads[i].fd = -1; }   /* reopen with clean state */
}

__attribute__((noreturn)) static void menu_main(void) {
  { char self[400]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); self[n > 0 ? n : 0] = 0; char *s = strrchr(self, '/'); if (s) *s = 0; snprintf(base_dir, sizeof base_dir, "%s", getenv("LEAN_DIR") ? getenv("LEAN_DIR") : self); }
  o_auto = 1; int sel = 0, top = 0; double hold_t = 0; unsigned prev = 0; int need_release = 1;
  for (;;) {
    scan_games(o_menu);
    if (sel >= nent) sel = nent ? nent - 1 : 0;
    if (drm_open()) { sleep(2); continue; }
    fb_bpp = 16; drm_alloc(0, 0);
    int sc = big[0].w >= 1280 ? 4 : 2, lh = 8 * sc + (sc == 4 ? 14 : 8), rows = (big[0].h - (8 * sc + 70) - 70) / lh;
    int dirty = 1, launch = 0; int npad = -1;
    while (!launch) {
      pads_poll();
      int cnt = (pads[0].fd >= 0) + (pads[1].fd >= 0); if (cnt != npad) { npad = cnt; dirty = 1; }
      unsigned m = pads[0].mask | pads[1].mask; double t = now();
      if (need_release) { if (!m) need_release = 0; prev = m; usleep(16000); continue; }
      unsigned press = m & ~prev; int move = 0;
      if (press & (1u << RETRO_DEVICE_ID_JOYPAD_DOWN)) { move = 1; hold_t = t + 0.35; }
      if (press & (1u << RETRO_DEVICE_ID_JOYPAD_UP)) { move = -1; hold_t = t + 0.35; }
      if (press & (1u << RETRO_DEVICE_ID_JOYPAD_R)) move = rows;
      if (press & (1u << RETRO_DEVICE_ID_JOYPAD_L)) move = -rows;
      if (!move && t > hold_t && hold_t > 0) {
        if (m & (1u << RETRO_DEVICE_ID_JOYPAD_DOWN)) { move = 1; hold_t = t + 0.07; } else if (m & (1u << RETRO_DEVICE_ID_JOYPAD_UP)) { move = -1; hold_t = t + 0.07; } else hold_t = 0;
      }
      if (move && nent) { sel += move; if (sel < 0) sel = move == -1 ? nent - 1 : 0; if (sel >= nent) sel = move == 1 ? 0 : nent - 1; dirty = 1; }
      if (sel < top) top = sel;
      if (sel >= top + rows) top = sel - rows + 1;
      if (nent && (press & ((1u << RETRO_DEVICE_ID_JOYPAD_B) | (1u << RETRO_DEVICE_ID_JOYPAD_A) | (1u << RETRO_DEVICE_ID_JOYPAD_START)))) launch = 1;
      prev = m;
      if (dirty) { menu_draw(sel, top, rows); dirty = 0; }
      usleep(12000);
    }
    fprintf(stderr, "menu: starting %s (%s, %s)\n", ents[sel].title, ents[sel].sys, ents[sel].path);
    run_game(&ents[sel]); need_release = 1; prev = 0;
  }
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "c:g:m:s:tanpd:D:A:V:B:N:W:T:PLM:")) != -1) switch (c) {
    case 'c': o_core = optarg; break; case 'g': o_game = optarg; break;
    case 'm': if (!strcmp(optarg, "auto")) o_auto = 1; else sscanf(optarg, "%dx%d", &o_w, &o_h); break;
    case 's': o_hw = !strcmp(optarg, "hw"); break;
    case 't': o_thread = 1; break; case 'a': o_audio = 1; break; case 'n': o_nosync = 1; break; case 'p': o_press = 1; break;
    case 'B': o_B = atoi(optarg); break; case 'N': o_N = atoi(optarg); break; case 'W': o_warm = atof(optarg); break; case 'T': o_dumpt = atof(optarg); break;
    case 'P': o_play = 1; break; case 'L': o_lat = 1; break; case 'M': o_menu = optarg; break;
    case 'd': o_secs = atof(optarg); break; case 'D': o_ppm = optarg; break; case 'A': o_adev = optarg; break;
    case 'V': { char *e = strchr(optarg, '='); if (e && o_nvar < 32) { *e = 0; o_var[o_nvar][0] = optarg; o_var[o_nvar++][1] = e + 1; } break; }
    default: return 2; }
  if (o_menu) menu_main();
  if (o_play) { o_secs = 1e9; pads_scan(); }
  if (!o_core || !o_game) { fprintf(stderr, "need -c and -g\n"); return 2; }
  void *h = dlopen(o_core, RTLD_NOW | RTLD_LOCAL);
  if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
#define SYM(n) __typeof__(&n) n##_ = (__typeof__(&n))dlsym(h, #n); if (!n##_) { fprintf(stderr, "no %s\n", #n); return 1; }
  SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample) SYM(retro_set_audio_sample_batch)
  SYM(retro_set_input_poll) SYM(retro_set_input_state) SYM(retro_init) SYM(retro_load_game) SYM(retro_run)
  SYM(retro_get_system_av_info) SYM(retro_get_system_info) SYM(retro_deinit) SYM(retro_unload_game)
  retro_set_environment_(cb_env); retro_init_();
  retro_set_video_refresh_(cb_video); retro_set_audio_sample_(cb_audio); retro_set_audio_sample_batch_(cb_audio_batch);
  retro_set_input_poll_(cb_poll); retro_set_input_state_(cb_input);
  struct retro_system_info si_; memset(&si_, 0, sizeof si_); retro_get_system_info_(&si_);
  struct retro_game_info gi = { .path = o_game };
  void *rom = 0;
  if (!si_.need_fullpath) {
    FILE *f = fopen(o_game, "rb"); if (!f) { perror(o_game); return 1; }
    fseek(f, 0, SEEK_END); gi.size = ftell(f); fseek(f, 0, SEEK_SET); rom = malloc(gi.size); if (fread(rom, 1, gi.size, f) != gi.size) return 1; fclose(f); gi.data = rom;
  }
  mkdir("/root/frl/lean/sys", 0755);
  if (!retro_load_game_(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  { void (*sp)(unsigned, unsigned) = (void (*)(unsigned, unsigned))dlsym(h, "retro_set_controller_port_device"); if (sp) { sp(0, RETRO_DEVICE_JOYPAD); } }
  struct retro_system_av_info av; memset(&av, 0, sizeof av); retro_get_system_av_info_(&av);
  a_rate = av.timing.sample_rate;
  fprintf(stderr, "core %s %s: base %ux%u max %ux%u fps %.3f audio %.0f Hz fmt %s\n", si_.library_name, si_.library_version,
          av.geometry.base_width, av.geometry.base_height, av.geometry.max_width, av.geometry.max_height, av.timing.fps, a_rate,
          core_fmt == RETRO_PIXEL_FORMAT_RGB565 ? "RGB565" : "XRGB8888");
  if (drm_open()) return 1;
  vb_interval = 1.0 / ((double)mode.clock * 1000 / (mode.htotal * mode.vtotal));
  drm_alloc(av.geometry.max_width, av.geometry.max_height);
  linebuf = malloc(mode.hdisplay * 4 + 64);
  stage = malloc(4u << 20);
  if (o_thread) pthread_create(&vth, 0, vthread, 0);
  if (o_audio) pthread_create(&ath, 0, athread, 0);

  double *rt = malloc(sizeof(double) * 400000); long nr = 0;
  unsigned long long b0[8] = {0}, t0c[8] = {0}, b1[8] = {0}, t1c[8] = {0};
  t_start_run = now(); double tm0 = 0, tmend; int warmed = 0;
  long fr0 = 0; double sc0 = 0, wa0 = 0, pc0 = 0; long vm0 = 0, vt0 = 0, ad0 = 0, ax0 = 0, np0 = 0;
  double sum_run = 0, maxrun = 0;
  for (;;) {
    double t = now();
    if (!warmed && t - t_start_run >= o_warm) {
      warmed = 1; tm0 = t; nr = 0; sum_run = 0; maxrun = 0; cpu_read(b0, t0c);
      sc0 = t_scale_acc; wa0 = t_wait_acc; pc0 = t_in_present_cb; vm0 = vb_miss; vt0 = vb_total; ad0 = a_drop; ax0 = a_xrun; np0 = n_pres; fr0 = frame_no;
    }
    if (warmed && t - tm0 >= o_secs) break;
    if (o_lat && lat_n >= 50 && t - t_lat_print > 30) { t_lat_print = t; fprintf(stderr, "LATENCY pad_event_to_flip n=%ld avg_ms=%.1f min_ms=%.1f max_ms=%.1f\n", lat_n, lat_sum / lat_n * 1000, lat_min * 1000, lat_max * 1000); }
    if (quit_req) { fprintf(stderr, "quit combo\n"); break; }
    double pc_before = t_in_present_cb;
    retro_run_(); frame_no++;
    double dt = now() - t - (t_in_present_cb - pc_before);   /* core time, without the video callback */
    if (warmed && nr < 400000) { rt[nr++] = dt; sum_run += dt; if (dt > maxrun) maxrun = dt; }
  }
  tmend = now(); cpu_read(b1, t1c);
  if (o_thread) { pthread_mutex_lock(&vmx); while (vjob) pthread_cond_wait(&vcv, &vmx); vquit = 1; pthread_cond_broadcast(&vcv); pthread_mutex_unlock(&vmx); pthread_join(vth, 0); }
  aquit = 1; if (o_audio) pthread_join(ath, 0);
  double el = tmend - tm0; long frames = frame_no - fr0;
  qsort(rt, nr, sizeof(double), cmp_d);
  double scale_ms = (t_scale_acc - sc0) / (n_pres - np0 ? n_pres - np0 : 1) * 1000;
  printf("RESULT core=%s mode=%dx%d scale=%s k=%d fmt=%d thread=%d audio=%d sync=%d secs=%.1f frames=%ld fps=%.2f core_ms_avg=%.2f core_ms_p95=%.2f core_ms_max=%.2f scale_ms=%.2f vb_miss=%ld/%ld a_drop=%ld a_xrun=%ld a_frames=%ld cpu0=%.0f cpu1=%.0f size=%dx%d\n",
         si_.library_name, mode.hdisplay, mode.vdisplay, o_hw ? "hw" : "sw", last_k, fb_bpp, o_thread, o_audio, !o_nosync, el, frames, frames / el,
         sum_run / nr * 1000, rt[(long)(nr * 0.95)] * 1000, maxrun * 1000, scale_ms, vb_miss - vm0, vb_total - vt0, a_drop - ad0, a_xrun - ax0, a_frames,
         100.0 * (b1[0] - b0[0]) / (t1c[0] - t0c[0]), 100.0 * (b1[1] - b0[1]) / (t1c[1] - t0c[1]), g_w, g_h);
  if (o_lat && lat_n) printf("LATENCY pad_event_to_flip n=%ld avg_ms=%.1f min_ms=%.1f max_ms=%.1f\n", lat_n, lat_sum / lat_n * 1000, lat_min * 1000, lat_max * 1000);
  retro_unload_game_(); retro_deinit_();
  return 0;
}
