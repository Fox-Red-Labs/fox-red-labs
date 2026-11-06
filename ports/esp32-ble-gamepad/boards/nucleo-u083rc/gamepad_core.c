#include "gamepad_core.h"
#include <string.h>

#define lowByte(v) ((uint8_t)((v) & 0xff))
#define highByte(v) ((uint8_t)(((uint16_t)(v)) >> 8))
#define EMIT(b) (g->desc[g->descSize++] = (uint8_t)(b))

void gp_default_config(gp_config_t *c)
{
  memset(c, 0, sizeof(*c));
  c->controllerType = 0x05;   /* CONTROLLER_TYPE_GAMEPAD */
  c->hidReportId = 3;
  c->buttonCount = 16;
  c->hatSwitchCount = 1;
  /* _whichSpecialButtons{false x8} in the library's constructor */
  for (int i = 0; i < 8; i++) c->whichAxes[i] = 1;
  c->axesMin = (int16_t)0x8000;
  c->axesMax = 0x7FFF;
}

static int special_total(const gp_config_t *c) { int n = 0; for (int i = 0; i < 8; i++) n += c->whichSpecial[i] ? 1 : 0; return n; }
static int special_desktop(const gp_config_t *c) { int n = 0; for (int i = 0; i < 3; i++) n += c->whichSpecial[i] ? 1 : 0; return n; }
static int special_consumer(const gp_config_t *c) { int n = 0; for (int i = 3; i < 8; i++) n += c->whichSpecial[i] ? 1 : 0; return n; }
static int axis_count(const gp_config_t *c) { int n = 0; for (int i = 0; i < 8; i++) n += c->whichAxes[i] ? 1 : 0; return n; }

void gp_reset_buttons(gp_t *g) { memset(g->buttons, 0, sizeof(g->buttons)); }

void gp_build_descriptor(gp_t *g)
{
  const gp_config_t *c = &g->cfg;
  g->descSize = 0;
  EMIT(0x05); EMIT(0x01);              /* USAGE_PAGE (Generic Desktop) */
  EMIT(0x09); EMIT(c->controllerType); /* USAGE */
  EMIT(0xa1); EMIT(0x01);              /* COLLECTION (Application) */
  EMIT(0x85); EMIT(c->hidReportId);    /* REPORT_ID */

  if (c->buttonCount > 0) {
    EMIT(0x05); EMIT(0x09);
    EMIT(0x15); EMIT(0x00);
    EMIT(0x25); EMIT(0x01);
    EMIT(0x75); EMIT(0x01);
    EMIT(0x19); EMIT(0x01);
    EMIT(0x29); EMIT(c->buttonCount);
    EMIT(0x95); EMIT(c->buttonCount);
    EMIT(0x81); EMIT(0x02);
    if (g->buttonPad > 0) {
      EMIT(0x75); EMIT(0x01);
      EMIT(0x95); EMIT(g->buttonPad);
      EMIT(0x81); EMIT(0x03);
    }
  }

  if (special_total(c) > 0) {
    EMIT(0x15); EMIT(0x00);
    EMIT(0x25); EMIT(0x01);
    EMIT(0x75); EMIT(0x01);
    if (special_desktop(c) > 0) {
      EMIT(0x05); EMIT(0x01);
      EMIT(0x95); EMIT(special_desktop(c));
      if (c->whichSpecial[0]) { EMIT(0x09); EMIT(0x3D); }
      if (c->whichSpecial[1]) { EMIT(0x09); EMIT(0x3E); }
      if (c->whichSpecial[2]) { EMIT(0x09); EMIT(0x86); }
      EMIT(0x81); EMIT(0x02);
    }
    if (special_consumer(c) > 0) {
      EMIT(0x05); EMIT(0x0C);
      EMIT(0x95); EMIT(special_consumer(c));
      if (c->whichSpecial[3]) { EMIT(0x0A); EMIT(0x23); EMIT(0x02); }
      if (c->whichSpecial[4]) { EMIT(0x0A); EMIT(0x24); EMIT(0x02); }
      if (c->whichSpecial[5]) { EMIT(0x09); EMIT(0xE9); }
      if (c->whichSpecial[6]) { EMIT(0x09); EMIT(0xEA); }
      if (c->whichSpecial[7]) { EMIT(0x09); EMIT(0xE2); }
      EMIT(0x81); EMIT(0x02);
    }
    if (g->specialPad > 0) {
      EMIT(0x75); EMIT(0x01);
      EMIT(0x95); EMIT(g->specialPad);
      EMIT(0x81); EMIT(0x03);
    }
  }

  if (axis_count(c) > 0) {
    EMIT(0x05); EMIT(0x01);
    EMIT(0x16); EMIT(lowByte(c->axesMin)); EMIT(highByte(c->axesMin));
    EMIT(0x26); EMIT(lowByte(c->axesMax)); EMIT(highByte(c->axesMax));
    EMIT(0x75); EMIT(0x10);
    uint8_t sliders = (c->whichAxes[6] ? 1 : 0) + (c->whichAxes[7] ? 1 : 0);
    EMIT(0x95); EMIT(axis_count(c) - sliders);
    EMIT(0xA1); EMIT(0x00);
    if (c->whichAxes[0]) { EMIT(0x09); EMIT(0x30); } /* X */
    if (c->whichAxes[1]) { EMIT(0x09); EMIT(0x31); } /* Y */
    if (c->whichAxes[2]) { EMIT(0x09); EMIT(0x32); } /* Z */
    if (c->whichAxes[5]) { EMIT(0x09); EMIT(0x35); } /* Rz */
    if (c->whichAxes[3]) { EMIT(0x09); EMIT(0x33); } /* Rx */
    if (c->whichAxes[4]) { EMIT(0x09); EMIT(0x34); } /* Ry */
    EMIT(0x81); EMIT(0x02);
    for (int s = 6; s <= 7; s++) {
      if (c->whichAxes[s]) {
        EMIT(0x95); EMIT(0x01);
        EMIT(0xA1); EMIT(0x02);
        EMIT(0x09); EMIT(0x36);
        EMIT(0x81); EMIT(0x02);
        EMIT(0xC0);
      }
    }
    EMIT(0xc0);
  }

  /* simulation controls, gyroscope, accelerometer: not carried (default config has none) */

  if (c->hatSwitchCount > 0) {
    EMIT(0xA1); EMIT(0x00);
    EMIT(0x05); EMIT(0x01);
    for (int i = 0; i < c->hatSwitchCount; i++) { EMIT(0x09); EMIT(0x39); }
    EMIT(0x15); EMIT(0x01);
    EMIT(0x25); EMIT(0x08);
    EMIT(0x35); EMIT(0x00);
    EMIT(0x46); EMIT(0x3B); EMIT(0x01);
    EMIT(0x65); EMIT(0x12);
    EMIT(0x75); EMIT(0x08);
    EMIT(0x95); EMIT(c->hatSwitchCount);
    EMIT(0x81); EMIT(0x42);
    EMIT(0xc0);
  }

  /* output / feature report blocks: not carried (disabled in the default config) */
  EMIT(0xc0);
}

void gp_begin(gp_t *g, const gp_config_t *c)
{
  memset(g, 0, sizeof(*g));
  g->cfg = *c;
  g->buttonPad = 8 - (c->buttonCount % 8);
  if (g->buttonPad == 8) g->buttonPad = 0;
  g->specialPad = 8 - (special_total(c) % 8);
  if (g->specialPad == 8) g->specialPad = 0;
  uint8_t pos = 0;
  for (int i = 0; i < POSSIBLESPECIALBUTTONS; i++) {
    g->specialPos[i] = pos;
    if (c->whichSpecial[i]) pos++;
  }
  uint8_t axisBytes = axis_count(c) * 2;
  g->numOfButtonBytes = c->buttonCount / 8;
  if (g->buttonPad > 0) g->numOfButtonBytes++;
  uint8_t specialBytes = special_total(c) / 8;
  if (g->specialPad > 0) specialBytes++;
  g->reportSize = g->numOfButtonBytes + specialBytes + axisBytes + c->hatSwitchCount;
  gp_build_descriptor(g);
}

int gp_pack(const gp_t *g, uint8_t *m)
{
  const gp_config_t *c = &g->cfg;
  uint8_t i = 0;
  memset(m, 0, g->reportSize);
  memcpy(m, g->buttons, g->reportSize < 16 ? g->reportSize : 16); /* library copies all 16 button bytes */
  i += g->numOfButtonBytes;
  if (special_total(c) > 0) m[i++] = g->specialButtons;
#define PUT16(v) do { m[i++] = (uint8_t)(v); m[i++] = (uint8_t)((v) >> 8); } while (0)
  if (c->whichAxes[0]) PUT16(g->x);
  if (c->whichAxes[1]) PUT16(g->y);
  if (c->whichAxes[2]) PUT16(g->z);
  if (c->whichAxes[5]) PUT16(g->rZ);
  if (c->whichAxes[3]) PUT16(g->rX);
  if (c->whichAxes[4]) PUT16(g->rY);
  if (c->whichAxes[6]) PUT16(g->slider1);
  if (c->whichAxes[7]) PUT16(g->slider2);
  if (c->hatSwitchCount > 0) {
    int8_t hats[4] = { g->hat1, 0, 0, 0 };
    for (int h = c->hatSwitchCount - 1; h >= 0; h--) m[i++] = (uint8_t)hats[h];
  }
  return g->reportSize;
}

void gp_press(gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] |= (uint8_t)(1 << ((b - 1) % 8)); }
void gp_release(gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] &= (uint8_t)~(1 << ((b - 1) % 8)); }
static void sp_press(gp_t *g, uint8_t b) { g->specialButtons |= (uint8_t)(1 << (g->specialPos[b] % 8)); }
static void sp_release(gp_t *g, uint8_t b) { g->specialButtons &= (uint8_t)~(1 << (g->specialPos[b] % 8)); }
void gp_press_start(gp_t *g) { sp_press(g, START_BUTTON); }
void gp_release_start(gp_t *g) { sp_release(g, START_BUTTON); }
void gp_press_select(gp_t *g) { sp_press(g, SELECT_BUTTON); }
void gp_release_select(gp_t *g) { sp_release(g, SELECT_BUTTON); }
void gp_set_axes(gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2)
{ g->x = x; g->y = y; g->z = z; g->rX = rX; g->rY = rY; g->rZ = rZ; g->slider1 = s1; g->slider2 = s2; }
void gp_set_hat1(gp_t *g, int8_t h) { g->hat1 = h; }
