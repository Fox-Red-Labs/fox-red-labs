# LP-MSPM33C321A pin map for the Educational BoosterPack MKII

BoosterPack pin number, the LaunchPad pin it reaches, and what to do about it.

**Warning, BoosterPack pin 5.** Never drive PA21. On this LaunchPad BoosterPack pin 5 has no chip connection (R21 not fitted), and PA21 is the VREF- pin, tied to ground through R10. A pack that puts a high level on this pin drives that ground. A pack microphone that needs power on this pin needs a jumper from pin 8 to pin 5.

Terms: [docs/vocab.md](../../../../../docs/vocab.md).

| BoosterPack pin | Function | LaunchPad pin | Note |
|---|---|---|---|
| 2 | joystick X (analog) | PA18 | PA18 is also the BSL button through J3 (not button S1, which is PA11): pull J3 |
| 5 | joystick press | PA21 | WARNING: Never drive PA21. On this LaunchPad BoosterPack pin 5 has no chip connection (R21 not fitted), and PA21 is the VREF- pin, tied to ground through R10. A pack that puts a high level on this pin drives that ground. A pack microphone that needs power on this pin needs a jumper from pin 8 to pin 5 |
| 6 | microphone (analog) | PA22 |  |
| 7 | screen clock | PB31 |  |
| 9 | I2C clock (light sensor) | PB13 |  |
| 10 | I2C data (light sensor) | PB12 |  |
| 13 | screen select | PB28 |  |
| 15 | screen data in | PB8 |  |
| 17 | screen reset | PC23 |  |
| 23 | accelerometer X (analog) | PA17 |  |
| 24 | accelerometer Y (analog) | PA16 |  |
| 25 | accelerometer Z (analog) | PA15 |  |
| 26 | joystick Y (analog) | PB27 |  |
| 31 | screen data/command | PC22 |  |
| 32 | button S2 | PC21 |  |
| 33 | button S1 | PA11 | PA11 is the backchannel UART RX; the hello example uses TX only |
| 34 | gator clip (console TX in the hello example) | PA10 | PA10 is the backchannel UART TX (BoosterPack gator-clip pin): console output |
| 37 | blue LED | PC19 |  |
| 38 | green LED | PC18 |  |
| 39 | red LED or backlight (J5) | PC29 | J5 on the BoosterPack picks backlight (2-3) or red LED (1-2) |
| 40 | buzzer | PC17 | PC17 is also button S2 through J4: pull J4 |
