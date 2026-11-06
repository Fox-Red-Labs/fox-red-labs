# LP-MSPM0G5187 pin map for the Educational BoosterPack MKII

BoosterPack pin number, the LaunchPad pin it reaches, and what to do about it.

Terms: [docs/vocab.md](../../../../../docs/vocab.md).

| BoosterPack pin | Function | LaunchPad pin | Note |
|---|---|---|---|
| 2 | joystick X (analog) | PA25 |  |
| 5 | joystick press | PB21 |  |
| 6 | microphone (analog) | PB22 |  |
| 7 | screen clock | PB18 |  |
| 9 | I2C clock (light sensor) | PA9 |  |
| 10 | I2C data (light sensor) | PA8 |  |
| 13 | screen select | PA30 | SPI2 chip-select 2; the microSD slot shares SPI2 (pins 13 to 15) when a card is inserted: leave the slot empty |
| 15 | screen data in | PB17 | PB17 also drives the on-board RGB red LED through J12: pull J12 or the screen data line loads the LED |
| 17 | screen reset | PB23 |  |
| 23 | accelerometer X (analog) | PB14 |  |
| 24 | accelerometer Y (analog) | PB15 |  |
| 25 | accelerometer Z (analog) | PB16 |  |
| 26 | joystick Y (analog) | PA16 | PA16 is also the audio codec I2S master clock net (R36): not isolated by a jumper |
| 31 | screen data/command | PA1 | PA1 is I2C0 SCL, also used as the screen data/command line here |
| 32 | button S2 | PA0 | PA0 is also the red LED through J9: pull J9 |
| 33 | button S1 | PA11 | PA11 is the XDS110 backchannel RX with no routing jumper |
| 34 | gator clip (console TX in the hello example) | PA10 | PA10 is the XDS110 backchannel TX with no routing jumper: the gator-clip pin carries the console |
| 37 | blue LED | PB1 |  |
| 38 | green LED | PB0 |  |
| 39 | red LED or backlight (J5) | PB5 |  |
| 40 | buzzer | PB4 |  |
