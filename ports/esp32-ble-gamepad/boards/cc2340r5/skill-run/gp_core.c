#include "gp_core.h"
#include <string.h>

void gp_config_default(gp_config_t *c)
{
    memset(c, 0, sizeof *c);
    c->controller_type = 0x05;
    c->report_id = 3;
    c->button_count = 16;
    c->hat_count = 1;
    for (int i = 0; i < 8; i++) c->axes[i] = 1;
    c->axes_min = (int16_t)0x8000;
    c->axes_max = 0x7FFF;
}

static uint8_t count_true(const uint8_t *a, int from, int to)
{
    uint8_t n = 0;
    for (int i = from; i < to; i++) n += a[i] ? 1 : 0;
    return n;
}

void gp_reset_buttons(gp_t *g) { memset(g->buttons, 0, sizeof g->buttons); }

void gp_build_descriptor(gp_t *g)
{
    uint8_t *d = g->desc;
    int n = 0;
    const gp_config_t *c = &g->cfg;
#define B(v) d[n++] = (uint8_t)(v)
    uint8_t nspecial = count_true(c->special, 0, GP_POSSIBLE_SPECIAL);
    uint8_t ndesktop = count_true(c->special, 0, 3);
    uint8_t nconsumer = count_true(c->special, 3, 8);
    uint8_t naxes = count_true(c->axes, 0, 8);
    uint8_t sliders = (c->axes[6] ? 1 : 0) + (c->axes[7] ? 1 : 0);

    B(0x05); B(0x01);
    B(0x09); B(c->controller_type);
    B(0xa1); B(0x01);
    B(0x85); B(c->report_id);

    if (c->button_count > 0) {
        B(0x05); B(0x09); B(0x15); B(0x00); B(0x25); B(0x01); B(0x75); B(0x01);
        B(0x19); B(0x01); B(0x29); B(c->button_count); B(0x95); B(c->button_count);
        B(0x81); B(0x02);
        if (g->button_pad_bits > 0) { B(0x75); B(0x01); B(0x95); B(g->button_pad_bits); B(0x81); B(0x03); }
    }
    if (nspecial > 0) {
        B(0x15); B(0x00); B(0x25); B(0x01); B(0x75); B(0x01);
        if (ndesktop > 0) {
            B(0x05); B(0x01); B(0x95); B(ndesktop);
            if (c->special[0]) { B(0x09); B(0x3D); }   /* Start  */
            if (c->special[1]) { B(0x09); B(0x3E); }   /* Select */
            if (c->special[2]) { B(0x09); B(0x86); }   /* Menu   */
            B(0x81); B(0x02);
        }
        if (nconsumer > 0) {
            B(0x05); B(0x0C); B(0x95); B(nconsumer);
            if (c->special[3]) { B(0x0A); B(0x23); B(0x02); }  /* Home */
            if (c->special[4]) { B(0x0A); B(0x24); B(0x02); }  /* Back */
            if (c->special[5]) { B(0x09); B(0xE9); }
            if (c->special[6]) { B(0x09); B(0xEA); }
            if (c->special[7]) { B(0x09); B(0xE2); }
            B(0x81); B(0x02);
        }
        if (g->special_pad_bits > 0) { B(0x75); B(0x01); B(0x95); B(g->special_pad_bits); B(0x81); B(0x03); }
    }
    if (naxes > 0) {
        B(0x05); B(0x01);
        B(0x16); B(c->axes_min & 0xFF); B((c->axes_min >> 8) & 0xFF);
        B(0x26); B(c->axes_max & 0xFF); B((c->axes_max >> 8) & 0xFF);
        B(0x75); B(0x10);
        B(0x95); B(naxes - sliders);
        B(0xA1); B(0x00);
        if (c->axes[0]) { B(0x09); B(0x30); }   /* X  */
        if (c->axes[1]) { B(0x09); B(0x31); }   /* Y  */
        if (c->axes[2]) { B(0x09); B(0x32); }   /* Z  */
        if (c->axes[5]) { B(0x09); B(0x35); }   /* Rz */
        if (c->axes[3]) { B(0x09); B(0x33); }   /* Rx */
        if (c->axes[4]) { B(0x09); B(0x34); }   /* Ry */
        B(0x81); B(0x02);
        for (int s = 6; s <= 7; s++) {
            if (c->axes[s]) {
                B(0x95); B(0x01); B(0xA1); B(0x02); B(0x09); B(0x36); B(0x81); B(0x02); B(0xC0);
            }
        }
        B(0xc0);
    }
    /* simulation controls, gyro, accel: not carried (disabled in default config) */
    if (c->hat_count > 0) {
        B(0xA1); B(0x00); B(0x05); B(0x01);
        for (int i = 0; i < c->hat_count; i++) { B(0x09); B(0x39); }
        B(0x15); B(0x01); B(0x25); B(0x08); B(0x35); B(0x00);
        B(0x46); B(0x3B); B(0x01); B(0x65); B(0x12); B(0x75); B(0x08);
        B(0x95); B(c->hat_count); B(0x81); B(0x42); B(0xc0);
    }
    /* output / feature reports: not carried (disabled by default) */
    B(0xc0);
#undef B
    g->desc_len = n;
}

void gp_begin(gp_t *g, const gp_config_t *c)
{
    memset(g, 0, sizeof *g);
    g->cfg = *c;
    uint8_t nspecial = count_true(c->special, 0, GP_POSSIBLE_SPECIAL);
    g->button_pad_bits = 8 - (c->button_count % 8);
    if (g->button_pad_bits == 8) g->button_pad_bits = 0;
    g->special_pad_bits = 8 - (nspecial % 8);
    if (g->special_pad_bits == 8) g->special_pad_bits = 0;
    uint8_t pos = 0;
    for (int i = 0; i < GP_POSSIBLE_SPECIAL; i++) { g->special_pos[i] = pos; if (c->special[i]) pos++; }
    g->button_bytes = c->button_count / 8;
    if (g->button_pad_bits > 0) g->button_bytes++;
    uint8_t sbytes = nspecial / 8;
    if (g->special_pad_bits > 0) sbytes++;
    g->report_size = g->button_bytes + sbytes + count_true(c->axes, 0, 8) * 2 + c->hat_count;
    g->desc_len = 0;
    gp_build_descriptor(g);
}

static void put16(uint8_t *m, int *i, int16_t v) { m[(*i)++] = (uint8_t)v; m[(*i)++] = (uint8_t)(v >> 8); }

int gp_pack(const gp_t *g, uint8_t *m)
{
    const gp_config_t *c = &g->cfg;
    int i = 0;
    memset(m, 0, g->report_size);
    /* upstream memcpy's all 16 button bytes first; later fields overwrite the tail */
    memcpy(m, g->buttons, g->report_size < sizeof g->buttons ? g->report_size : sizeof g->buttons);
    i += g->button_bytes;
    if (count_true(c->special, 0, GP_POSSIBLE_SPECIAL) > 0) m[i++] = g->special_bits;
    if (c->axes[0]) put16(m, &i, g->x);
    if (c->axes[1]) put16(m, &i, g->y);
    if (c->axes[2]) put16(m, &i, g->z);
    if (c->axes[5]) put16(m, &i, g->rZ);
    if (c->axes[3]) put16(m, &i, g->rX);
    if (c->axes[4]) put16(m, &i, g->rY);
    if (c->axes[6]) put16(m, &i, g->slider1);
    if (c->axes[7]) put16(m, &i, g->slider2);
    if (c->hat_count > 0) {
        int8_t hats[4] = { g->hat1, 0, 0, 0 };   /* hat2..4 are zero upstream unless set */
        for (int h = c->hat_count - 1; h >= 0; h--) m[i++] = (uint8_t)hats[h];
    }
    return g->report_size;
}

void gp_press(gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] |= (uint8_t)(1u << ((b - 1) % 8)); }
void gp_release(gp_t *g, uint8_t b) { g->buttons[(b - 1) / 8] &= (uint8_t)~(1u << ((b - 1) % 8)); }

static void special(gp_t *g, uint8_t b, int on)
{
    uint8_t pos = b >= GP_POSSIBLE_SPECIAL ? 0 : g->special_pos[b];
    uint8_t mask = (uint8_t)(1u << (pos % 8));
    if (on) g->special_bits |= mask; else g->special_bits &= (uint8_t)~mask;
}
void gp_press_start(gp_t *g)     { special(g, GP_START, 1); }
void gp_release_start(gp_t *g)   { special(g, GP_START, 0); }
void gp_press_select(gp_t *g)    { special(g, GP_SELECT, 1); }
void gp_release_select(gp_t *g)  { special(g, GP_SELECT, 0); }

void gp_set_axes(gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2)
{
    g->x = x; g->y = y; g->z = z; g->rX = rX; g->rY = rY; g->rZ = rZ; g->slider1 = s1; g->slider2 = s2;
}
void gp_set_hat1(gp_t *g, int8_t h) { g->hat1 = h; }
