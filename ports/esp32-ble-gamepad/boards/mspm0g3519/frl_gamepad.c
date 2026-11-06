#include "frl_gamepad.h"
#include <string.h>

void frl_gp_default_config(frl_gp_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->controllerType = 0x05;
    c->hidReportId = 3;
    c->buttonCount = 16;
    c->hatSwitchCount = 1;
    for (int i = 0; i < 8; i++) c->whichAxes[i] = 1;
    c->axesMin = (int16_t)0x8000;
    c->axesMax = 0x7FFF;
#if defined(FRL_GP_START_SELECT) && FRL_GP_START_SELECT
    c->whichSpecial[START_BUTTON] = 1;
    c->whichSpecial[SELECT_BUTTON] = 1;
#endif
}

static uint8_t count_true(const uint8_t *a, int from, int to)
{
    uint8_t n = 0;
    for (int i = from; i < to; i++) n += a[i] ? 1 : 0;
    return n;
}

void frl_gp_init(frl_gp_t *g, const frl_gp_config_t *c)
{
    memset(g, 0, sizeof(*g));
    g->cfg = *c;
    uint8_t special = count_true(c->whichSpecial, 0, 8);
    g->buttonPad = 8 - (c->buttonCount % 8);
    if (g->buttonPad == 8) g->buttonPad = 0;
    g->specialPad = 8 - (special % 8);
    if (g->specialPad == 8) g->specialPad = 0;
    uint8_t pos = 0;
    for (int i = 0; i < 8; i++) {
        g->specialPos[i] = pos;
        if (c->whichSpecial[i]) pos++;
    }
    g->buttonBytes = c->buttonCount / 8;
    if (g->buttonPad > 0) g->buttonBytes++;
    uint8_t specialBytes = special / 8;
    if (g->specialPad > 0) specialBytes++;
    g->reportSize = g->buttonBytes + specialBytes + count_true(c->whichAxes, 0, 8) * 2 + c->hatSwitchCount;
}

typedef struct { uint8_t *p; size_t n, cap; int over; } wr_t;
static void put(wr_t *w, uint8_t b)
{
    if (w->n < w->cap) w->p[w->n] = b; else w->over = 1;
    w->n++;
}

size_t frl_gp_build_descriptor(const frl_gp_t *g, uint8_t *out, size_t cap)
{
    const frl_gp_config_t *c = &g->cfg;
    wr_t w = { out, 0, cap, 0 };
    const uint8_t *ax = c->whichAxes;
    uint8_t special = count_true(c->whichSpecial, 0, 8);
    uint8_t desktop = count_true(c->whichSpecial, 0, 3);
    uint8_t consumer = count_true(c->whichSpecial, 3, 8);
    uint8_t axisCount = count_true(ax, 0, 8);

    put(&w, 0x05); put(&w, 0x01);                 /* USAGE_PAGE (Generic Desktop) */
    put(&w, 0x09); put(&w, c->controllerType);    /* USAGE */
    put(&w, 0xa1); put(&w, 0x01);                 /* COLLECTION (Application) */
    put(&w, 0x85); put(&w, c->hidReportId);       /* REPORT_ID */

    if (c->buttonCount > 0) {
        put(&w, 0x05); put(&w, 0x09);
        put(&w, 0x15); put(&w, 0x00);
        put(&w, 0x25); put(&w, 0x01);
        put(&w, 0x75); put(&w, 0x01);
        put(&w, 0x19); put(&w, 0x01);
        put(&w, 0x29); put(&w, (uint8_t)c->buttonCount);
        put(&w, 0x95); put(&w, (uint8_t)c->buttonCount);
        put(&w, 0x81); put(&w, 0x02);
        if (g->buttonPad > 0) {
            put(&w, 0x75); put(&w, 0x01);
            put(&w, 0x95); put(&w, g->buttonPad);
            put(&w, 0x81); put(&w, 0x03);
        }
    }

    if (special > 0) {
        put(&w, 0x15); put(&w, 0x00);
        put(&w, 0x25); put(&w, 0x01);
        put(&w, 0x75); put(&w, 0x01);
        if (desktop > 0) {
            put(&w, 0x05); put(&w, 0x01);
            put(&w, 0x95); put(&w, desktop);
            if (c->whichSpecial[START_BUTTON]) { put(&w, 0x09); put(&w, 0x3D); }
            if (c->whichSpecial[SELECT_BUTTON]) { put(&w, 0x09); put(&w, 0x3E); }
            if (c->whichSpecial[MENU_BUTTON]) { put(&w, 0x09); put(&w, 0x86); }
            put(&w, 0x81); put(&w, 0x02);
        }
        if (consumer > 0) {
            put(&w, 0x05); put(&w, 0x0C);
            put(&w, 0x95); put(&w, consumer);
            if (c->whichSpecial[HOME_BUTTON]) { put(&w, 0x0A); put(&w, 0x23); put(&w, 0x02); }
            if (c->whichSpecial[BACK_BUTTON]) { put(&w, 0x0A); put(&w, 0x24); put(&w, 0x02); }
            if (c->whichSpecial[VOLUME_INC_BUTTON]) { put(&w, 0x09); put(&w, 0xE9); }
            if (c->whichSpecial[VOLUME_DEC_BUTTON]) { put(&w, 0x09); put(&w, 0xEA); }
            if (c->whichSpecial[VOLUME_MUTE_BUTTON]) { put(&w, 0x09); put(&w, 0xE2); }
            put(&w, 0x81); put(&w, 0x02);
        }
        if (g->specialPad > 0) {
            put(&w, 0x75); put(&w, 0x01);
            put(&w, 0x95); put(&w, g->specialPad);
            put(&w, 0x81); put(&w, 0x03);
        }
    }

    if (axisCount > 0) {
        uint8_t sliders = (ax[6] ? 1 : 0) + (ax[7] ? 1 : 0);
        put(&w, 0x05); put(&w, 0x01);
        put(&w, 0x16); put(&w, (uint8_t)c->axesMin); put(&w, (uint8_t)((uint16_t)c->axesMin >> 8));
        put(&w, 0x26); put(&w, (uint8_t)c->axesMax); put(&w, (uint8_t)((uint16_t)c->axesMax >> 8));
        put(&w, 0x75); put(&w, 0x10);
        put(&w, 0x95); put(&w, axisCount - sliders);
        put(&w, 0xA1); put(&w, 0x00);
        if (ax[0]) { put(&w, 0x09); put(&w, 0x30); } /* X */
        if (ax[1]) { put(&w, 0x09); put(&w, 0x31); } /* Y */
        if (ax[2]) { put(&w, 0x09); put(&w, 0x32); } /* Z */
        if (ax[5]) { put(&w, 0x09); put(&w, 0x35); } /* Rz */
        if (ax[3]) { put(&w, 0x09); put(&w, 0x33); } /* Rx */
        if (ax[4]) { put(&w, 0x09); put(&w, 0x34); } /* Ry */
        put(&w, 0x81); put(&w, 0x02);
        for (int s = 6; s < 8; s++) {
            if (!ax[s]) continue;
            put(&w, 0x95); put(&w, 0x01);
            put(&w, 0xA1); put(&w, 0x02);
            put(&w, 0x09); put(&w, 0x36);
            put(&w, 0x81); put(&w, 0x02);
            put(&w, 0xC0);
        }
        put(&w, 0xc0);
    }

    if (c->hatSwitchCount > 0) {
        put(&w, 0xA1); put(&w, 0x00);
        put(&w, 0x05); put(&w, 0x01);
        for (int i = 0; i < c->hatSwitchCount; i++) { put(&w, 0x09); put(&w, 0x39); }
        put(&w, 0x15); put(&w, 0x01);
        put(&w, 0x25); put(&w, 0x08);
        put(&w, 0x35); put(&w, 0x00);
        put(&w, 0x46); put(&w, 0x3B); put(&w, 0x01);
        put(&w, 0x65); put(&w, 0x12);
        put(&w, 0x75); put(&w, 0x08);
        put(&w, 0x95); put(&w, c->hatSwitchCount);
        put(&w, 0x81); put(&w, 0x42);
        put(&w, 0xc0);
    }

    put(&w, 0xc0); /* END_COLLECTION (Application) */
    return w.over ? 0 : w.n;
}

static uint8_t *put16(uint8_t *m, int16_t v)
{
    *m++ = (uint8_t)v;
    *m++ = (uint8_t)((uint16_t)v >> 8);
    return m;
}

size_t frl_gp_pack(const frl_gp_t *g, uint8_t *out)
{
    const frl_gp_config_t *c = &g->cfg;
    const uint8_t *ax = c->whichAxes;
    uint8_t *m = out;
    memset(out, 0, g->reportSize);
    memcpy(out, g->buttons, g->reportSize < sizeof(g->buttons) ? g->reportSize : sizeof(g->buttons));
    m += g->buttonBytes;
    if (count_true(c->whichSpecial, 0, 8) > 0) *m++ = g->special;
    if (ax[0]) m = put16(m, g->x);
    if (ax[1]) m = put16(m, g->y);
    if (ax[2]) m = put16(m, g->z);
    if (ax[5]) m = put16(m, g->rZ);
    if (ax[3]) m = put16(m, g->rX);
    if (ax[4]) m = put16(m, g->rY);
    if (ax[6]) m = put16(m, g->slider1);
    if (ax[7]) m = put16(m, g->slider2);
    if (c->hatSwitchCount > 0) {
        /* the library fills hats[0..3] from _hat1.._hat4 and writes them last to first; with one hat
         * that is hat 1. Only hat 1 is ported. */
        for (int i = c->hatSwitchCount - 1; i >= 0; i--) *m++ = (uint8_t)(i == 0 ? g->hat1 : 0);
    }
    return g->reportSize;
}

void frl_gp_resetButtons(frl_gp_t *g) { memset(g->buttons, 0, sizeof(g->buttons)); }

void frl_gp_press(frl_gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] |= (uint8_t)(1 << ((b - 1) % 8)); }
void frl_gp_release(frl_gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] &= (uint8_t)~(1 << ((b - 1) % 8)); }

static uint8_t special_pos(const frl_gp_t *g, uint8_t b) { return b >= 8 ? 0 : g->specialPos[b]; }
static void press_special(frl_gp_t *g, uint8_t b) { g->special |= (uint8_t)(1 << (special_pos(g, b) % 8)); }
static void release_special(frl_gp_t *g, uint8_t b) { g->special &= (uint8_t)~(1 << (special_pos(g, b) % 8)); }
void frl_gp_pressStart(frl_gp_t *g) { press_special(g, START_BUTTON); }
void frl_gp_releaseStart(frl_gp_t *g) { release_special(g, START_BUTTON); }
void frl_gp_pressSelect(frl_gp_t *g) { press_special(g, SELECT_BUTTON); }

void frl_gp_setAxes(frl_gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ,
                    int16_t s1, int16_t s2)
{
    g->x = x; g->y = y; g->z = z; g->rX = rX; g->rY = rY; g->rZ = rZ; g->slider1 = s1; g->slider2 = s2;
}

void frl_gp_setHat1(frl_gp_t *g, int8_t hat1) { g->hat1 = hat1; }
