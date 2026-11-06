#include <string.h>
#include "gp_core.h"

#define B8(v) ((gp_byte)((v) & 0xFF))
#define EMIT(v) (tempHidReportDescriptor[hidReportDescriptorSize++] = B8(v))
#define lowByte(w) B8(w)
#define highByte(w) B8((w) >> 8)

/* BleGamepadConfiguration getters used by the generic path */
#define GP_ControllerType(c) ((c)->controllerType)
#define GP_HidReportId(c) ((c)->hidReportId)
#define GP_ButtonCount(c) ((c)->buttonCount)
#define GP_HatSwitchCount(c) ((c)->hatSwitchCount)
#define GP_AxesMin(c) ((c)->axesMin)
#define GP_AxesMax(c) ((c)->axesMax)
#define GP_SimulationMin(c) ((c)->simulationMin)
#define GP_SimulationMax(c) ((c)->simulationMax)
#define GP_MotionMin(c) ((c)->motionMin)
#define GP_MotionMax(c) ((c)->motionMax)
#define GP_EnableOutputReport(c) ((c)->enableOutputReport)
#define GP_EnableFeatureReport(c) ((c)->enableFeatureReport)
#define GP_OutputReportLength(c) ((c)->outputReportLength)
#define GP_FeatureReportLength(c) ((c)->featureReportLength)
#define GP_IncludeGyroscope(c) ((c)->includeGyroscope)
#define GP_IncludeAccelerometer(c) ((c)->includeAccelerometer)
#define GP_IncludeStart(c) ((c)->whichSpecialButtons[0])
#define GP_IncludeSelect(c) ((c)->whichSpecialButtons[1])
#define GP_IncludeMenu(c) ((c)->whichSpecialButtons[2])
#define GP_IncludeHome(c) ((c)->whichSpecialButtons[3])
#define GP_IncludeBack(c) ((c)->whichSpecialButtons[4])
#define GP_IncludeVolumeInc(c) ((c)->whichSpecialButtons[5])
#define GP_IncludeVolumeDec(c) ((c)->whichSpecialButtons[6])
#define GP_IncludeVolumeMute(c) ((c)->whichSpecialButtons[7])
#define GP_IncludeXAxis(c) ((c)->whichAxes[0])
#define GP_IncludeYAxis(c) ((c)->whichAxes[1])
#define GP_IncludeZAxis(c) ((c)->whichAxes[2])
#define GP_IncludeRxAxis(c) ((c)->whichAxes[3])
#define GP_IncludeRyAxis(c) ((c)->whichAxes[4])
#define GP_IncludeRzAxis(c) ((c)->whichAxes[5])
#define GP_IncludeSlider1(c) ((c)->whichAxes[6])
#define GP_IncludeSlider2(c) ((c)->whichAxes[7])
#define GP_IncludeRudder(c) ((c)->whichSimulationControls[0])
#define GP_IncludeThrottle(c) ((c)->whichSimulationControls[1])
#define GP_IncludeAccelerator(c) ((c)->whichSimulationControls[2])
#define GP_IncludeBrake(c) ((c)->whichSimulationControls[3])
#define GP_IncludeSteering(c) ((c)->whichSimulationControls[4])

static unsigned int count_true(const int *a, int from, int to) { unsigned int n = 0; int i; for (i = from; i < to; i++) n += (a[i] != 0); return n; }
#define GP_TotalSpecialButtonCount(c) count_true((c)->whichSpecialButtons, 0, GP_POSSIBLESPECIALBUTTONS)
#define GP_DesktopSpecialButtonCount(c) count_true((c)->whichSpecialButtons, 0, 3)
#define GP_ConsumerSpecialButtonCount(c) count_true((c)->whichSpecialButtons, 3, 8)
#define GP_AxisCount(c) count_true((c)->whichAxes, 0, GP_POSSIBLEAXES)
#define GP_SimulationCount(c) count_true((c)->whichSimulationControls, 0, GP_POSSIBLESIMULATIONCONTROLS)

void gp_config_default(gp_config *c)   /* BleGamepadConfiguration.cpp constructor */
{
    int i;
    memset(c, 0, sizeof(*c));
    c->controllerType = 0x05;          /* CONTROLLER_TYPE_GAMEPAD */
    c->hidReportId = 3;
    c->buttonCount = 16;
    c->hatSwitchCount = 1;
    for (i = 0; i < GP_POSSIBLEAXES; i++) c->whichAxes[i] = 1;
    c->axesMin = (gp_i16)0x8000; c->axesMax = 0x7FFF;
    c->simulationMin = 0; c->simulationMax = 0x7FFF;
    c->motionMin = 0; c->motionMax = 0x7FFF;
    c->outputReportLength = 64; c->featureReportLength = 64;
}

void gp_reset_buttons(gp_state *g) { memset(g->buttons, 0, sizeof(g->buttons)); }

/* body below is lines 150-776 of BleGamepad.cpp buildGenericDescriptor(), mechanically rewritten (plus uint8_t -> gp_byte on one local):
   tempHidReportDescriptor[n++] = v  ->  EMIT(v); configuration.getX() -> GP_X(c). Nothing else changed. */
void gp_build_descriptor(gp_state *g)
{
  const gp_config *c = &g->cfg;
  gp_byte *tempHidReportDescriptor = g->desc;
  unsigned int hidReportDescriptorSize = 0;
  gp_byte genericButtonPaddingBits = g->genericButtonPaddingBits;
  gp_byte genericSpecialButtonPaddingBits = g->genericSpecialButtonPaddingBits;
  // USAGE_PAGE (Generic Desktop)
  EMIT(0x05);
  EMIT(0x01);

  // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
  EMIT(0x09);
  EMIT(GP_ControllerType(c));

  // COLLECTION (Application)
  EMIT(0xa1);
  EMIT(0x01);

  // REPORT_ID (Default: 3)
  EMIT(0x85);
  EMIT(GP_HidReportId(c));

  if (GP_ButtonCount(c) > 0)
  {
    // USAGE_PAGE (Button)
    EMIT(0x05);
    EMIT(0x09);

    // LOGICAL_MINIMUM (0)
    EMIT(0x15);
    EMIT(0x00);

    // LOGICAL_MAXIMUM (1)
    EMIT(0x25);
    EMIT(0x01);

    // REPORT_SIZE (1)
    EMIT(0x75);
    EMIT(0x01);

    // USAGE_MINIMUM (Button 1)
    EMIT(0x19);
    EMIT(0x01);

    // USAGE_MAXIMUM (Up to 128 buttons possible)
    EMIT(0x29);
    EMIT(GP_ButtonCount(c));

    // REPORT_COUNT (# of buttons)
    EMIT(0x95);
    EMIT(GP_ButtonCount(c));

    // INPUT (Data,Var,Abs)
    EMIT(0x81);
    EMIT(0x02);

    if (genericButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      EMIT(0x75);
      EMIT(0x01);

      // REPORT_COUNT (# of padding bits)
      EMIT(0x95);
      EMIT(genericButtonPaddingBits);

      // INPUT (Const,Var,Abs)
      EMIT(0x81);
      EMIT(0x03);
    }
  }

  if (GP_TotalSpecialButtonCount(c) > 0)
  {
    // LOGICAL_MINIMUM (0)
    EMIT(0x15);
    EMIT(0x00);

    // LOGICAL_MAXIMUM (1)
    EMIT(0x25);
    EMIT(0x01);

    // REPORT_SIZE (1)
    EMIT(0x75);
    EMIT(0x01);

    if (GP_DesktopSpecialButtonCount(c) > 0)
    {
      // USAGE_PAGE (Generic Desktop)
      EMIT(0x05);
      EMIT(0x01);

      // REPORT_COUNT
      EMIT(0x95);
      EMIT(GP_DesktopSpecialButtonCount(c));

      if (GP_IncludeStart(c))
      {
        // USAGE (Start)
        EMIT(0x09);
        EMIT(0x3D);
      }

      if (GP_IncludeSelect(c))
      {
        // USAGE (Select)
        EMIT(0x09);
        EMIT(0x3E);
      }

      if (GP_IncludeMenu(c))
      {
        // USAGE (App Menu)
        EMIT(0x09);
        EMIT(0x86);
      }

      // INPUT (Data,Var,Abs)
      EMIT(0x81);
      EMIT(0x02);
    }

    if (GP_ConsumerSpecialButtonCount(c) > 0)
    {
      // USAGE_PAGE (Consumer Page)
      EMIT(0x05);
      EMIT(0x0C);

      // REPORT_COUNT
      EMIT(0x95);
      EMIT(GP_ConsumerSpecialButtonCount(c));

      if (GP_IncludeHome(c))
      {
        // USAGE (Home)
        EMIT(0x0A);
        EMIT(0x23);
        EMIT(0x02);
      }

      if (GP_IncludeBack(c))
      {
        // USAGE (Back)
        EMIT(0x0A);
        EMIT(0x24);
        EMIT(0x02);
      }

      if (GP_IncludeVolumeInc(c))
      {
        // USAGE (Volume Increment)
        EMIT(0x09);
        EMIT(0xE9);
      }

      if (GP_IncludeVolumeDec(c))
      {
        // USAGE (Volume Decrement)
        EMIT(0x09);
        EMIT(0xEA);
      }

      if (GP_IncludeVolumeMute(c))
      {
        // USAGE (Mute)
        EMIT(0x09);
        EMIT(0xE2);
      }

      // INPUT (Data,Var,Abs)
      EMIT(0x81);
      EMIT(0x02);
    }

    if (genericSpecialButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      EMIT(0x75);
      EMIT(0x01);

      // REPORT_COUNT (# of padding bits)
      EMIT(0x95);
      EMIT(genericSpecialButtonPaddingBits);

      // INPUT (Const,Var,Abs)
      EMIT(0x81);
      EMIT(0x03);
    }
  }

  if (GP_AxisCount(c) > 0)
  {
    // USAGE_PAGE (Generic Desktop)
    EMIT(0x05);
    EMIT(0x01);

    // LOGICAL_MINIMUM (-32767)
    EMIT(0x16);
    EMIT(lowByte(GP_AxesMin(c)));
    EMIT(highByte(GP_AxesMin(c)));

    // LOGICAL_MAXIMUM (+32767)
    EMIT(0x26);
    EMIT(lowByte(GP_AxesMax(c)));
    EMIT(highByte(GP_AxesMax(c)));

    // REPORT_SIZE (16)
    EMIT(0x75);
    EMIT(0x10);

    // REPORT_COUNT (GP_AxisCount(c) - sliders)
    gp_byte sliders = (GP_IncludeSlider1(c) ? 1 : 0) + (GP_IncludeSlider2(c) ? 1 : 0);
    EMIT(0x95);
    EMIT(GP_AxisCount(c) - sliders);

    // COLLECTION (Physical)
    EMIT(0xA1);
    EMIT(0x00);

    if (GP_IncludeXAxis(c))
    {
      // USAGE (X)
      EMIT(0x09);
      EMIT(0x30);
    }

    if (GP_IncludeYAxis(c))
    {
      // USAGE (Y)
      EMIT(0x09);
      EMIT(0x31);
    }

    if (GP_IncludeZAxis(c))
    {
      // USAGE (Z)
      EMIT(0x09);
      EMIT(0x32);
    }

    if (GP_IncludeRzAxis(c))
    {
      // USAGE (Rz)
      EMIT(0x09);
      EMIT(0x35);
    }

    if (GP_IncludeRxAxis(c))
    {
      // USAGE (Rx)
      EMIT(0x09);
      EMIT(0x33);
    }

    if (GP_IncludeRyAxis(c))
    {
      // USAGE (Ry)
      EMIT(0x09);
      EMIT(0x34);
    }

    // INPUT (Data,Var,Abs)
    EMIT(0x81);
    EMIT(0x02);

    if (GP_IncludeSlider1(c))
    {
      // REPORT_COUNT (1)
      EMIT(0x95);
      EMIT(0x01);

      // COLLECTION (Logical)
      EMIT(0xA1);
      EMIT(0x02);

      // USAGE (Slider)
      EMIT(0x09);
      EMIT(0x36);

      // INPUT (Data,Var,Abs)
      EMIT(0x81);
      EMIT(0x02);

      // END_COLLECTION (Logical)
      EMIT(0xC0);
    }

    if (GP_IncludeSlider2(c))
    {
      // REPORT_COUNT (1)
      EMIT(0x95);
      EMIT(0x01);

      // COLLECTION (Logical)
      EMIT(0xA1);
      EMIT(0x02);

      // USAGE (Slider)
      EMIT(0x09);
      EMIT(0x36);

      // INPUT (Data,Var,Abs)
      EMIT(0x81);
      EMIT(0x02);

      // END_COLLECTION (Logical)
      EMIT(0xC0);
    }

    // END_COLLECTION (Physical)
    EMIT(0xc0);
  }

  if (GP_SimulationCount(c) > 0)
  {
    // USAGE_PAGE (Simulation Controls)
    EMIT(0x05);
    EMIT(0x02);

    // LOGICAL_MINIMUM (-32767)
    EMIT(0x16);
    EMIT(lowByte(GP_SimulationMin(c)));
    EMIT(highByte(GP_SimulationMin(c)));

    // LOGICAL_MAXIMUM (+32767)
    EMIT(0x26);
    EMIT(lowByte(GP_SimulationMax(c)));
    EMIT(highByte(GP_SimulationMax(c)));

    // REPORT_SIZE (16)
    EMIT(0x75);
    EMIT(0x10);

    // REPORT_COUNT (GP_SimulationCount(c))
    EMIT(0x95);
    EMIT(GP_SimulationCount(c));

    // COLLECTION (Physical)
    EMIT(0xA1);
    EMIT(0x00);

    if (GP_IncludeRudder(c))
    {
      // USAGE (Rudder)
      EMIT(0x09);
      EMIT(0xBA);
    }

    if (GP_IncludeThrottle(c))
    {
      // USAGE (Throttle)
      EMIT(0x09);
      EMIT(0xBB);
    }

    if (GP_IncludeAccelerator(c))
    {
      // USAGE (Accelerator)
      EMIT(0x09);
      EMIT(0xC4);
    }

    if (GP_IncludeBrake(c))
    {
      // USAGE (Brake)
      EMIT(0x09);
      EMIT(0xC5);
    }

    if (GP_IncludeSteering(c))
    {
      // USAGE (Steering)
      EMIT(0x09);
      EMIT(0xC8);
    }

    // INPUT (Data,Var,Abs)
    EMIT(0x81);
    EMIT(0x02);

    // END_COLLECTION (Physical)
    EMIT(0xc0);
  }

  // Gyroscope
  if (GP_IncludeGyroscope(c))
  {
    // COLLECTION (Physical)
    EMIT(0xA1);
    EMIT(0x00);

    // USAGE_PAGE (Generic Desktop)
    EMIT(0x05);
    EMIT(0x01);

    // USAGE (Gyroscope - Rotational X - Rx)
    EMIT(0x09);
    EMIT(0x33);

    // USAGE (Rotational - Rotational Y - Ry)
    EMIT(0x09);
    EMIT(0x34);

    // USAGE (Rotational - Rotational Z - Rz)
    EMIT(0x09);
    EMIT(0x35);

    // LOGICAL_MINIMUM (-32767)
    EMIT(0x16);
    EMIT(lowByte(GP_MotionMin(c)));
    EMIT(highByte(GP_MotionMin(c)));

    // LOGICAL_MAXIMUM (+32767)
    EMIT(0x26);
    EMIT(lowByte(GP_MotionMax(c)));
    EMIT(highByte(GP_MotionMax(c)));

    // REPORT_SIZE (16)
    EMIT(0x75);
    EMIT(0x10);

    // REPORT_COUNT (3)
    EMIT(0x95);
    EMIT(0x03);

    // INPUT (Data,Var,Abs)
    EMIT(0x81);
    EMIT(0x02);

    // END_COLLECTION (Physical)
    EMIT(0xc0);
  }

  // Accelerometer
  if (GP_IncludeAccelerometer(c))
  {
    // COLLECTION (Physical)
    EMIT(0xA1);
    EMIT(0x00);

    // USAGE_PAGE (Generic Desktop)
    EMIT(0x05);
    EMIT(0x01);

    // USAGE (Accelerometer - Vector X - Vx)
    EMIT(0x09);
    EMIT(0x40);

    // USAGE (Accelerometer - Vector Y - Vy)
    EMIT(0x09);
    EMIT(0x41);

    // USAGE (Accelerometer - Vector Z - Vz)
    EMIT(0x09);
    EMIT(0x42);

    // LOGICAL_MINIMUM (-32767)
    EMIT(0x16);
    EMIT(lowByte(GP_MotionMin(c)));
    EMIT(highByte(GP_MotionMin(c)));

    // LOGICAL_MAXIMUM (+32767)
    EMIT(0x26);
    EMIT(lowByte(GP_MotionMax(c)));
    EMIT(highByte(GP_MotionMax(c)));

    // REPORT_SIZE (16)
    EMIT(0x75);
    EMIT(0x10);

    // REPORT_COUNT (3)
    EMIT(0x95);
    EMIT(0x03);

    // INPUT (Data,Var,Abs)
    EMIT(0x81);
    EMIT(0x02);

    // END_COLLECTION (Physical)
    EMIT(0xc0);
  }

  if (GP_HatSwitchCount(c) > 0)
  {
    // COLLECTION (Physical)
    EMIT(0xA1);
    EMIT(0x00);

    // USAGE_PAGE (Generic Desktop)
    EMIT(0x05);
    EMIT(0x01);

    // USAGE (Hat Switch)
    for (int currentHatIndex = 0; currentHatIndex < GP_HatSwitchCount(c); currentHatIndex++)
    {
      EMIT(0x09);
      EMIT(0x39);
    }

    // Logical Min (1)
    EMIT(0x15);
    EMIT(0x01);

    // Logical Max (8)
    EMIT(0x25);
    EMIT(0x08);

    // Physical Min (0)
    EMIT(0x35);
    EMIT(0x00);

    // Physical Max (315)
    EMIT(0x46);
    EMIT(0x3B);
    EMIT(0x01);

    // Unit (SI Rot : Ang Pos)
    EMIT(0x65);
    EMIT(0x12);

    // Report Size (8)
    EMIT(0x75);
    EMIT(0x08);

    // Report Count (4)
    EMIT(0x95);
    EMIT(GP_HatSwitchCount(c));

    // Input (Data, Variable, Absolute)
    EMIT(0x81);
    EMIT(0x42);

    // END_COLLECTION (Physical)
    EMIT(0xc0);
  }

  if (GP_EnableOutputReport(c))
  {
    // Usage Page (Vendor Defined 0xFF00)
    EMIT(0x06);
    EMIT(0x00);
    EMIT(0xFF);

    // Usage (Vendor Usage 0x01)
    EMIT(0x09);
    EMIT(0x01);

    // Usage (0x01)
    EMIT(0x09);
    EMIT(0x01);

    // Logical Minimum (0)
    EMIT(0x15);
    EMIT(0x00);

    // Logical Maximum (255)
    EMIT(0x26);
    EMIT(0xFF);
    EMIT(0x00);

    // Report Size (8 bits)
    EMIT(0x75);
    EMIT(0x08);

    if (GP_OutputReportLength(c) <= 0xFF)
    {
      // Report Count (0~255 bytes)
      EMIT(0x95);
      EMIT(GP_OutputReportLength(c));
    }
    else
    {
      // Report Count (0~65535 bytes)
      EMIT(0x96);
      EMIT(lowByte(GP_OutputReportLength(c)));
      EMIT(highByte(GP_OutputReportLength(c)));
    }

    // Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    EMIT(0x91);
    EMIT(0x02);
  }

  if (GP_EnableFeatureReport(c))
  {
    // Vendor blob feature report
    // Usage Page (Vendor Defined 0xFF00)
    EMIT(0x06);
    EMIT(0x00);
    EMIT(0xFF);

    // Usage (Vendor Usage 0x01)
    EMIT(0x09);
    EMIT(0x01);

    // Usage (0x01)
    EMIT(0x09);
    EMIT(0x01);

    // Logical Minimum (0)
    EMIT(0x15);
    EMIT(0x00);

    // Logical Maximum (255)
    EMIT(0x26);
    EMIT(0xFF);
    EMIT(0x00);

    // Report Size (8 bits)
    EMIT(0x75);
    EMIT(0x08);

    if (GP_FeatureReportLength(c) <= 0xFF)
    {
      // Report Count (0~255 bytes)
      EMIT(0x95);
      EMIT(GP_FeatureReportLength(c));
    }
    else
    {
      // Report Count (0~65535 bytes)
      EMIT(0x96);
      EMIT(lowByte(GP_FeatureReportLength(c)));
      EMIT(highByte(GP_FeatureReportLength(c)));
    }

    // Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    EMIT(0xB1);
    EMIT(0x02);
  }

  // END_COLLECTION (Application)
  EMIT(0xc0);
  g->hidReportDescriptorSize = hidReportDescriptorSize;
}

/* The generic part of BleGamepad::begin() (padding bits, special-button positions, report size). */
void gp_begin(gp_state *g)
{
  const gp_config *c = &g->cfg;
  unsigned int i, pos = 0, numOfSpecialButtonBytes, numOfAxisBytes, numOfSimulationBytes, numOfMotionBytes = 0;
  g->hidReportDescriptorSize = 0;
  g->hidReportSize = 0;
  g->genericButtonPaddingBits = B8(8 - (GP_ButtonCount(c) % 8));
  if (g->genericButtonPaddingBits == 8) g->genericButtonPaddingBits = 0;
  g->genericSpecialButtonPaddingBits = B8(8 - (GP_TotalSpecialButtonCount(c) % 8));
  if (g->genericSpecialButtonPaddingBits == 8) g->genericSpecialButtonPaddingBits = 0;
  for (i = 0; i < GP_POSSIBLESPECIALBUTTONS; i++)
  {
    g->specialButtonPositions[i] = B8(pos);
    if (c->whichSpecialButtons[i]) pos++;
  }
  numOfAxisBytes = GP_AxisCount(c) * 2;
  numOfSimulationBytes = GP_SimulationCount(c) * 2;
  g->numOfButtonBytes = B8(GP_ButtonCount(c) / 8);
  if (g->genericButtonPaddingBits > 0) g->numOfButtonBytes++;
  numOfSpecialButtonBytes = GP_TotalSpecialButtonCount(c) / 8;
  if (g->genericSpecialButtonPaddingBits > 0) numOfSpecialButtonBytes++;
  if (GP_IncludeAccelerometer(c)) numOfMotionBytes += 6;
  if (GP_IncludeGyroscope(c)) numOfMotionBytes += 6;
  g->hidReportSize = g->numOfButtonBytes + numOfSpecialButtonBytes + numOfAxisBytes + numOfSimulationBytes + numOfMotionBytes + GP_HatSwitchCount(c);
  gp_build_descriptor(g);
}

#define PUT16(v) do { m[currentReportIndex++] = B8(v); m[currentReportIndex++] = B8((v) >> 8); } while (0)

unsigned int gp_pack_report(gp_state *g, gp_byte *m)   /* sendGenericReport(); m has hidReportSize entries */
{
  const gp_config *c = &g->cfg;
  unsigned int currentReportIndex = 0;
  memset(m, 0, g->hidReportSize * sizeof(gp_byte));
  memcpy(m, g->buttons, sizeof(g->buttons));   /* as the library: all 16 button bytes, later fields overwrite */
  currentReportIndex += g->numOfButtonBytes;
  if (GP_TotalSpecialButtonCount(c) > 0) m[currentReportIndex++] = g->specialButtons;
  if (GP_IncludeXAxis(c)) PUT16(g->x);
  if (GP_IncludeYAxis(c)) PUT16(g->y);
  if (GP_IncludeZAxis(c)) PUT16(g->z);
  if (GP_IncludeRzAxis(c)) PUT16(g->rZ);
  if (GP_IncludeRxAxis(c)) PUT16(g->rX);
  if (GP_IncludeRyAxis(c)) PUT16(g->rY);
  if (GP_IncludeSlider1(c)) PUT16(g->slider1);
  if (GP_IncludeSlider2(c)) PUT16(g->slider2);
  if (GP_IncludeRudder(c)) PUT16(g->rudder);
  if (GP_IncludeThrottle(c)) PUT16(g->throttle);
  if (GP_IncludeAccelerator(c)) PUT16(g->accelerator);
  if (GP_IncludeBrake(c)) PUT16(g->brake);
  if (GP_IncludeSteering(c)) PUT16(g->steering);
  if (GP_IncludeGyroscope(c)) { PUT16(g->gX); PUT16(g->gY); PUT16(g->gZ); }
  if (GP_IncludeAccelerometer(c)) { PUT16(g->aX); PUT16(g->aY); PUT16(g->aZ); }
  if (GP_HatSwitchCount(c) > 0)
  {
    signed char hats[4];
    int i;
    hats[0] = g->hat1; hats[1] = g->hat2; hats[2] = g->hat3; hats[3] = g->hat4;
    for (i = GP_HatSwitchCount(c) - 1; i >= 0; i--) m[currentReportIndex++] = B8(hats[i]);
  }
  return g->hidReportSize;
}

void gp_press(gp_state *g, gp_byte b)
{
  gp_byte index = (gp_byte)((b - 1) / 8), bit = (gp_byte)((b - 1) % 8);
  g->buttons[index] = B8(g->buttons[index] | (1u << bit));
}
void gp_release(gp_state *g, gp_byte b)
{
  gp_byte index = (gp_byte)((b - 1) / 8), bit = (gp_byte)((b - 1) % 8);
  g->buttons[index] = B8(g->buttons[index] & ~(1u << bit));
}
static gp_byte special_pos(const gp_state *g, gp_byte b) { return b >= GP_POSSIBLESPECIALBUTTONS ? 0 : g->specialButtonPositions[b]; }
void gp_press_special(gp_state *g, gp_byte b)
{
  gp_byte bit = special_pos(g, b) % 8;
  g->specialButtons = B8(g->specialButtons | (1u << bit));
}
void gp_release_special(gp_state *g, gp_byte b)
{
  gp_byte bit = special_pos(g, b) % 8;
  g->specialButtons = B8(g->specialButtons & ~(1u << bit));
}
void gp_set_axes(gp_state *g, gp_i16 x, gp_i16 y, gp_i16 z, gp_i16 rX, gp_i16 rY, gp_i16 rZ, gp_i16 s1, gp_i16 s2)
{
  g->x = x; g->y = y; g->z = z; g->rZ = rZ; g->rX = rX; g->rY = rY; g->slider1 = s1; g->slider2 = s2;  /* clampAxis is identity in this commit */
}
void gp_set_hat1(gp_state *g, signed char h) { g->hat1 = h; }
