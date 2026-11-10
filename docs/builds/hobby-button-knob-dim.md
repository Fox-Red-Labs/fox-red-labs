# Button, knob and dimmer on a breadboard

The board's S2 button toggles an LED, then a knob sets how bright it is.

The page: site/project-hobby-button-knob-dim.html. This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Button, knob and dimmer

The board's S2 button, one knob, one LED, on a breadboard.

Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/hobby/button-knob-dim//AGENTS.md`):

```bash
git sparse-checkout add portability/hobby/button-knob-dim/
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | the board, with the S2 button the build reads |
| Freenove Ultimate Starter Kit for Raspberry Pi Pico, board not included (FNK0058) | 1 | a red LED, a 220 to 330 Ohm resistor and a 10 kOhm knob (potentiometer) |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 https://github.com/Fox-Red-Labs/fox-red-labs.git`. This page calls the folder you clone it into `<repository folder>`; every command below runs from inside it.
- Three free TI tools from ti.com: the MSPM0 SDK, SysConfig and TI Arm Clang. The build stops and names the missing tool and its download page when one is absent.

### Wiring

The button is S2 on the board and needs no wire. The knob's outer pins go to 3.3 V and ground, and its middle pin to the analog pin. The LED goes from its pin through the resistor to ground. Serial output needs no wiring: it goes through the board's USB debugger at 115200 baud.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| Button S2 | onboard (PB3) | the board's S2 button; no wire needed | required |
| LED, long leg, through the resistor | J3 pin 28 (PB27) | PWM output | required |
| LED, short leg | J2 pin 20 (GND) | ground | required |
| Knob, middle pin (wiper) | J1 pin 2 (PA14) | analog input | required |
| Knob, outer pin 1 | J1 pin 1 (3V3) | power, 3.3 V only | required |
| Knob, outer pin 2 | J2 pin 20 (GND) | ground | required |

### Steps

1. **Bench.** With the USB cable unplugged, put the LED with its resistor and the knob on the breadboard and wire them as in the pin table. The button is S2 on the board, so it needs no wire.

   You should see: Two small circuits on the breadboard, each ending at the LaunchPad. Nothing is lit.

   If the LED never lights later, turn it around: the long leg goes to the resistor side.

2. **Terminal.** Build and flash the program. Plug the board into your computer with the USB cable first.

   ```text
   cd portability/hobby/button-knob-dim && ./build.sh --board mspm0g3519 --flash
   ```

   You should see: The build finishes and the flash reports done. Build time: measured at the first run.

   If the build stops and names a missing tool, install that tool from the page it names and run the command again. If the flash cannot find the board, try another USB cable: a charge-only cable looks the same.

3. **Bench.** Press S2, the button on the board.

   You should see: The LED toggles on each press.

   If nothing happens, check that the LED's resistor leg is on J3 pin 28 and its other leg on ground. The pins are listed in `portability/hobby/button-knob-dim/PINS.md`.

4. **Bench.** Turn the knob slowly from one end to the other.

   You should see: The LED gets brighter as you turn one way and dimmer the other way. The serial port (115200 baud) prints a line like `adc=914 led=on duty=22` twice a second.

   If it only jumps between off and full, the middle pin is on a digital pin or the outer pins are swapped; check the pin table.

5. **Bench.** Open `portability/hobby/button-knob-dim/app.c` in a text editor. Change `PWM_PERIOD_US`, stop the knob at half brightness, or make S2 hold the LED on for two seconds. Run the command from step 2 after each change.

   You should see: The change shows within seconds of the flash.

   If the old behavior is still there, the flash did not finish: run the command from the build step again and wait for done.

### Expected result

S2 toggles the LED and the knob sets its brightness from off to full. The PWM rate is 1 kHz by design; the measured brightness range and build time: measured at the first run.

### What this shows

- A knob, an LED and one resistor connect to a LaunchPad with five wires; the button is already on the board.
- Digital input, analog input and PWM output are three separate peripherals; this build uses each once.

### Limits

- That the program runs: nobody has run this build yet.
- Any timing or brightness figure: those are measured at the first run.
- The LED is driven straight from a pin through 220 Ohm.
- The pins are listed in the build folder's PINS.md, one table per board.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size on the LP-MSPM0G3507 | 3.2 kB flash of 128 kB, 712 B RAM of 32 kB (the linker reserves 512 B of the RAM for the stack) | `portability/hobby/README.md` |
| Size on the LP-MSPM0G3519 | 3.2 kB flash of 512 kB, 456 B RAM of 128 kB | `portability/hobby/README.md` |
| Size on the LP-MSPM0L1306 | 3.1 kB flash of 64 kB, 268 B RAM of 4 kB | `portability/hobby/README.md` |
| Build | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS at -Os, SysConfig 1.28.0, built 2026-11-08 | `portability/hobby/README.md` |

## Button, knob and dimmer

The board's S2 button, one knob, one LED, on a breadboard.

Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/hobby/button-knob-dim//AGENTS.md`):

```bash
git sparse-checkout add portability/hobby/button-knob-dim/
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board, with the S2 button the build reads |
| Freenove Ultimate Starter Kit for Raspberry Pi Pico, board not included (FNK0058) | 1 | a red LED, a 220 to 330 Ohm resistor and a 10 kOhm knob (potentiometer) |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 https://github.com/Fox-Red-Labs/fox-red-labs.git`. This page calls the folder you clone it into `<repository folder>`; every command below runs from inside it.
- Three free TI tools from ti.com: the MSPM0 SDK, SysConfig and TI Arm Clang. The build stops and names the missing tool and its download page when one is absent.

### Wiring

The button is S2 on the board and needs no wire. The knob's outer pins go to 3.3 V and ground, and its middle pin to the analog pin. The LED goes from its pin through the resistor to ground. Serial output needs no wiring: it goes through the board's USB debugger at 115200 baud.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| Button S2 | onboard (PB21) | the board's S2 button; no wire needed | required |
| LED, long leg, through the resistor | J4 pin 40 (PB4) | PWM output | required |
| LED, short leg | J2 pin 20 (GND) | ground | required |
| Knob, middle pin (wiper) | J1 pin 2 (PA25) | analog input | required |
| Knob, outer pin 1 | J1 pin 1 (3V3) | power, 3.3 V only | required |
| Knob, outer pin 2 | J2 pin 20 (GND) | ground | required |

### Steps

1. **Bench.** With the USB cable unplugged, put the LED with its resistor and the knob on the breadboard and wire them as in the pin table. The button is S2 on the board, so it needs no wire.

   You should see: Two small circuits on the breadboard, each ending at the LaunchPad. Nothing is lit.

   If the LED never lights later, turn it around: the long leg goes to the resistor side.

2. **Terminal.** Build and flash the program. Plug the board into your computer with the USB cable first.

   ```text
   cd portability/hobby/button-knob-dim && ./build.sh --board mspm0g3507 --flash
   ```

   You should see: The build finishes and the flash reports done. Build time: measured at the first run.

   If the build stops and names a missing tool, install that tool from the page it names and run the command again. If the flash cannot find the board, try another USB cable: a charge-only cable looks the same.

3. **Bench.** Press S2, the button on the board.

   You should see: The LED toggles on each press.

   If nothing happens, check that the LED's resistor leg is on J4 pin 40 and its other leg on ground. The pins are listed in `portability/hobby/button-knob-dim/PINS.md`.

4. **Bench.** Turn the knob slowly from one end to the other.

   You should see: The LED gets brighter as you turn one way and dimmer the other way. The serial port (115200 baud) prints a line like `adc=914 led=on duty=22` twice a second.

   If it only jumps between off and full, the middle pin is on a digital pin or the outer pins are swapped; check the pin table.

5. **Bench.** Open `portability/hobby/button-knob-dim/app.c` in a text editor. Change `PWM_PERIOD_US`, stop the knob at half brightness, or make S2 hold the LED on for two seconds. Run the command from step 2 after each change.

   You should see: The change shows within seconds of the flash.

   If the old behavior is still there, the flash did not finish: run the command from the build step again and wait for done.

### Expected result

S2 toggles the LED and the knob sets its brightness from off to full. The PWM rate is 1 kHz by design; the measured brightness range and build time: measured at the first run.

### What this shows

- A knob, an LED and one resistor connect to a LaunchPad with five wires; the button is already on the board.
- Digital input, analog input and PWM output are three separate peripherals; this build uses each once.

### Limits

- That the program runs: nobody has run this build yet.
- Any timing or brightness figure: those are measured at the first run.
- The LED is driven straight from a pin through 220 Ohm.
- The pins are listed in the build folder's PINS.md, one table per board.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size on the LP-MSPM0G3507 | 3.2 kB flash of 128 kB, 712 B RAM of 32 kB (the linker reserves 512 B of the RAM for the stack) | `portability/hobby/README.md` |
| Size on the LP-MSPM0G3519 | 3.2 kB flash of 512 kB, 456 B RAM of 128 kB | `portability/hobby/README.md` |
| Size on the LP-MSPM0L1306 | 3.1 kB flash of 64 kB, 268 B RAM of 4 kB | `portability/hobby/README.md` |
| Build | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS at -Os, SysConfig 1.28.0, built 2026-11-08 | `portability/hobby/README.md` |
