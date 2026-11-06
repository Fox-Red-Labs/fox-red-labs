void RefGamepad::buildGenericDescriptor()
{
  // USAGE_PAGE (Generic Desktop)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

  // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
  tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getControllerType();

  // COLLECTION (Application)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0xa1;
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

  // REPORT_ID (Default: 3)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85;
  tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getHidReportId();

  if (configuration.getButtonCount() > 0)
  {
    // USAGE_PAGE (Button)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;

    // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // LOGICAL_MAXIMUM (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // REPORT_SIZE (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE_MINIMUM (Button 1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x19;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE_MAXIMUM (Up to 128 buttons possible)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x29;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getButtonCount();

    // REPORT_COUNT (# of buttons)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getButtonCount();

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    if (genericButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = genericButtonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;
    }
  }

  if (configuration.getTotalSpecialButtonCount() > 0)
  {
    // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // LOGICAL_MAXIMUM (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // REPORT_SIZE (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    if (configuration.getDesktopSpecialButtonCount() > 0)
    {
      // USAGE_PAGE (Generic Desktop)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getDesktopSpecialButtonCount();

      if (configuration.getIncludeStart())
      {
        // USAGE (Start)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3D;
      }

      if (configuration.getIncludeSelect())
      {
        // USAGE (Select)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3E;
      }

      if (configuration.getIncludeMenu())
      {
        // USAGE (App Menu)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x86;
      }

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
    }

    if (configuration.getConsumerSpecialButtonCount() > 0)
    {
      // USAGE_PAGE (Consumer Page)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0C;

      // REPORT_COUNT
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getConsumerSpecialButtonCount();

      if (configuration.getIncludeHome())
      {
        // USAGE (Home)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0A;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x23;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
      }

      if (configuration.getIncludeBack())
      {
        // USAGE (Back)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0A;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x24;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
      }

      if (configuration.getIncludeVolumeInc())
      {
        // USAGE (Volume Increment)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xE9;
      }

      if (configuration.getIncludeVolumeDec())
      {
        // USAGE (Volume Decrement)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xEA;
      }

      if (configuration.getIncludeVolumeMute())
      {
        // USAGE (Mute)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xE2;
      }

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
    }

    if (genericSpecialButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = genericSpecialButtonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;
    }
  }

  if (configuration.getAxisCount() > 0)
  {
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMax());

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (configuration.getAxisCount() - sliders)
    uint8_t sliders = (configuration.getIncludeSlider1() ? 1 : 0) + (configuration.getIncludeSlider2() ? 1 : 0);
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getAxisCount() - sliders;

    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    if (configuration.getIncludeXAxis())
    {
      // USAGE (X)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x30;
    }

    if (configuration.getIncludeYAxis())
    {
      // USAGE (Y)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x31;
    }

    if (configuration.getIncludeZAxis())
    {
      // USAGE (Z)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x32;
    }

    if (configuration.getIncludeRzAxis())
    {
      // USAGE (Rz)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
    }

    if (configuration.getIncludeRxAxis())
    {
      // USAGE (Rx)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;
    }

    if (configuration.getIncludeRyAxis())
    {
      // USAGE (Ry)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;
    }

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    if (configuration.getIncludeSlider1())
    {
      // REPORT_COUNT (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // USAGE (Slider)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // END_COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC0;
    }

    if (configuration.getIncludeSlider2())
    {
      // REPORT_COUNT (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // USAGE (Slider)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // END_COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC0;
    }

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }

  if (configuration.getSimulationCount() > 0)
  {
    // USAGE_PAGE (Simulation Controls)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMax());

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (configuration.getSimulationCount())
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getSimulationCount();

    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    if (configuration.getIncludeRudder())
    {
      // USAGE (Rudder)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBA;
    }

    if (configuration.getIncludeThrottle())
    {
      // USAGE (Throttle)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBB;
    }

    if (configuration.getIncludeAccelerator())
    {
      // USAGE (Accelerator)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC4;
    }

    if (configuration.getIncludeBrake())
    {
      // USAGE (Brake)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC5;
    }

    if (configuration.getIncludeSteering())
    {
      // USAGE (Steering)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC8;
    }

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }

  // Gyroscope
  if (configuration.getIncludeGyroscope())
  {
    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Gyroscope - Rotational X - Rx)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;

    // USAGE (Rotational - Rotational Y - Ry)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;

    // USAGE (Rotational - Rotational Z - Rz)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (3)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }

  // Accelerometer
  if (configuration.getIncludeAccelerometer())
  {
    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Accelerometer - Vector X - Vx)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x40;

    // USAGE (Accelerometer - Vector Y - Vy)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x41;

    // USAGE (Accelerometer - Vector Z - Vz)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x42;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (3)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }

  if (configuration.getHatSwitchCount() > 0)
  {
    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = USAGE_PAGE(1);
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Hat Switch)
    for (int currentHatIndex = 0; currentHatIndex < configuration.getHatSwitchCount(); currentHatIndex++)
    {
      tempHidReportDescriptor[hidReportDescriptorSize++] = USAGE(1);
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x39;
    }

    // Logical Min (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Max (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    // Physical Min (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Physical Max (315)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x46;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3B;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Unit (SI Rot : Ang Pos)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x65;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x12;

    // Report Size (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    // Report Count (4)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getHatSwitchCount();

    // Input (Data, Variable, Absolute)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x42;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }

  if (configuration.getEnableOutputReport())
  {
    // Usage Page (Vendor Defined 0xFF00)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x06;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;

    // Usage (Vendor Usage 0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Usage (0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Minimum (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Logical Maximum (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Report Size (8 bits)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    if (configuration.getOutputReportLength() <= 0xFF)
    {
      // Report Count (0~255 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getOutputReportLength();
    }
    else
    {
      // Report Count (0~65535 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x96;
      tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getOutputReportLength());
      tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getOutputReportLength());
    }

    // Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x91;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
  }

  if (configuration.getEnableFeatureReport())
  {
    // Vendor blob feature report
    // Usage Page (Vendor Defined 0xFF00)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x06;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;

    // Usage (Vendor Usage 0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Usage (0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Minimum (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Logical Maximum (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Report Size (8 bits)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    if (configuration.getFeatureReportLength() <= 0xFF)
    {
      // Report Count (0~255 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getFeatureReportLength();
    }
    else
    {
      // Report Count (0~65535 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x96;
      tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getFeatureReportLength());
      tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getFeatureReportLength());
    }

    // Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xB1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
  }

  // END_COLLECTION (Application)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
}

void RefGamepad::sendGenericReport()
{
  uint8_t currentReportIndex = 0;

  uint8_t m[hidReportSize];

  memset(&m, 0, sizeof(m));
  memcpy(&m, &_buttons, sizeof(_buttons));

  currentReportIndex += numOfButtonBytes;

  if (configuration.getTotalSpecialButtonCount() > 0)
  {
    m[currentReportIndex++] = _specialButtons;
  }

  if (configuration.getIncludeXAxis())
  {
    m[currentReportIndex++] = _x;
    m[currentReportIndex++] = (_x >> 8);
  }
  if (configuration.getIncludeYAxis())
  {
    m[currentReportIndex++] = _y;
    m[currentReportIndex++] = (_y >> 8);
  }
  if (configuration.getIncludeZAxis())
  {
    m[currentReportIndex++] = _z;
    m[currentReportIndex++] = (_z >> 8);
  }
  if (configuration.getIncludeRzAxis())
  {
    m[currentReportIndex++] = _rZ;
    m[currentReportIndex++] = (_rZ >> 8);
  }
  if (configuration.getIncludeRxAxis())
  {
    m[currentReportIndex++] = _rX;
    m[currentReportIndex++] = (_rX >> 8);
  }
  if (configuration.getIncludeRyAxis())
  {
    m[currentReportIndex++] = _rY;
    m[currentReportIndex++] = (_rY >> 8);
  }

  if (configuration.getIncludeSlider1())
  {
    m[currentReportIndex++] = _slider1;
    m[currentReportIndex++] = (_slider1 >> 8);
  }
  if (configuration.getIncludeSlider2())
  {
    m[currentReportIndex++] = _slider2;
    m[currentReportIndex++] = (_slider2 >> 8);
  }

  if (configuration.getIncludeRudder())
  {
    m[currentReportIndex++] = _rudder;
    m[currentReportIndex++] = (_rudder >> 8);
  }
  if (configuration.getIncludeThrottle())
  {
    m[currentReportIndex++] = _throttle;
    m[currentReportIndex++] = (_throttle >> 8);
  }
  if (configuration.getIncludeAccelerator())
  {
    m[currentReportIndex++] = _accelerator;
    m[currentReportIndex++] = (_accelerator >> 8);
  }
  if (configuration.getIncludeBrake())
  {
    m[currentReportIndex++] = _brake;
    m[currentReportIndex++] = (_brake >> 8);
  }
  if (configuration.getIncludeSteering())
  {
    m[currentReportIndex++] = _steering;
    m[currentReportIndex++] = (_steering >> 8);
  }

  if (configuration.getIncludeGyroscope())
  {
    m[currentReportIndex++] = _gX;
    m[currentReportIndex++] = (_gX >> 8);
    m[currentReportIndex++] = _gY;
    m[currentReportIndex++] = (_gY >> 8);
    m[currentReportIndex++] = _gZ;
    m[currentReportIndex++] = (_gZ >> 8);
  }
  
  if (configuration.getIncludeAccelerometer())
  {
    m[currentReportIndex++] = _aX;
    m[currentReportIndex++] = (_aX >> 8);
    m[currentReportIndex++] = _aY;
    m[currentReportIndex++] = (_aY >> 8);
    m[currentReportIndex++] = _aZ;
    m[currentReportIndex++] = (_aZ >> 8);
  }
  
  if (configuration.getHatSwitchCount() > 0)
  {
    signed char hats[4];

    hats[0] = _hat1;
    hats[1] = _hat2;
    hats[2] = _hat3;
    hats[3] = _hat4;

    for (int currentHatIndex = configuration.getHatSwitchCount() - 1; currentHatIndex >= 0; currentHatIndex--)
    {
      m[currentReportIndex++] = hats[currentHatIndex];
    }
  }

  #if BLE_GAMEPAD_DEBUG == 1
    dumpHIDReport(m, sizeof(m));
  #endif

  this->inputGamepad->setValue(m, sizeof(m));
  this->inputGamepad->notify();
}

void RefGamepad::press(uint8_t b)
{
  uint8_t index = (b - 1) / 8;
  uint8_t bit = (b - 1) % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _buttons[index] | bitmask;

  if (result != _buttons[index])
  {
    _buttons[index] = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void RefGamepad::release(uint8_t b)
{
  uint8_t index = (b - 1) / 8;
  uint8_t bit = (b - 1) % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _buttons[index] & ~bitmask;

  if (result != _buttons[index])
  {
    _buttons[index] = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

uint8_t RefGamepad::specialButtonBitPosition(uint8_t b)
{
  if (b >= POSSIBLESPECIALBUTTONS)
  {
    return 0;
  }
  return _specialButtonPositions[b];
}

void RefGamepad::pressSpecialButton(uint8_t b)
{
  uint8_t button = specialButtonBitPosition(b);
  uint8_t bit = button % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _specialButtons | bitmask;

  if (result != _specialButtons)
  {
    _specialButtons = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void RefGamepad::releaseSpecialButton(uint8_t b)
{
  uint8_t button = specialButtonBitPosition(b);
  uint8_t bit = button % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _specialButtons & ~bitmask;

  if (result != _specialButtons)
  {
    _specialButtons = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void RefGamepad::pressStart()
{
  pressSpecialButton(START_BUTTON);
}

void RefGamepad::releaseStart()
{
  releaseSpecialButton(START_BUTTON);
}

void RefGamepad::pressSelect()
{
  pressSpecialButton(SELECT_BUTTON);
}

void RefGamepad::releaseSelect()
{
  releaseSpecialButton(SELECT_BUTTON);
}

void RefGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t slider1, int16_t slider2)
{
  x = clampAxis(x);
  y = clampAxis(y);
  z = clampAxis(z);
  rZ = clampAxis(rZ);
  rX = clampAxis(rX);
  rY = clampAxis(rY);
  slider1 = clampAxis(slider1);
  slider2 = clampAxis(slider2);

  _x = x;
  _y = y;
  _z = z;
  _rZ = rZ;
  _rX = rX;
  _rY = rY;
  _slider1 = slider1;
  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void RefGamepad::setHat1(signed char hat1)
{
  _hat1 = hat1;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void RefGamepad::resetButtons()
{
  memset(&_buttons, 0, sizeof(_buttons));
}

void RefGamepad::begin(BleGamepadConfiguration *config)
{
  BleGamepadConfiguration defaultConfig;
  configuration = config ? *config : defaultConfig;

  // begin() may be called again to re-apply a changed configuration. Reset the
  // sizes so they are recomputed from scratch: hidReportDescriptorSize indexes
  // tempHidReportDescriptor as the descriptor is assembled below, so a stale
  // non-zero value would append past the previous descriptor and overflow the
  // fixed buffer. (They start at 0 from the constructor on the first call.)
  hidReportDescriptorSize = 0;
  hidReportSize = 0;

  GamepadMode mode = configuration.getGamepadMode();
  if (mode == GamepadMode::SInput || mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
  {
    configuration.setIncludeSlider1(false);
    configuration.setIncludeSlider2(false);
    configuration.setIncludeRudder(false);
    configuration.setIncludeThrottle(false);
    configuration.setIncludeAccelerator(false);
    configuration.setIncludeBrake(false);
    configuration.setIncludeSteering(false);
    configuration.setIncludeGyroscope(false);
    configuration.setIncludeAccelerometer(false);
    configuration.setEnableOutputReport(false);
    configuration.setEnableFeatureReport(false);
    configuration.setHatSwitchCount(1);

    if (mode == GamepadMode::SInput)
      configuration.setButtonCount(25);
    else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
    {
      configuration.setButtonCount(11);
      deviceName = "Xbox Wireless Controller";
      deviceManufacturer = "Microsoft";
      // Mystfit's XBOX_1708_SERIAL / XBOX_1914_SERIAL — ASCII hex serials
      // from real controller BLE captures, used by the Xbox driver for matching.
      if (mode == GamepadMode::XInputSeriesX)
        configuration.setSerialNumber("3039373130303637313034303231");
      else
        configuration.setSerialNumber("3033363030343037323136373239");
    }

    if (mode == GamepadMode::SInput)
    {
      if (configuration.getVid() != SINPUT_USB_VID || configuration.getPid() != SINPUT_USB_PID_GENERIC)
        Serial.println("WARNING: SInput mode requires VID 0x2E8A / PID 0x10C6 for host recognition. Do not override setVid()/setPid().");
    }
    else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
    {
      if (configuration.getVid() != XINPUT_USB_VID)
        Serial.println("WARNING: XInputOneS/XInputSeriesX mode requires VID 0x045E for Xbox driver recognition. Do not override setVid()/setPid().");
    }
  }

  enableOutputReport = configuration.getEnableOutputReport();
  outputReportLength = configuration.getOutputReportLength();
  enableFeatureReport = configuration.getEnableFeatureReport();
  featureReportLength = configuration.getFeatureReportLength();
  enableSInput = configuration.getEnableSInput();

  genericButtonPaddingBits = 8 - (configuration.getButtonCount() % 8);
  if (genericButtonPaddingBits == 8)
  {
    genericButtonPaddingBits = 0;
  }
  genericSpecialButtonPaddingBits = 8 - (configuration.getTotalSpecialButtonCount() % 8);
  if (genericSpecialButtonPaddingBits == 8)
  {
    genericSpecialButtonPaddingBits = 0;
  }

  // Precompute bit positions for each special button so press/release don't
  // need to iterate the configuration array on every call.
  uint8_t pos = 0;
  for (int i = 0; i < POSSIBLESPECIALBUTTONS; i++)
  {
    _specialButtonPositions[i] = pos;
    if (configuration.getWhichSpecialButtons()[i]) pos++;
  }

  uint8_t numOfAxisBytes = configuration.getAxisCount() * 2;
  uint8_t numOfSimulationBytes = configuration.getSimulationCount() * 2;

  numOfButtonBytes = configuration.getButtonCount() / 8;
  if (genericButtonPaddingBits > 0)
  {
    numOfButtonBytes++;
  }

  uint8_t numOfSpecialButtonBytes = configuration.getTotalSpecialButtonCount() / 8;
  if (genericSpecialButtonPaddingBits > 0)
  {
    numOfSpecialButtonBytes++;
  }
  
  uint8_t numOfMotionBytes = 0;
  if (configuration.getIncludeAccelerometer())
  {
    numOfMotionBytes += 6;
  }
  
  if (configuration.getIncludeGyroscope())
  {
    numOfMotionBytes += 6;
  }

  hidReportSize = numOfButtonBytes + numOfSpecialButtonBytes + numOfAxisBytes + numOfSimulationBytes + numOfMotionBytes + configuration.getHatSwitchCount();

  if (mode == GamepadMode::SInput)
  {
    enableSInput = true;
    buildSInputDescriptor();
  }
  else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
  {
    enableSInput = false;
    buildXInputDescriptor();
  }
  else
  {
    enableSInput = false;
    buildGenericDescriptor();
  }

  // tempHidReportDescriptor is a fixed buffer. A very large configuration (max
  // buttons + every axis/simulation control + motion + hats + feature/output
  // reports) can approach its capacity; if it is ever exceeded the writes above
  // have already run past the end of the array, so fail loud here rather than
  // ship a silently corrupt descriptor. getHidReportDescriptorSize() lets a
  // caller watch the headroom.
  if (hidReportDescriptorSize > (int)sizeof(tempHidReportDescriptor))
  {
    NIMBLE_LOGE(LOG_TAG, "HID report descriptor (%d bytes) overflowed its %u-byte buffer", hidReportDescriptorSize, (unsigned)sizeof(tempHidReportDescriptor));
  }

  // Set task priority from 5 to 1 in order to get ESP32-C3 working
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 1, NULL);
}

