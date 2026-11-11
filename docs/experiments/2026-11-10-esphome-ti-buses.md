<!-- experiment
id: E321
board: LP-EM-CC2340R5
date: 2026-11-10
status: waiting
reached: run
line: ESPHome's `i2c:` bus, a new `ti_adc` sensor and a new `ti_pwm` output now build and run on TI's I2C, ADC and PWM drivers on the cc27xx and cc23xx platforms. PWM is verified on the LP-EM-CC2755P10: a 1 kHz timer period of 47,999 counts held compare values of 11,999 and 35,999 for 25 percent and 75 percent. The I2C bus scans and the HDC2080 and OPT3001 drivers start, but no reading was taken because the sensors BoosterPack was not on the board when the test ran; the ADC reads a floating pin (1.0 to 1.8 V) and the jumper test was not done. SPI and UART buses were not done.
stop: hardware
-->
# Can ESPHome's stock sensor and output components run on TI's I2C, ADC and PWM drivers?

**Short answer.** The software side works; the I2C and ADC proofs need a connected part. `i2c:` runs on TI's I2C driver and ESPHome's own `hdc2080` and `opt3001` components load on it. `ti_pwm` drives an LGPT timer through TI's PWM driver for `output:` and `light: monochromatic`, and the timer's compare register shows the right duty. `ti_adc` reads an analog-capable DIO through TI's ADC driver. ESPHome's `adc`, `zephyr_pwm` and `i2c` code is built for nRF devicetree, so `ti_adc` and `ti_pwm` are new components and `i2c:` reuses ESPHome's class with a TI driver underneath.

Terms: [vocab](../vocab.md). PWM is pulse-width modulation. LGPT is the chip's general-purpose timer. The sensors BoosterPack is BP-BASSENSORSMKII.

**Question.** Do `i2c:`, an analog input and a PWM output work from an ESPHome YAML on the TI platforms, with ESPHome's own components on top?

**What was expected.** TI's drivers are configured by SysConfig, so each bus and pin has to be a SysConfig instance generated from the YAML. With the BoosterPack on the LP-EM-CC2340R5 (no wires), the HDC2080 at 0x40 and the OPT3001 at 0x44 should answer on DIO0 and DIO24 (the header's I2C pins). A pin tied to 3V3 should read near 3.3 V. ESPHome's `i2c`, `spi` and `uart` components are limited to a list of platforms, so the platform has to let its names through.

**What was tried.**

1. `periph.py` turns `i2c:`, `ti_adc` and `ti_pwm` entries into SysConfig instances with the pins from the YAML, checked against the pin table (a signal the pin cannot carry is refused), and writes a header the shim reads.
2. `shim/i2c_bus_f3.cpp` implements ESPHome's `ZephyrI2CBus` class on `I2C_transfer`; `shim/ti_periph.cpp` has the ADC and PWM calls the two components use.
3. PWM on the LP-EM-CC2755P10 red LED (DIO16, LGPT3): light cycled between 25 percent and 75 percent with gamma off, the timer registers read with DSLite.
4. I2C on the LP-EM-CC2340R5 with nothing plugged on, then ADC on the LP-EM-CC2755P10 with DIO21 and DIO22 open.
5. SPI and UART buses: ESPHome's schemas for both name their platforms in the schema itself and the generated code needs a platform class; not attempted past reading them.

**How far it got.** PWM verified; I2C and ADC ran without a connected part.

| Step | Result | Cause tag | What fixed it |
|---|---|---|---|
| `i2c:` in the YAML | `This feature is only available on [esp32, esp8266, rp2, nrf52, host]` | Third-party behavior | The platform wraps ESPHome's platform gate so its names pass wherever nrf52 does |
| `spi:` | The same message, and the list has no nrf52 | Third-party behavior | Not fixed: the generated code needs an SPI class for the platform |
| SysConfig, ADC pin | `DIO21` is not a pin name on the CC2755P10 | Our mistake | Use the name with its analog suffix, `DIO21_A4` |
| SysConfig, PWM on DIO14 | Resource conflict with TI's red LED GPIO instance | Tool or docs | Drop that instance when a driver takes the pin; keep its macro as the DIO number |
| Compile, `light` | `static assertion failed: std::numeric_limits<float>::is_iec559` | Third-party behavior | TI's C++ library reports false for IEEE float; the assertion is removed in the build copy |
| PWM, CC2755P10 | Period 0xBB7F (47,999), compare 0x2EDF at 25 percent and 0x8C9F at 75 percent, matching 11,999 and 35,999 | | |
| I2C scan, CC2340R5, no BoosterPack | Reports one device at 0x3C; the HDC2080 start fails with "Communication failed" | Setup or handling | The zero-length address probe is a one-byte write; with no pull-ups the bus floats. A reading needs the BoosterPack |
| ADC, CC2755P10, open pins | 1.01 to 1.84 V from DIO21 and DIO22, drifting | Setup or handling | Floating inputs; a jumper to 3V3 and to GND is the test |

**What this does not establish.** No humidity, light or voltage reading checked against anything. The I2C probe for a bus scan may report false devices on a bus with no pull-ups. One PWM output per timer, so at most as many as the chip has LGPT timers. No SPI and no UART bus.

**Ask.** Can ESPHome's bus components declare their platform support through a hook, not through `only_on` lists in each schema?

**Effort.** About 3 hours to the first run of all three; the I2C and ADC proofs wait for the bench step.

**Verdict.** Partly done. The drivers work from a YAML; the reading proofs are the next step.
