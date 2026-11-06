/* Portable core of lemmingDev/ESP32-BLE-Gamepad (commit 65d5178), GENERIC mode.
   No device headers. Ported from BleGamepad.cpp buildGenericDescriptor / sendGenericReport. */
#ifndef GAMEPAD_CORE_H
#define GAMEPAD_CORE_H
#include <stdint.h>
#include <stddef.h>

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
#define POSSIBLESPECIALBUTTONS 8

#define GP_DESC_MAX 512 /* tempHidReportDescriptor[512] in the library */
#define GP_REPORT_MAX 64

typedef struct {
  uint8_t controllerType;  /* 0x05 gamepad */
  uint8_t hidReportId;     /* 3 */
  uint16_t buttonCount;    /* 16 */
  uint8_t hatSwitchCount;  /* 1 */
  uint8_t whichSpecial[POSSIBLESPECIALBUTTONS]; /* START,SELECT,MENU,HOME,BACK,VOL+,VOL-,MUTE */
  uint8_t whichAxes[8];    /* X,Y,Z,Rx,Ry,Rz,SLIDER1,SLIDER2 */
  int16_t axesMin, axesMax;
} gp_config_t;

typedef struct {
  gp_config_t cfg;
  uint8_t desc[GP_DESC_MAX];
  int descSize;
  int reportSize;
  uint8_t numOfButtonBytes, buttonPad, specialPad;
  uint8_t specialPos[POSSIBLESPECIALBUTTONS];
  uint8_t buttons[16], specialButtons;
  int16_t x, y, z, rX, rY, rZ, slider1, slider2;
  int8_t hat1;
} gp_t;

void gp_default_config(gp_config_t *c); /* BleGamepadConfiguration() constructor defaults */
void gp_begin(gp_t *g, const gp_config_t *c);  /* computes sizes + buildGenericDescriptor */
void gp_build_descriptor(gp_t *g);
int gp_pack(const gp_t *g, uint8_t *out);      /* sendGenericReport payload, returns length */
void gp_reset_buttons(gp_t *g);
void gp_press(gp_t *g, uint8_t b);
void gp_release(gp_t *g, uint8_t b);
void gp_press_start(gp_t *g);
void gp_release_start(gp_t *g);
void gp_press_select(gp_t *g);
void gp_release_select(gp_t *g);
void gp_set_axes(gp_t *g, int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t s1, int16_t s2);
void gp_set_hat1(gp_t *g, int8_t h);
#endif
