# Build 3, Parking sensor: pins

Written by `portability/hobby/gen_boards.py` from its pin table; the firmware is built from the same table. Header positions are the BoosterPack numbers 1 to 40 printed as J1 (pins 1 to 10), J2 (11 to 20), J3 (21 to 30) and J4 (31 to 40). A pin reads "J2 pin 14 (PB7)": header, pin number, chip pin.

Parts: RCWL-1601 ultrasonic module (HC-SR04 pinout, 3.3 V), passive buzzer.

Power and returns: RCWL-1601: VCC to J1 pin 1 (3V3), GND to J2 pin 20 (GND). Run it at 3.3 V so ECHO never exceeds 3.3 V. Buzzer: passive, other leg to GND.

## LP-MSPM0G3519

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J4 pins 34 (PA10 TX) and 33 (PA11 RX), through the probe). Power: LP-MSPM0G3519 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital output | TRIG | J4 pin 35 (PB5) | RCWL-1601 TRIG |
| digital input, internal pull-up | ECHO | J4 pin 36 (PB4) | RCWL-1601 ECHO |
| timer PWM output | BUZZ | J2 pin 14 (PB7) | passive buzzer, other side to GND |

## LP-MSPM0G3507

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J4 pins 34 (PA10 TX) and 33 (PA11 RX), through the probe). Power: LP-MSPM0G3507 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital output | TRIG | J4 pin 35 (PB13) | RCWL-1601 TRIG |
| digital input, internal pull-up | ECHO | J4 pin 36 (PB20) | RCWL-1601 ECHO |
| timer PWM output | BUZZ | J2 pin 14 (PB7) | passive buzzer, other side to GND |

## LP-MSPM0L1306

Serial, nothing to wire: 115200 8N1 on the probe's first serial port (J1 pins 4 (PA8 TX) and 3 (PA9 RX), through the probe). Power: LP-MSPM0L1306 J1 pin 1 is 3V3, J2 pin 20 and J3 pin 22 are GND, J3 pin 21 is 5V.

| Function | Signal | Pin on the board | Notes |
|---|---|---|---|
| digital output | TRIG | J2 pin 18 (PA7) | RCWL-1601 TRIG |
| digital input, internal pull-up | ECHO | J4 pin 36 (PA10) | RCWL-1601 ECHO |
| timer PWM output | BUZZ | J2 pin 14 (PA4) | passive buzzer, other side to GND |

