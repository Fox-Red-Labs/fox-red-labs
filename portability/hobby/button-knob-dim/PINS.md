# Build 1, Button, knob, dimmer: pins

Written by `portability/hobby/gen_boards.py` from its pin table; the firmware is built from the same table. Header positions are the BoosterPack numbers 1 to 40 printed as J1 (pins 1 to 10), J2 (11 to 20), J3 (21 to 30) and J4 (31 to 40). A pin reads "J2 pin 14 (PB7)": header, pin number, chip pin.

Parts: LED with a 330 ohm resistor (on the LP-MSPM0G3519 the board's LED2 green does it with no wire), 10 kOhm potentiometer, the board's S2 button.

Power and returns: Potentiometer 10 kOhm: one end to J1 pin 1 (3V3), the other end to J2 pin 20 (GND), wiper to the ADC pin.

## LP-MSPM0G3519

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J4 pins 34 (PA10 TX) and 33 (PA11 RX), through the probe). Power: LP-MSPM0G3519 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital input, internal pull-up | BTN_A | J4 pin 40 (PB3) | S2 button on the board (to ground); an outside button between this pin and ground works too |
| timer PWM output | LED | J3 pin 28 (PB27) | LED2 green on the board; an outside LED (anode to the pin, 330 ohm in series, cathode to GND) works too |
| ADC input | POT1 | J1 pin 2 (PA14) | potentiometer 1 wiper |

## LP-MSPM0G3507

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J4 pins 34 (PA10 TX) and 33 (PA11 RX), through the probe). Power: LP-MSPM0G3507 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital input, internal pull-up | BTN_A | onboard (PB21) | S2 button on the board (to ground, on the QEI connector J27 pin 4); no wire needed |
| timer PWM output | LED | J4 pin 40 (PB4) | LED anode, 330 ohm in series, cathode to GND |
| ADC input | POT1 | J1 pin 2 (PA25) | potentiometer 1 wiper |

## LP-MSPM0L1306

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J1 pins 4 (PA8 TX) and 3 (PA9 RX), through the probe). Power: LP-MSPM0L1306 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital input, internal pull-up | BTN_A | onboard (PA14) | S2 button on the board (to ground); no wire needed |
| timer PWM output | LED | J4 pin 40 (PA12) | LED anode, 330 ohm in series, cathode to GND |
| ADC input | POT1 | J1 pin 2 (PA25) | potentiometer 1 wiper |

