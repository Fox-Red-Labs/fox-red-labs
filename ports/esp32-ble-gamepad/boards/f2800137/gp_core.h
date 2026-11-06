/* Generic-mode core of lemmingDev/ESP32-BLE-Gamepad @ 65d5178 (MIT): descriptor builder and report packer.
   No device headers. A "byte" here is gp_byte: one unsigned char holding 0..255. On the C28x an unsigned
   char is 16 bits wide, so every store into a gp_byte is masked with 0xFF; on a host it is a real byte. */
#ifndef GP_CORE_H
#define GP_CORE_H
typedef unsigned char gp_byte;
typedef short gp_i16;            /* the C28x short is 16 bits; this core is only built where short is 16 */

#define GP_DESC_MAX 512          /* tempHidReportDescriptor[512] in the library */
#define GP_BUTTON_BYTES 16       /* _buttons[16] */
#define GP_POSSIBLESPECIALBUTTONS 8
#define GP_POSSIBLEAXES 8
#define GP_POSSIBLESIMULATIONCONTROLS 5

#define BUTTON_1 0x1
#define BUTTON_5 0x5
#define BUTTON_8 0x8
#define BUTTON_9 0x9
#define BUTTON_16 0x10
#define HAT_CENTERED 0
#define HAT_UP_RIGHT 2
#define HAT_DOWN_RIGHT 4
#define HAT_UP_LEFT 8
#define START_BUTTON 0
#define SELECT_BUTTON 1

typedef struct {                 /* the BleGamepadConfiguration fields the generic path reads */
    gp_byte controllerType, hidReportId, hatSwitchCount;
    unsigned int buttonCount;
    int whichSpecialButtons[GP_POSSIBLESPECIALBUTTONS];
    int whichAxes[GP_POSSIBLEAXES];
    int whichSimulationControls[GP_POSSIBLESIMULATIONCONTROLS];
    int includeGyroscope, includeAccelerometer, enableOutputReport, enableFeatureReport;
    gp_i16 axesMin, axesMax, simulationMin, simulationMax, motionMin, motionMax;
    unsigned int outputReportLength, featureReportLength;
} gp_config;

typedef struct {
    gp_config cfg;
    gp_byte buttons[GP_BUTTON_BYTES];
    gp_byte specialButtons;
    gp_byte specialButtonPositions[GP_POSSIBLESPECIALBUTTONS];
    gp_i16 x, y, z, rX, rY, rZ, slider1, slider2;
    gp_i16 rudder, throttle, accelerator, brake, steering, gX, gY, gZ, aX, aY, aZ;
    signed char hat1, hat2, hat3, hat4;
    gp_byte genericButtonPaddingBits, genericSpecialButtonPaddingBits, numOfButtonBytes;
    unsigned int hidReportSize, hidReportDescriptorSize;
    gp_byte desc[GP_DESC_MAX];
} gp_state;

void gp_config_default(gp_config *c);            /* BleGamepadConfiguration() */
void gp_begin(gp_state *g);                      /* the generic part of BleGamepad::begin(), no radio */
void gp_build_descriptor(gp_state *g);           /* buildGenericDescriptor() */
unsigned int gp_pack_report(gp_state *g, gp_byte *m);  /* sendGenericReport() up to setValue(); returns length */
void gp_reset_buttons(gp_state *g);
void gp_press(gp_state *g, gp_byte b);
void gp_release(gp_state *g, gp_byte b);
void gp_press_special(gp_state *g, gp_byte b);
void gp_release_special(gp_state *g, gp_byte b);
#define gp_press_start(g) gp_press_special((g), START_BUTTON)
#define gp_release_start(g) gp_release_special((g), START_BUTTON)
#define gp_press_select(g) gp_press_special((g), SELECT_BUTTON)
void gp_set_axes(gp_state *g, gp_i16 x, gp_i16 y, gp_i16 z, gp_i16 rX, gp_i16 rY, gp_i16 rZ, gp_i16 s1, gp_i16 s2);
void gp_set_hat1(gp_state *g, signed char h);
#endif
