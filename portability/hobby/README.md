# Hobby builds 1 to 6 as portable programs for MSPM0

Six small programs of the kind hobby kits teach, each one `app.c` over one small layer, built unchanged for the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM0L1306. Only the pins and the SysConfig file differ per board, and a script writes both from one table. Layout and idea follow [../three-ways/README.md](../three-ways/README.md).

Terms: [docs/vocab.md](../../docs/vocab.md). **Glue** is the code that connects `app.c` to one board's drivers. **SysConfig** is TI's tool that writes a chip's pin and clock setup. **PWM** is pulse-width modulation, a pin that switches on and off fast so its average level, or a servo's angle, can be set. 1 kB = 1,024 bytes. Built 2026-11-08 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS `-Os` and SysConfig 1.28.0 (the 1.26.3 that `build.sh` prefers is not installed on this Mac).

## Quick start

```bash
cd portability/hobby/button-knob-dim && ./build.sh --board mspm0g3507 --flash
```

Every build folder has the same `build.sh --board <mspm0g3507|mspm0g3519|mspm0l1306> [--flash] [--probe <serial>]`. It builds, and with `--flash` backs up, writes and checks the flash through `tools/frl/frl.py` (one LaunchPad on USB needs no `--probe`). `--board` is required. Pins are in each build's `PINS.md`.

Tested status:

- 1 button-knob-dim: LP-MSPM0G3519 ran it and its pins and timer were checked through the debugger; no human saw the LED, S2 and the pot were not exercised. Other boards built only.
- 2 led-sequences: LP-MSPM0G3519 ran it and its LED pins, serial lines and buzzer timer were checked through the debugger; no human saw the LEDs, no button was pressed. Other boards built only.
- 3 parking-sensor: built only on all three boards; logic passes on the host.
- 4 weather-station: built only on all three boards; logic passes on the host.
- 5 servo-pantilt: built only on all three boards; logic passes on the host.
- 6 pir-alarm: built only on all three boards; logic passes on the host.

## The six builds

| # | Folder | What it does | Parts | Pins |
|---|---|---|---|---|
| 1 | `button-knob-dim/` | A button toggles an LED, a potentiometer sets its brightness through 1 kHz PWM, and the ADC value goes out on the serial port twice a second (`adc=914 led=on duty=22`). | LED and 330 ohm (the LP-MSPM0G3519 has LED2 green on the same pin), 10 kOhm potentiometer, the board's S2 button | |
| 2 | `led-sequences/` | Three programs in a loop: a traffic light on three LEDs (red 3 s, green 3 s, yellow 1 s), a reaction game on two buttons (wait 2 to 5 s, green lights, the first press wins and its time prints in ms, an early press is a foul), and a 14-note melody on a passive buzzer. Every step prints a line. | Red, yellow, green LEDs, one button plus S2, passive buzzer | |
| 3 | `parking-sensor/` | RCWL-1601 (HC-SR04 protocol, 3.3 V): a 12 us trigger, the echo pulse timed on the free-running SysTick counter, distance in cm = echo us / 58 printed at 10 Hz (`dist=34 cm`). A buzzer beeps faster as the object comes closer: 40 ms beeps, gap 60 ms + 5 ms per cm, silent beyond 100 cm. | RCWL-1601, passive buzzer | |
| 4 | `weather-station/` | DHT11 one-wire read every 2 s, shown on an SSD1306 128x64 OLED with u8g2 in page mode (a 128-byte buffer) over a bit-banged I2C, and printed (`temp=23 C humidity=45 %`). | DHT11 module, SSD1306 OLED at 0x3C | |
| 5 | `servo-pantilt/` | Servo 1 follows potentiometer 1 and servo 2 follows potentiometer 2: 50 Hz, 1.0 to 2.0 ms pulse. Wire servo 1 first; the program drives both channels. | Two SG90 servos, two 10 kOhm potentiometers | |
| 6 | `pir-alarm/` | HC-SR501 on an input: each new motion adds one to a count on a four-digit TM1637 display, beeps 200 ms and prints a line. 30 s warm-up first (dashes on the display). Our own 30-line TM1637 driver in `app.c`. | HC-SR501, TM1637 display, passive buzzer | |

Every `PINS.md` lists, for each of the three boards, the function, the signal, the pin as "J2 pin 14 (PB7)" and the wiring note. They are written by `gen_boards.py` from the same table the firmware is built from, so a pin in a document cannot differ from a pin in the firmware. Where a part is listed in docs/arrivals/hobby-parts.md (RCWL-1601, TM1637, the OLED's I2C), the pins are that document's; for the rest the pins were chosen from each board's SDK board file and checked against the debug, backchannel serial, LED and sensor pins. Header positions match across the boards where the chips allow: buzzer J2 pin 14, second button J2 pin 15, PIR J2 pin 12, DHT J1 pin 7, potentiometers J1 pins 2 and 6, servos J4 pins 40 and 39, OLED J1 pins 9 and 10.

No pin used is a debug pin or a backchannel serial pin. The LP-MSPM0L1306 has its debug clock on J2 pin 13 (PA20, SWCLK), which is why its PIR sits on J2 pin 12.

## Layout

| Path | What |
|---|---|
| `common/hobby.h` | The portable layer: `hb_init`, `hb_us`, `hb_ms`, `hb_delay_*`, `hb_out`, `hb_in`, `hb_od` (pull a line low or let it go), `hb_adc`, `hb_pwm`, `hb_pwm_off`, `hb_puts`, `hb_printf`. Each `app.c` includes only this and `stdint.h` (build 4 adds `u8g2.h`). |
| `common/hobby_text.c` | A 100-line `printf` (`%u %d %s %c`, width and zero fill), no floating point or heap. |
| `driverlib/glue_dl.c` | The one glue file for the three boards, on TI DriverLib, bare metal. Time is SysTick free-running at the CPU clock. |
| `driverlib/build.sh` | Clean build for one build and one board. |
| `<build>/app.c` | The program. Build 4 also has `oled.c` (the two u8g2 callbacks). |
| `<build>/boards/<id>/` | `<build>.syscfg` and `hobby_pins.h`, both generated. |
| `<build>/PINS.md` | The pin table, generated. |
| `gen_boards.py` | The pin table and the generator. `--check` fails when a committed file is stale. |
| `host/` | Host tests of each app's logic against a fake HAL with a simulated clock (`run_host_tests.sh`). |
| `tools/` | `sizes.py` (flash and RAM from a linker map), `g3519_check.py` and `g3519_flash_test.sh` (the flash test). |
| `maps/` | The linker maps the sizes below come from. |

The u8g2 library is [../../third-party/u8g2/](../../third-party/u8g2/SOURCE.md), tag 2.37.1, two-clause BSD, with the two fonts it already carries; build 4 compiles its core and SSD1306 files and sets the display up in `oled.c` instead of `u8g2_d_setup.c`, which would pull in every display driver. No Freenove code is used (its license is CC BY-NC-SA); the RCWL-1601 timing, the DHT11 read and the TM1637 driver are written here. The TM1637 driver replaces the LGPL library `docs/arrivals/hobby-parts.md` names.

## Build

```bash
portability/hobby/build_all.sh                                    # gen check, host tests, all 18 builds, size table; about 1 minute
portability/hobby/driverlib/build.sh button-knob-dim mspm0g3519    # one build for one board
python3 portability/hobby/gen_boards.py                            # rewrite the SysConfig files, pin headers and PINS.md from the table
```

The image is `build/<build>-<board>/hobby.out` (git-ignored). Boards: `mspm0g3519`, `mspm0g3507`, `mspm0l1306`.

## Flash

Use the repo's one board path, under the probe lock, with your own probe serial:

```bash
python3 tools/preflight_image.py mspm0g3519 portability/hobby/build/button-knob-dim-mspm0g3519/hobby.out
python3 tools/frl/lock.py exec --serial <probe serial> -- python3 tools/frl/frl.py backup --board mspm0g3519 --probe <probe serial>
python3 tools/frl/lock.py exec --serial <probe serial> -- python3 tools/frl/frl.py flash --board mspm0g3519 --image portability/hobby/build/button-knob-dim-mspm0g3519/hobby.out --probe <probe serial>
```

Serial is 115200 8N1 on the probe's first port. Put the backup back with `frl restore <backup folder>`; it reads the flash back and compares. The whole flash test on the LP-MSPM0G3519 (backup, two builds, restore) is `tools/g3519_flash_test.sh`, written up in RESULTS.md. Only the LP-MSPM0G3519 was flashed; the other two boards were built.

## Sizes

Flash and RAM are the "used" column of the linker map's memory table (`maps/<build>-<board>.map`, read by `tools/sizes.py`), in bytes. RAM includes the stack the linker file reserves (512 bytes on the LP-MSPM0G3507, 256 on the other two) and, on builds with PWM, SysConfig's timer backup record (188 bytes for the one PWM timer of build 1 on the LP-MSPM0G3519, read from its map). Nothing writes the boot configuration area: all 18 maps show 0 bytes in `BCR_CONFIG` and `BSL_CONFIG`.

| Build | G3519 flash | G3519 RAM | G3507 flash | G3507 RAM | L1306 flash | L1306 RAM |
|---|--:|--:|--:|--:|--:|--:|
| 1 button-knob-dim | 3,256 | 456 | 3,232 | 712 | 3,192 | 268 |
| 2 led-sequences | 3,840 | 272 | 3,824 | 528 | 3,800 | 272 |
| 3 parking-sensor | 3,224 | 268 | 3,224 | 524 | 3,192 | 268 |
| 4 weather-station | 8,152 | 545 | 8,128 | 801 | 8,104 | 545 |
| 5 servo-pantilt | 3,080 | 456 | 3,064 | 712 | 3,032 | 268 |
| 6 pir-alarm | 3,496 | 268 | 3,488 | 524 | 3,456 | 268 |

The chips have 512, 128 and 64 kB of flash and 128, 32 and 4 kB of RAM. The biggest build (the OLED one, with u8g2 and a font) uses about 8 kB of flash and 0.5 kB of RAM, so all six fit the LP-MSPM0L1306.

## Limits

- Only builds 1 and 2 ran on a board. What the run showed and what it could not show is in RESULTS.md. Builds 3 to 6 are built and checked against a fake HAL on the host, not on their parts.
- The ADC reads against the supply (VDDA, about 3.3 V), not the internal reference, so a potentiometer between 3V3 and ground uses the full range.
- `hb_us` and `hb_ms` need a call at least every 0.5 s and wrap after 71 minutes; the apps call them far more often. SysTick runs at the CPU clock (32 MHz here).
- The DHT11 read is timed with busy waits and no interrupts are enabled in these programs. Use a DHT11 module with its pull-up, or add 4.7 to 10 kOhm to 3V3.
- On the LP-MSPM0L1306 the OLED's lines are PA0 and PA1, open-drain pins with no internal pull-up: pull jumper J2 (LED1 is on PA0), and put J9 and J10 to the 3V side or rely on the module's own pull-ups.
- The passive buzzer is driven at 50 percent duty on a timer pin; an active buzzer also works but ignores the melody's pitches.
