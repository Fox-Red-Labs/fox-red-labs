/*
 * frl_gamepad.h: portable core of lemmingDev/ESP32-BLE-Gamepad (commit 65d5178),
 * GENERIC mode only: HID report descriptor builder and input report packer.
 * Plain C99, no device headers. Ported from BleGamepad.cpp (buildGenericDescriptor,
 * sendGenericReport, press/release/special/setAxes/setHat1/resetButtons) and
 * BleGamepadConfiguration.cpp (defaults). Left out: SInput, XInput, output and
 * feature reports, NuS, pairing management, touchpad.
 */
#ifndef FRL_GAMEPAD_H
#define FRL_GAMEPAD_H

#include <stdint.h>

#define FRL_GP_POSSIBLE_SPECIAL 8
#define FRL_GP_POSSIBLE_AXES    8
#define FRL_GP_POSSIBLE_SIM     5
#define FRL_GP_DESC_MAX         512   /* tempHidReportDescriptor[512] in the library */
#define FRL_GP_REPORT_MAX       64

/* Constants as in BleGamepadConfiguration.h */
#define FRL_BUTTON_1   0x1
#define FRL_BUTTON_2   0x2
#define FRL_BUTTON_5   0x5
#define FRL_BUTTON_8   0x8
#define FRL_BUTTON_9   0x9
#define FRL_BUTTON_16  0x10
#define FRL_HAT_CENTERED    0
#define FRL_HAT_UP          1
#define FRL_HAT_DOWN_RIGHT  4
#define FRL_HAT_UP_LEFT     8
#define FRL_START_BUTTON    0
#define FRL_SELECT_BUTTON   1

typedef struct {
    uint8_t  controllerType;                       /* 0x05 gamepad */
    uint8_t  hidReportId;                          /* 3 */
    uint16_t buttonCount;                          /* 16 */
    uint8_t  hatSwitchCount;                       /* 1 */
    uint8_t  whichSpecial[FRL_GP_POSSIBLE_SPECIAL];
    uint8_t  whichAxes[FRL_GP_POSSIBLE_AXES];      /* X Y Z Rx Ry Rz S1 S2 */
    uint8_t  whichSim[FRL_GP_POSSIBLE_SIM];
    uint8_t  includeGyro, includeAccel;
    int16_t  axesMin, axesMax, simMin, simMax, motionMin, motionMax;
} frl_gp_config_t;

typedef struct {
    frl_gp_config_t cfg;
    uint8_t  buttons[16];
    uint8_t  special;
    int16_t  x, y, z, rX, rY, rZ, slider1, slider2;
    int16_t  rudder, throttle, accelerator, brake, steering;
    int16_t  gX, gY, gZ, aX, aY, aZ;
    signed char hat1, hat2, hat3, hat4;
    uint8_t  buttonPad, specialPad;
    uint8_t  specialPos[FRL_GP_POSSIBLE_SPECIAL];
    uint8_t  numButtonBytes;
    uint8_t  reportSize;                           /* hidReportSize */
    uint16_t descLen;                              /* hidReportDescriptorSize */
    uint8_t  desc[FRL_GP_DESC_MAX];
} frl_gp_t;

/* The library's BleGamepadConfiguration constructor defaults. Note: the library
 * default has NO special buttons (start/select are off), whatever the example
 * comment says. Define FRL_GP_START_SELECT=1 in the glue to switch them on. */
void frl_gp_config_default(frl_gp_config_t *c);
/* begin() minus the BLE part: state to zero, sizes computed, descriptor built.
 * Returns 0, or -1 if the descriptor would not fit. */
int  frl_gp_begin(frl_gp_t *g, const frl_gp_config_t *c);
void frl_gp_resetButtons(frl_gp_t *g);
void frl_gp_press(frl_gp_t *g, uint8_t b);
void frl_gp_release(frl_gp_t *g, uint8_t b);
void frl_gp_pressStart(frl_gp_t *g);
void frl_gp_releaseStart(frl_gp_t *g);
void frl_gp_pressSelect(frl_gp_t *g);
void frl_gp_releaseSelect(frl_gp_t *g);
void frl_gp_setAxes(frl_gp_t *g, int16_t x, int16_t y, int16_t z,
                    int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2);
void frl_gp_setHat1(frl_gp_t *g, signed char hat1);
/* sendGenericReport() without the notify: writes the payload (no report id byte)
 * to out (>= FRL_GP_REPORT_MAX) and returns its length (= reportSize). */
uint8_t frl_gp_pack(const frl_gp_t *g, uint8_t *out);

#endif
