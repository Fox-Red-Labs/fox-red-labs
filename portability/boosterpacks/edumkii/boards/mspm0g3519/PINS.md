# LP-MSPM0G3519 pin map for the Educational BoosterPack MKII

BoosterPack pin number, the LaunchPad pin it reaches, and what to do about it.

Terms: [docs/vocab.md](../../../../../docs/vocab.md).

| BoosterPack pin | Function | LaunchPad pin | Note |
|---|---|---|---|
| 2 | joystick X (analog) | PA14 |  |
| 5 | joystick press | PB29 |  |
| 6 | microphone (analog) | PA25 |  |
| 7 | screen clock | PB31 |  |
| 9 | I2C clock (light sensor) | PC2 |  |
| 10 | I2C data (light sensor) | PC3 |  |
| 13 | screen select | PB12 |  |
| 15 | screen data in | PB8 |  |
| 17 | screen reset | PA30 |  |
| 23 | accelerometer X (analog) | PA27 |  |
| 24 | accelerometer Y (analog) | PA26 |  |
| 25 | accelerometer Z (analog) | PB24 |  |
| 26 | joystick Y (analog) | PB25 |  |
| 31 | screen data/command | PA13 |  |
| 32 | button S2 | PA12 |  |
| 33 | button S1 | PA11 | PA11 reaches the XDS110 backchannel through J13 (default): move J13 to 1-2 to give S1 its own pin (the console then goes quiet) |
| 34 | gator clip (console TX in the hello example) | PA10 | PA10 is the console TX (J12 default): the gator-clip pin carries the console |
| 37 | blue LED | PC5 |  |
| 38 | green LED | PC4 |  |
| 39 | red LED or backlight (J5) | PB9 | pins 39 and 40 default to two halves of one timer channel; GPIO use is unaffected |
| 40 | buzzer | PB3 | PB3 is also button S2 through J23: pull J23 |
