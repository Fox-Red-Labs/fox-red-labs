/* Portable core of ESP32-BLE-Gamepad (lemmingDev, MIT) -- GENERIC mode only.
 * Derived from BleGamepad.cpp buildGenericDescriptor()/sendGenericReport()/press()/...
 * at commit 65d517864f86dc55b161f8b32a1650e0eb3c25aa. No device headers. */
#ifndef GP_CORE_H
#define GP_CORE_H
#include <stdint.h>

#define GP_MAX_DESC 512            /* tempHidReportDescriptor[512] upstream */
#define GP_POSSIBLE_SPECIAL 8
#define GP_START 0
#define GP_SELECT 1
#define GP_HAT_CENTERED 0
#define GP_HAT_UP 1
#define GP_HAT_UP_LEFT 8
#define GP_HAT_DOWN_RIGHT 4

typedef struct {                   /* subset of BleGamepadConfiguration, upstream defaults */
    uint8_t  controller_type;      /* 0x05 gamepad */
    uint8_t  report_id;            /* 3 */
    uint16_t button_count;         /* 16 */
    uint8_t  hat_count;            /* 1 */
    uint8_t  special[GP_POSSIBLE_SPECIAL]; /* upstream default: all 0 (see NOTES) */
    uint8_t  axes[8];              /* X Y Z Rx Ry Rz Slider1 Slider2: all 1 */
    int16_t  axes_min, axes_max;   /* 0x8000, 0x7FFF */
} gp_config_t;

typedef struct {
    gp_config_t cfg;
    uint8_t  desc[GP_MAX_DESC];
    int      desc_len;
    uint8_t  buttons[16];
    uint8_t  special_bits;
    int16_t  x, y, z, rX, rY, rZ, slider1, slider2;
    int8_t   hat1;
    uint8_t  special_pos[GP_POSSIBLE_SPECIAL];
    uint8_t  button_pad_bits, special_pad_bits, button_bytes;
    uint8_t  report_size;
} gp_t;

void gp_config_default(gp_config_t *c);
void gp_begin(gp_t *g, const gp_config_t *c);      /* sizes + descriptor, like begin() */
void gp_build_descriptor(gp_t *g);                 /* buildGenericDescriptor() */
int  gp_pack(const gp_t *g, uint8_t *out);         /* sendGenericReport() body; returns len */
void gp_press(gp_t *g, uint8_t b);
void gp_release(gp_t *g, uint8_t b);
void gp_press_start(gp_t *g);
void gp_release_start(gp_t *g);
void gp_press_select(gp_t *g);
void gp_release_select(gp_t *g);
void gp_set_axes(gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2);
void gp_set_hat1(gp_t *g, int8_t h);
void gp_reset_buttons(gp_t *g);
#endif
