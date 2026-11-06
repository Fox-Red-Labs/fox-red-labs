# LP-MSPM0L1306 pin map for the Educational BoosterPack MKII

BoosterPack pin number, the LaunchPad pin it reaches, and what to do about it.

Terms: [docs/vocab.md](../../../../../docs/vocab.md).

| BoosterPack pin | Function | LaunchPad pin | Note |
|---|---|---|---|
| 2 | joystick X (analog) | PA25 | PA25 reaches the on-board light sensor through J4: pull J4 |
| 5 | joystick press | PA24 | PA24 is not a GPIO in the VQFN-32 package (SysConfig lists no PA24): the press is read as analog channel 3 (ADC0.3) against 2048; PA24 also reaches the light sensor through J5: pull J5 |
| 6 | microphone (analog) | PA21 |  |
| 7 | screen clock | PA6 (SPI0 clock) |  |
| 9 | I2C clock (light sensor) | PA1 |  |
| 10 | I2C data (light sensor) | PA0 | PA0 is also the red LED D2 through J2: pull J2 or the LED loads the I2C data line |
| 13 | screen select | PA20 (SWCLK) | SWCLK: no screen select while the debugger is attached |
| 15 | screen data in | PA5 (SPI0 data out) |  |
| 17 | screen reset | PA19 (SWDIO) | SWDIO: no screen reset while the debugger is attached |
| 23 | accelerometer X (analog) | PA15 | PA15 carries the thermistor through J1: pull J1 |
| 24 | accelerometer Y (analog) | PA16 |  |
| 25 | accelerometer Z (analog) | PA17 (op-amp input, no ADC) | PA17 is an op-amp input: no accelerometer Z |
| 26 | joystick Y (analog) | PA18 |  |
| 31 | screen data/command | not connected | no pin: the screen has no data/command line |
| 32 | button S2 | not connected | no pin: no S2 |
| 33 | button S1 | not connected | no pin: no S1 |
| 34 | gator clip (console TX in the hello example) | not connected | no pin: the hello console is on pin 4 (PA8, UART TX through J16, the default routing to the probe) |
| 37 | blue LED | PA27 | PA27 is the on-board blue LED through J13 |
| 38 | green LED | PA26 | PA26 is the on-board red LED through J12 |
| 39 | red LED or backlight (J5) | PA13 | PA13 is the on-board green LED through J3 |
| 40 | buzzer | PA12 |  |
