/* Portable core of lemmingDev ESP32-BLE-Gamepad (commit 65d5178, MIT), generic mode only.
 * No device headers. Descriptor builder and report packer follow BleGamepad.cpp
 * buildGenericDescriptor() and sendGenericReport() byte for byte. */
#ifndef FRL_GAMEPAD_H_
#define FRL_GAMEPAD_H_
#include <stdint.h>
#include <stddef.h>

#define FRL_GP_MAX_DESC 512 /* size of tempHidReportDescriptor in the library */

#define BUTTON_1 0x1
#define BUTTON_5 0x5
#define BUTTON_8 0x8
#define BUTTON_9 0x9
#define BUTTON_16 0x10
#define HAT_CENTERED 0
#define HAT_UP 1
#define HAT_UP_RIGHT 2
#define HAT_RIGHT 3
#define HAT_DOWN_RIGHT 4
#define HAT_DOWN 5
#define HAT_DOWN_LEFT 6
#define HAT_LEFT 7
#define HAT_UP_LEFT 8
#define START_BUTTON 0
#define SELECT_BUTTON 1
#define MENU_BUTTON 2
#define HOME_BUTTON 3
#define BACK_BUTTON 4
#define VOLUME_INC_BUTTON 5
#define VOLUME_DEC_BUTTON 6
#define VOLUME_MUTE_BUTTON 7

typedef struct {
    uint8_t controllerType;      /* 0x05 gamepad */
    uint8_t hidReportId;         /* 3 */
    uint16_t buttonCount;        /* 16 */
    uint8_t hatSwitchCount;      /* 1 */
    uint8_t whichSpecial[8];     /* start, select, menu, home, back, vol+, vol-, mute */
    uint8_t whichAxes[8];        /* x, y, z, rX, rY, rZ, slider1, slider2 */
    int16_t axesMin, axesMax;    /* 0x8000, 0x7FFF */
} frl_gp_config_t;

typedef struct {
    frl_gp_config_t cfg;
    uint8_t buttons[16];
    uint8_t special;
    int16_t x, y, z, rX, rY, rZ, slider1, slider2;
    int16_t hat1;
    uint8_t specialPos[8];
    uint8_t buttonPad, specialPad, buttonBytes;
    uint16_t reportSize;
} frl_gp_t;

/* The library's BleGamepadConfiguration constructor values. Define FRL_GP_START_SELECT=1 to also
 * enable start and select (what the example's comment and the job spec say; the library code does not). */
void frl_gp_default_config(frl_gp_config_t *c);
/* begin(): derive padding, special-button positions and report size; zero all state. */
void frl_gp_init(frl_gp_t *g, const frl_gp_config_t *c);
/* buildGenericDescriptor(): returns the length, or 0 if cap is too small. */
size_t frl_gp_build_descriptor(const frl_gp_t *g, uint8_t *out, size_t cap);
/* sendGenericReport() without the notify: writes hidReportSize bytes, returns the length. */
size_t frl_gp_pack(const frl_gp_t *g, uint8_t *out);

void frl_gp_resetButtons(frl_gp_t *g);
void frl_gp_press(frl_gp_t *g, uint8_t b);
void frl_gp_release(frl_gp_t *g, uint8_t b);
void frl_gp_pressStart(frl_gp_t *g);
void frl_gp_releaseStart(frl_gp_t *g);
void frl_gp_pressSelect(frl_gp_t *g);
void frl_gp_setAxes(frl_gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ,
                    int16_t slider1, int16_t slider2);
void frl_gp_setHat1(frl_gp_t *g, int8_t hat1);

#endif
