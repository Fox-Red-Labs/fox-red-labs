<!-- experiment
id: E292
board: LP-EM-CC2340R5
date: 2026-10-30
status: in progress
reached: boot
line: A Bluetooth text display firmware (Nordic UART to an SSD1306 OLED) builds in 195.1 kB of flash with 3.5 kB of RAM left, flashes and boots on the LP-EM-CC2340R5; the Bluetooth echo and the display are not yet run.
stop: none
-->
# Can the LP-EM-CC2340R5 drive a text display from Bluetooth messages?

**Short answer.** Not shown yet.

**Question.** Can the LP-EM-CC2340R5 show text a Mac sends over Bluetooth on an SSD1306 OLED, in the RAM the radio stack leaves?

**Vendor position.** TI's `basic_ble` example and I2C driver are used unchanged (SimpleLink Low Power F3 SDK 9.21); nothing there promises the combination.

**Could work.** A Mac pushes text lines over the Nordic UART Service to an SSD1306 128x64 OLED on the LP-EM-CC2340R5's I2C pins. The firmware builds, flashes, boots, advertises and finds no display, as it should with nothing wired. The Bluetooth echo and the display itself were not run.

Terms: [docs/vocab.md](../vocab.md). Measured 2026-10-30 on an Intel Mac, macOS 15.7.9: SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0, one LP-EM-CC2340R5 (probe LS470GTU).

## Expected and measured

| Check | Expected (written before the build) | Measured |
|---|---|---|
| Build | about 40 s, as the Nordic port | Built; flash 195.1 kB, RAM 33.4 kB used, 3.5 kB left (a frame buffer would not fit, so the driver writes one text line per I2C transfer) |
| Flash | about 20 s | Success, backup of main flash (512 kB) and CCFG (2 kB) taken first |
| Boot, console at 115200 | advertises, reports the display | `Adv status: Started`, `Nordic ... up`, `OLED not found (I2C 0x3C, SCL DIO24, SDA DIO0)` |
| Mac pushes three lines, each echoed | echo equals the line, about 24 ms | Not run: Python's Bluetooth access is refused for the agent shell (macOS aborts the process), and from the Terminal panel the scan returned nothing in 75 s |
| Display draws lines | text on the OLED | Not run: not wired |

## What it is

- `cc2340r5/desk-display/app/app_desk.c`: the Nordic port's app file plus a command parser. A line `N:text` (N 1 to 8) sets display line N; `cls` clears. Every accepted line is also printed on the console as `DRAW N:text`, so Bluetooth can be proven without the display.
- `app/oled.c`, `oled.h`, `font5x7.h`: SSD1306 driver on TI's I2C driver, address 0x3C, 21 characters by 8 lines.
- `build.sh`: copies TI's `basic_ble`, applies `ports/nordic-apps/basic_ble.patch`, adds the I2C instance (the Sensors BoosterPack's I2C pins) and the objects. Device name "FRL Desk".
- `host/desk.py`: `test`, `msg "text"` and `clock` (time, date and, with DESK_LAT and DESK_LON set, weather from Open-Meteo, no key). `web/index.html`: Web Bluetooth page with link state, advertisement signal strength where the browser allows it, echo time and a live copy of the screen.
- Pins: I2C SCL is J1 pin 9 (DIO24), SDA is J1 pin 10 (DIO0), 3V3 is J1 pin 1, GND is J3 pin 22. Neither pin is the LEDs (DIO14, DIO15), the buttons (DIO9, DIO10), the console UART (DIO20, DIO22) or the SPI pins. The board has no I2C pull-ups; the OLED module's own pull-ups serve.

## Honest limits

- The font table is typed from memory of the common 5x7 table and has not been seen on a display.
- The display address 0x3C is the usual one for the ELEGOO module; a module strapped to 0x3D shows nothing.
- Bluetooth connects from a Mac have crashed the Mac's `bluetoothd` before (see `ports/nordic-apps/README.md`).

## At the bench, 2026-10-31

The OLED was wired with SCL and SDA swapped at first: the firmware's new bus scan (status line 4 on the console) read "bus scan: none" at 100 kHz. With the two data wires the right way round it reads "OLED found; i2c open 1; bus scan: 3C" and the screen draws. A blank scan with the I2C driver open points at wiring first. The bus runs at 100 kHz now; 400 kHz was never tried with the display wired.

Bluetooth link from the Mac (Terminal.app, 2026-10-31): `run.sh test` saw "FRL Desk" at -67 dBm, sent two text lines and a clear, and got all three echoes back. Round trips 45.0 and 45.9 ms for a text line, 134.5 ms for a clear (it redraws all eight lines). Median 45.9 ms.
