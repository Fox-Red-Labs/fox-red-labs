# Parking sensor that beeps faster as you get close

An RCWL-1601 ultrasonic sensor and a buzzer; the closer you get, the faster it beeps and the serial port prints the distance.

The page: site/project-hobby-parking-sensor.html. This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## RCWL-1601 parking sensor

An ultrasonic sensor and a passive buzzer.

Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/hobby/parking-sensor//AGENTS.md`):

```bash
git sparse-checkout add portability/hobby/parking-sensor/
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | the board |
| Adafruit RCWL-1601 ultrasonic distance sensor (Adafruit 4007) | 1 | distance sensor; it runs at 3.3 V so its echo pin is safe for the LaunchPad |
| Freenove Ultimate Starter Kit for Raspberry Pi Pico, board not included (FNK0058) | 1 | passive buzzer with a 330 Ohm resistor in series, jumper wires and a breadboard |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 https://github.com/Fox-Red-Labs/fox-red-labs.git`. This page calls the folder you clone it into `<repository folder>`; every command below runs from inside it.
- Three free TI tools from ti.com: the MSPM0 SDK, SysConfig and TI Arm Clang. The build stops and names the missing tool and its download page when one is absent.

### Wiring

Power the sensor from 3.3 V. The kit's own HC-SR04 is a 5 V part whose echo pin would put 5 V on the LaunchPad, so leave it in the box. The kit's passive buzzer is a magnetic one: wire a 330 Ohm resistor in series with its plus leg, because straight on a pin it draws more current than the chip's pin may give.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| RCWL-1601 VCC | J1 pin 1 (3V3) | power, 3.3 V | required |
| RCWL-1601 GND | J2 pin 20 (GND) | ground | required |
| RCWL-1601 Trig | J4 pin 35 (PB5) | trigger pulse out | required |
| RCWL-1601 Echo | J4 pin 36 (PB4) | echo pulse in, timed on a free-running counter | required |
| Passive buzzer, plus leg, through a 330 Ohm resistor | J2 pin 14 (PB7) | tone output (a PWM pin) | required |
| Passive buzzer, other leg | J2 pin 20 (GND) | ground | required |

### Steps

1. **Bench.** With the USB cable unplugged, wire the sensor and the buzzer as in the pin table. Put the sensor where you can wave a hand in front of it.

   You should see: Four wires to the sensor and two to the buzzer. Nothing is lit.

   If the sensor has a header with SCL and SDA printed beside Trig and Echo, use the Trig and Echo pins: the other pair is for its serial modes.

2. **Terminal.** Build and flash the program. Plug the board into your computer with the USB cable first.

   ```text
   cd portability/hobby/parking-sensor && ./build.sh --board mspm0g3519 --flash
   ```

   You should see: The build finishes and the flash reports done. Build time: measured at the first run.

   If the build stops and names a missing tool, install that tool from the page it names and run the command again. If the flash cannot find the board, try another USB cable: a charge-only cable looks the same.

3. **Bench.** Hold your hand about 30 cm in front of the sensor, then move it slowly toward the sensor.

   You should see: The buzzer beeps slowly at first and faster as your hand gets closer, and the serial port (115200 baud) prints a line like `dist=34 cm` ten times a second. Beyond 100 cm it stays silent.

   If the serial port prints `dist=none`, check the echo wire first: a loose echo wire reads as no object in range.

4. **Bench.** Hold a ruler beside the sensor and hold a flat object at 10, 30 and 100 cm.

   You should see: The distance the program prints at each mark matches the ruler. The error at each distance: measured at the first run.

   If the readings are far too large, soft or angled surfaces reflect little sound; use a flat, hard object.

5. **Bench.** Open `portability/hobby/parking-sensor/app.c` in a text editor. Change the distance where the beeping stops, the gap between beeps, or the pitch. Run the command from step 2 after each change.

   You should see: The change shows within seconds of the flash.

   If the old behavior is still there, the flash did not finish: run the command from the build step again and wait for done.

### Expected result

The beep rate rises as the object gets closer. Distance error at three ranges, the measuring rate and image size: measured at the first run.

### What this shows

- Distance is the time an echo takes to return, halved and multiplied by the speed of sound.
- A timer capture times the echo without the program waiting on it.

### Limits

- That the program runs: nobody has run this build yet.
- How accurate it is: the error at three ranges is measured at the first run.
- The sensor reads a cone, not a line, and soft surfaces return little echo.
- The kit's HC-SR04 is a 5 V part and is not used here.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size on the LP-MSPM0G3507 | 3.1 kB flash of 128 kB, 524 B RAM of 32 kB (the linker reserves 512 B of the RAM for the stack) | `portability/hobby/README.md` |
| Size on the LP-MSPM0G3519 | 3.1 kB flash of 512 kB, 268 B RAM of 128 kB | `portability/hobby/README.md` |
| Size on the LP-MSPM0L1306 | 3.1 kB flash of 64 kB, 268 B RAM of 4 kB | `portability/hobby/README.md` |
| Build | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS at -Os, SysConfig 1.28.0, built 2026-11-08 | `portability/hobby/README.md` |

## RCWL-1601 parking sensor

An ultrasonic sensor and a passive buzzer.

Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/hobby/parking-sensor//AGENTS.md`):

```bash
git sparse-checkout add portability/hobby/parking-sensor/
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board |
| Adafruit RCWL-1601 ultrasonic distance sensor (Adafruit 4007) | 1 | distance sensor; it runs at 3.3 V so its echo pin is safe for the LaunchPad |
| Freenove Ultimate Starter Kit for Raspberry Pi Pico, board not included (FNK0058) | 1 | passive buzzer with a 330 Ohm resistor in series, jumper wires and a breadboard |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 https://github.com/Fox-Red-Labs/fox-red-labs.git`. This page calls the folder you clone it into `<repository folder>`; every command below runs from inside it.
- Three free TI tools from ti.com: the MSPM0 SDK, SysConfig and TI Arm Clang. The build stops and names the missing tool and its download page when one is absent.

### Wiring

Power the sensor from 3.3 V. The kit's own HC-SR04 is a 5 V part whose echo pin would put 5 V on the LaunchPad, so leave it in the box. The kit's passive buzzer is a magnetic one: wire a 330 Ohm resistor in series with its plus leg, because straight on a pin it draws more current than the chip's pin may give.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| RCWL-1601 VCC | J1 pin 1 (3V3) | power, 3.3 V | required |
| RCWL-1601 GND | J2 pin 20 (GND) | ground | required |
| RCWL-1601 Trig | J4 pin 35 (PB13) | trigger pulse out | required |
| RCWL-1601 Echo | J4 pin 36 (PB20) | echo pulse in, timed on a free-running counter | required |
| Passive buzzer, plus leg, through a 330 Ohm resistor | J2 pin 14 (PB7) | tone output (a PWM pin) | required |
| Passive buzzer, other leg | J2 pin 20 (GND) | ground | required |

### Steps

1. **Bench.** With the USB cable unplugged, wire the sensor and the buzzer as in the pin table. Put the sensor where you can wave a hand in front of it.

   You should see: Four wires to the sensor and two to the buzzer. Nothing is lit.

   If the sensor has a header with SCL and SDA printed beside Trig and Echo, use the Trig and Echo pins: the other pair is for its serial modes.

2. **Terminal.** Build and flash the program. Plug the board into your computer with the USB cable first.

   ```text
   cd portability/hobby/parking-sensor && ./build.sh --board mspm0g3507 --flash
   ```

   You should see: The build finishes and the flash reports done. Build time: measured at the first run.

   If the build stops and names a missing tool, install that tool from the page it names and run the command again. If the flash cannot find the board, try another USB cable: a charge-only cable looks the same.

3. **Bench.** Hold your hand about 30 cm in front of the sensor, then move it slowly toward the sensor.

   You should see: The buzzer beeps slowly at first and faster as your hand gets closer, and the serial port (115200 baud) prints a line like `dist=34 cm` ten times a second. Beyond 100 cm it stays silent.

   If the serial port prints `dist=none`, check the echo wire first: a loose echo wire reads as no object in range.

4. **Bench.** Hold a ruler beside the sensor and hold a flat object at 10, 30 and 100 cm.

   You should see: The distance the program prints at each mark matches the ruler. The error at each distance: measured at the first run.

   If the readings are far too large, soft or angled surfaces reflect little sound; use a flat, hard object.

5. **Bench.** Open `portability/hobby/parking-sensor/app.c` in a text editor. Change the distance where the beeping stops, the gap between beeps, or the pitch. Run the command from step 2 after each change.

   You should see: The change shows within seconds of the flash.

   If the old behavior is still there, the flash did not finish: run the command from the build step again and wait for done.

### Expected result

The beep rate rises as the object gets closer. Distance error at three ranges, the measuring rate and image size: measured at the first run.

### What this shows

- Distance is the time an echo takes to return, halved and multiplied by the speed of sound.
- A timer capture times the echo without the program waiting on it.

### Limits

- That the program runs: nobody has run this build yet.
- How accurate it is: the error at three ranges is measured at the first run.
- The sensor reads a cone, not a line, and soft surfaces return little echo.
- The kit's HC-SR04 is a 5 V part and is not used here.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size on the LP-MSPM0G3507 | 3.1 kB flash of 128 kB, 524 B RAM of 32 kB (the linker reserves 512 B of the RAM for the stack) | `portability/hobby/README.md` |
| Size on the LP-MSPM0G3519 | 3.1 kB flash of 512 kB, 268 B RAM of 128 kB | `portability/hobby/README.md` |
| Size on the LP-MSPM0L1306 | 3.1 kB flash of 64 kB, 268 B RAM of 4 kB | `portability/hobby/README.md` |
| Build | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS at -Os, SysConfig 1.28.0, built 2026-11-08 | `portability/hobby/README.md` |
