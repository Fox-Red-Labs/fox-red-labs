/* frl_gamepad.c: see frl_gamepad.h. Order of every byte follows BleGamepad.cpp. */
#include <string.h>
#include "frl_gamepad.h"

#define LOW(v)  ((uint8_t)((uint16_t)(v) & 0xFF))
#define HIGH(v) ((uint8_t)(((uint16_t)(v) >> 8) & 0xFF))

void frl_gp_config_default(frl_gp_config_t *c)
{
    int i;
    memset(c, 0, sizeof(*c));
    c->controllerType = 0x05;       /* CONTROLLER_TYPE_GAMEPAD */
    c->hidReportId = 3;
    c->buttonCount = 16;
    c->hatSwitchCount = 1;
    for (i = 0; i < FRL_GP_POSSIBLE_AXES; i++) c->whichAxes[i] = 1;
    c->axesMin = (int16_t)0x8000;
    c->axesMax = 0x7FFF;
    c->simMin = 0x0000;
    c->simMax = 0x7FFF;
    c->motionMin = 0x0000;
    c->motionMax = 0x7FFF;
#if defined(FRL_GP_START_SELECT) && FRL_GP_START_SELECT
    c->whichSpecial[FRL_START_BUTTON] = 1;
    c->whichSpecial[FRL_SELECT_BUTTON] = 1;
#endif
}

static uint8_t count(const uint8_t *a, int from, int to)
{
    int i, n = 0;
    for (i = from; i < to; i++) n += a[i] ? 1 : 0;
    return (uint8_t)n;
}

void frl_gp_resetButtons(frl_gp_t *g) { memset(g->buttons, 0, sizeof(g->buttons)); }

/* Descriptor writer: bounds-checked (the library only checks after the fact). */
typedef struct { frl_gp_t *g; int over; } W;
static void E1(W *w, uint8_t a)
{
    if (w->g->descLen + 1 > FRL_GP_DESC_MAX) { w->over = 1; return; }
    w->g->desc[w->g->descLen++] = a;
}
static void E2(W *w, uint8_t a, uint8_t b) { E1(w, a); E1(w, b); }
static void E3(W *w, uint8_t a, uint8_t b, uint8_t c) { E1(w, a); E1(w, b); E1(w, c); }

static void build_generic(frl_gp_t *g)
{
    const frl_gp_config_t *c = &g->cfg;
    W w = { g, 0 }, *p = &w;
    uint8_t sp = count(c->whichSpecial, 0, 8);
    uint8_t spDesktop = count(c->whichSpecial, 0, 3);
    uint8_t spConsumer = count(c->whichSpecial, 3, 8);
    uint8_t axisCount = count(c->whichAxes, 0, 8);
    uint8_t simCount = count(c->whichSim, 0, 5);
    int h;

    E2(p, 0x05, 0x01);                  /* USAGE_PAGE (Generic Desktop) */
    E2(p, 0x09, c->controllerType);     /* USAGE (gamepad) */
    E2(p, 0xa1, 0x01);                  /* COLLECTION (Application) */
    E2(p, 0x85, c->hidReportId);        /* REPORT_ID */

    if (c->buttonCount > 0) {
        E2(p, 0x05, 0x09);              /* USAGE_PAGE (Button) */
        E2(p, 0x15, 0x00);
        E2(p, 0x25, 0x01);
        E2(p, 0x75, 0x01);
        E2(p, 0x19, 0x01);              /* USAGE_MINIMUM */
        E2(p, 0x29, (uint8_t)c->buttonCount);
        E2(p, 0x95, (uint8_t)c->buttonCount);
        E2(p, 0x81, 0x02);
        if (g->buttonPad > 0) {
            E2(p, 0x75, 0x01);
            E2(p, 0x95, g->buttonPad);
            E2(p, 0x81, 0x03);
        }
    }

    if (sp > 0) {
        E2(p, 0x15, 0x00);
        E2(p, 0x25, 0x01);
        E2(p, 0x75, 0x01);
        if (spDesktop > 0) {
            E2(p, 0x05, 0x01);
            E2(p, 0x95, spDesktop);
            if (c->whichSpecial[0]) E2(p, 0x09, 0x3D);   /* Start */
            if (c->whichSpecial[1]) E2(p, 0x09, 0x3E);   /* Select */
            if (c->whichSpecial[2]) E2(p, 0x09, 0x86);   /* App Menu */
            E2(p, 0x81, 0x02);
        }
        if (spConsumer > 0) {
            E2(p, 0x05, 0x0C);
            E2(p, 0x95, spConsumer);
            if (c->whichSpecial[3]) E3(p, 0x0A, 0x23, 0x02);   /* Home */
            if (c->whichSpecial[4]) E3(p, 0x0A, 0x24, 0x02);   /* Back */
            if (c->whichSpecial[5]) E2(p, 0x09, 0xE9);         /* Volume Inc */
            if (c->whichSpecial[6]) E2(p, 0x09, 0xEA);         /* Volume Dec */
            if (c->whichSpecial[7]) E2(p, 0x09, 0xE2);         /* Mute */
            E2(p, 0x81, 0x02);
        }
        if (g->specialPad > 0) {
            E2(p, 0x75, 0x01);
            E2(p, 0x95, g->specialPad);
            E2(p, 0x81, 0x03);
        }
    }

    if (axisCount > 0) {
        uint8_t sliders = (uint8_t)((c->whichAxes[6] ? 1 : 0) + (c->whichAxes[7] ? 1 : 0));
        E2(p, 0x05, 0x01);
        E3(p, 0x16, LOW(c->axesMin), HIGH(c->axesMin));
        E3(p, 0x26, LOW(c->axesMax), HIGH(c->axesMax));
        E2(p, 0x75, 0x10);
        E2(p, 0x95, (uint8_t)(axisCount - sliders));
        E2(p, 0xA1, 0x00);              /* COLLECTION (Physical) */
        if (c->whichAxes[0]) E2(p, 0x09, 0x30);   /* X  */
        if (c->whichAxes[1]) E2(p, 0x09, 0x31);   /* Y  */
        if (c->whichAxes[2]) E2(p, 0x09, 0x32);   /* Z  */
        if (c->whichAxes[5]) E2(p, 0x09, 0x35);   /* Rz */
        if (c->whichAxes[3]) E2(p, 0x09, 0x33);   /* Rx */
        if (c->whichAxes[4]) E2(p, 0x09, 0x34);   /* Ry */
        E2(p, 0x81, 0x02);
        if (c->whichAxes[6]) {
            E2(p, 0x95, 0x01);
            E2(p, 0xA1, 0x02);          /* COLLECTION (Logical) */
            E2(p, 0x09, 0x36);          /* Slider */
            E2(p, 0x81, 0x02);
            E1(p, 0xC0);
        }
        if (c->whichAxes[7]) {
            E2(p, 0x95, 0x01);
            E2(p, 0xA1, 0x02);
            E2(p, 0x09, 0x36);
            E2(p, 0x81, 0x02);
            E1(p, 0xC0);
        }
        E1(p, 0xc0);                    /* END_COLLECTION (Physical) */
    }

    if (simCount > 0) {
        E2(p, 0x05, 0x02);              /* Simulation Controls */
        E3(p, 0x16, LOW(c->simMin), HIGH(c->simMin));
        E3(p, 0x26, LOW(c->simMax), HIGH(c->simMax));
        E2(p, 0x75, 0x10);
        E2(p, 0x95, simCount);
        E2(p, 0xA1, 0x00);
        if (c->whichSim[0]) E2(p, 0x09, 0xBA);
        if (c->whichSim[1]) E2(p, 0x09, 0xBB);
        if (c->whichSim[2]) E2(p, 0x09, 0xC4);
        if (c->whichSim[3]) E2(p, 0x09, 0xC5);
        if (c->whichSim[4]) E2(p, 0x09, 0xC8);
        E2(p, 0x81, 0x02);
        E1(p, 0xc0);
    }

    if (c->includeGyro) {
        E2(p, 0xA1, 0x00);
        E2(p, 0x05, 0x01);
        E2(p, 0x09, 0x33); E2(p, 0x09, 0x34); E2(p, 0x09, 0x35);
        E3(p, 0x16, LOW(c->motionMin), HIGH(c->motionMin));
        E3(p, 0x26, LOW(c->motionMax), HIGH(c->motionMax));
        E2(p, 0x75, 0x10);
        E2(p, 0x95, 0x03);
        E2(p, 0x81, 0x02);
        E1(p, 0xc0);
    }
    if (c->includeAccel) {
        E2(p, 0xA1, 0x00);
        E2(p, 0x05, 0x01);
        E2(p, 0x09, 0x40); E2(p, 0x09, 0x41); E2(p, 0x09, 0x42);
        E3(p, 0x16, LOW(c->motionMin), HIGH(c->motionMin));
        E3(p, 0x26, LOW(c->motionMax), HIGH(c->motionMax));
        E2(p, 0x75, 0x10);
        E2(p, 0x95, 0x03);
        E2(p, 0x81, 0x02);
        E1(p, 0xc0);
    }

    if (c->hatSwitchCount > 0) {
        E2(p, 0xA1, 0x00);
        E2(p, 0x05, 0x01);              /* USAGE_PAGE(1) */
        for (h = 0; h < c->hatSwitchCount; h++) E2(p, 0x09, 0x39);   /* Hat Switch */
        E2(p, 0x15, 0x01);              /* Logical Min (1) */
        E2(p, 0x25, 0x08);              /* Logical Max (8) */
        E2(p, 0x35, 0x00);              /* Physical Min (0) */
        E3(p, 0x46, 0x3B, 0x01);        /* Physical Max (315) */
        E2(p, 0x65, 0x12);              /* Unit */
        E2(p, 0x75, 0x08);
        E2(p, 0x95, c->hatSwitchCount);
        E2(p, 0x81, 0x42);
        E1(p, 0xc0);
    }

    E1(p, 0xc0);                        /* END_COLLECTION (Application) */
    if (w.over) g->descLen = FRL_GP_DESC_MAX + 1;
}

int frl_gp_begin(frl_gp_t *g, const frl_gp_config_t *cfg)
{
    const frl_gp_config_t *c;
    uint8_t pos = 0, sp, nSpecialBytes, nMotion = 0;
    int i;

    memset(g, 0, sizeof(*g));
    g->cfg = *cfg;
    c = &g->cfg;
    sp = count(c->whichSpecial, 0, 8);

    g->buttonPad = (uint8_t)(8 - (c->buttonCount % 8));
    if (g->buttonPad == 8) g->buttonPad = 0;
    g->specialPad = (uint8_t)(8 - (sp % 8));
    if (g->specialPad == 8) g->specialPad = 0;

    for (i = 0; i < FRL_GP_POSSIBLE_SPECIAL; i++) {
        g->specialPos[i] = pos;
        if (c->whichSpecial[i]) pos++;
    }

    g->numButtonBytes = (uint8_t)(c->buttonCount / 8);
    if (g->buttonPad > 0) g->numButtonBytes++;
    nSpecialBytes = (uint8_t)(sp / 8);
    if (g->specialPad > 0) nSpecialBytes++;
    if (c->includeAccel) nMotion += 6;
    if (c->includeGyro) nMotion += 6;

    g->reportSize = (uint8_t)(g->numButtonBytes + nSpecialBytes
                    + count(c->whichAxes, 0, 8) * 2 + count(c->whichSim, 0, 5) * 2
                    + nMotion + c->hatSwitchCount);

    build_generic(g);
    if (g->descLen > FRL_GP_DESC_MAX || g->reportSize > FRL_GP_REPORT_MAX) return -1;
    return 0;
}

void frl_gp_press(frl_gp_t *g, uint8_t b)
{
    g->buttons[(b - 1) / 8] |= (uint8_t)(1u << ((b - 1) % 8));
}

void frl_gp_release(frl_gp_t *g, uint8_t b)
{
    g->buttons[(b - 1) / 8] &= (uint8_t)~(1u << ((b - 1) % 8));
}

static uint8_t spbit(const frl_gp_t *g, uint8_t b)
{
    return (b >= FRL_GP_POSSIBLE_SPECIAL) ? 0 : g->specialPos[b];
}
static void sp_press(frl_gp_t *g, uint8_t b)   { g->special |= (uint8_t)(1u << (spbit(g, b) % 8)); }
static void sp_release(frl_gp_t *g, uint8_t b) { g->special &= (uint8_t)~(1u << (spbit(g, b) % 8)); }
void frl_gp_pressStart(frl_gp_t *g)     { sp_press(g, FRL_START_BUTTON); }
void frl_gp_releaseStart(frl_gp_t *g)   { sp_release(g, FRL_START_BUTTON); }
void frl_gp_pressSelect(frl_gp_t *g)    { sp_press(g, FRL_SELECT_BUTTON); }
void frl_gp_releaseSelect(frl_gp_t *g)  { sp_release(g, FRL_SELECT_BUTTON); }

void frl_gp_setAxes(frl_gp_t *g, int16_t x, int16_t y, int16_t z,
                    int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2)
{
    /* clampAxis() is the identity at this commit */
    g->x = x; g->y = y; g->z = z; g->rX = rX; g->rY = rY; g->rZ = rZ;
    g->slider1 = s1; g->slider2 = s2;
}

void frl_gp_setHat1(frl_gp_t *g, signed char hat1) { g->hat1 = hat1; }

#define PUT16(v) do { m[i++] = (uint8_t)(v); m[i++] = (uint8_t)((v) >> 8); } while (0)

uint8_t frl_gp_pack(const frl_gp_t *g, uint8_t *m)
{
    const frl_gp_config_t *c = &g->cfg;
    uint8_t i = 0;
    int h;

    memset(m, 0, g->reportSize);
    /* The library memcpy()s all 16 button bytes into the report buffer, then lets
     * later fields overwrite the excess. Copying only the button bytes gives the
     * same bytes whenever axes or hats follow, and cannot overrun a short buffer. */
    memcpy(m, g->buttons, g->numButtonBytes);
    i += g->numButtonBytes;

    if (count(c->whichSpecial, 0, 8) > 0) m[i++] = g->special;

    if (c->whichAxes[0]) PUT16(g->x);
    if (c->whichAxes[1]) PUT16(g->y);
    if (c->whichAxes[2]) PUT16(g->z);
    if (c->whichAxes[5]) PUT16(g->rZ);
    if (c->whichAxes[3]) PUT16(g->rX);
    if (c->whichAxes[4]) PUT16(g->rY);
    if (c->whichAxes[6]) PUT16(g->slider1);
    if (c->whichAxes[7]) PUT16(g->slider2);

    if (c->whichSim[0]) PUT16(g->rudder);
    if (c->whichSim[1]) PUT16(g->throttle);
    if (c->whichSim[2]) PUT16(g->accelerator);
    if (c->whichSim[3]) PUT16(g->brake);
    if (c->whichSim[4]) PUT16(g->steering);

    if (c->includeGyro)  { PUT16(g->gX); PUT16(g->gY); PUT16(g->gZ); }
    if (c->includeAccel) { PUT16(g->aX); PUT16(g->aY); PUT16(g->aZ); }

    if (c->hatSwitchCount > 0) {
        signed char hats[4];
        hats[0] = g->hat1; hats[1] = g->hat2; hats[2] = g->hat3; hats[3] = g->hat4;
        for (h = c->hatSwitchCount - 1; h >= 0; h--) m[i++] = (uint8_t)hats[h];
    }
    return g->reportSize;
}
