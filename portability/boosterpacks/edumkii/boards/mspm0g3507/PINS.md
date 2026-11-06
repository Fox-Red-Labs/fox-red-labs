# LP-MSPM0G3507 pin map for the Educational BoosterPack MKII

BoosterPack pin number, the LaunchPad pin it reaches, and what to do about it.

Terms: [docs/vocab.md](../../../../../docs/vocab.md).

| BoosterPack pin | Function | LaunchPad pin | Note |
|---|---|---|---|
| 2 | joystick X (analog) | PA25 |  |
| 5 | joystick press | PA26 | PA26 carries the on-board light sensor input through J18: pull J18 |
| 6 | microphone (analog) | PB24 | PB24 carries the on-board thermistor through J9: pull J9 |
| 7 | screen clock | PB9 |  |
| 9 | I2C clock (light sensor) | PB2 |  |
| 10 | I2C data (light sensor) | PB3 |  |
| 13 | screen select | PB6 |  |
| 15 | screen data in | PB8 |  |
| 17 | screen reset | PB15 |  |
| 23 | accelerometer X (analog) | PB19 |  |
| 24 | accelerometer Y (analog) | PA22 | PA22 is the light-sensor op-amp output through J16: pull J16 |
| 25 | accelerometer Z (analog) | PB18 |  |
| 26 | joystick Y (analog) | PA18 | PA18 is also button S1/BSL through J8: pull J8 |
| 31 | screen data/command | PA13 |  |
| 32 | button S2 | PA12 |  |
| 33 | button S1 | PA11 | PA11 is the backchannel RX through J22: move J22 to 2-3 for S1 |
| 34 | gator clip (console TX in the hello example) | PA10 | PA10 is the console TX through J21 (default): the gator-clip pin carries the console |
| 37 | blue LED | PA31 |  |
| 38 | green LED | PA28 |  |
| 39 | red LED or backlight (J5) | PB1 |  |
| 40 | buzzer | PB4 |  |
