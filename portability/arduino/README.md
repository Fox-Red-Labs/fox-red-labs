# Arduino libraries on a TI LP-MSPM0G3507: what ran

Partly. Four programs were built, and two ran on the board: `shimcheck` (this repo's own check of the layer) and `pdqgfx` (Arduino_GFX's own `PDQgraphicstest` example, which drew on the ST7796S screen). `touchpaint` and `pdqgfx_libmode` built and were not run. A 649-line Arduino API layer, written in this repo, sits under them.

Only one of the two Arduino libraries pulled from GitHub ran on the board: Arduino_GFX. `Adafruit_FT6206` built, inside `touchpaint`, and was not run. The touch chip was read through this repo's own `Wire` layer in `shimcheck`, not through that library. `digitalRead` has idle readings only (`S1=1 S2=1`, no button pressed).

What was built: the layer (in `core/`, 11 files), plus two real Arduino libraries taken from GitHub, `Arduino_GFX` (the ST7796S screen) and `Adafruit_FT6206` (the FT6336U touch chip). The libraries' own source files are unmodified. For the Arduino_GFX example, two of its own configuration headers were edited, as the example intends. The pin map is in `boards/mspm0g3507/variant.h`; library versions are under Libraries below.

Arduino's own Blink and AnalogReadSerial also run unchanged on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based Arduino core, which has `analogRead` and this layer does not: [zephyr/README.md](zephyr/README.md).

Terms: [vocab](../../docs/vocab.md). Questions and fixes: [FAQ](../../mspm0g3507/FAQ.md); build problems of this layer: [FAQ](FAQ.md). The screen and touch module, and the two traps found with it (Trap 1: a level translator on the module loses short chip-select pulses; Trap 2: the touch chip answers software I2C but not the chip's I2C block), are described in [display-test](../../mspm0g3507/display-test/README.md). Built and run 2026-10-06 on an Intel Mac, macOS 15.7, CCStudio 21.0.1.00010, MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS (C++17, no exceptions, no RTTI). Sizes in bytes unless marked kB (1 kB = 1,024 bytes).

Contents: [Run on the board](#run-on-the-board-2026-10-06), [Build and flash](#build-and-flash), [What the layer covers](#what-the-layer-covers), [Libraries](#libraries-source-unmodified), [Sketches](#sketches), [Sizes](#sizes), [Problems while building](#problems-while-building), [Answered and still open](#answered-and-still-open), [Files](#files).

## Run on the board (2026-10-06)

Values below were seen on the serial port or the screen and were not saved.

- **shimcheck: runs.** Serial printed a line every second: `millis=1000 micros=1001725 S1=1 S2=1 touch vendor/chip/fw=0x11 0x64 0x12`. Serial, SysTick time and the bit-banged `Wire` (I2C driven in software on two ordinary pins) ran, and the touch chip answered through Arduino's `Wire` API with the IDs the sketch expected. `digitalRead` gave idle readings only: `S1=1 S2=1`, with no button pressed. One reading was taken, and it was not saved.
- **pdqgfx: runs, library source unmodified.** Arduino_GFX's own PDQgraphicstest drew on the screen and printed its table. Screen fill (five full fills) took 2.55 s, about 0.51 s per fill, against 0.365 s for the hand-written driver in `display-test` at the same 8 MHz SPI. Other lines: text 0.18 s, lines 7.4 s, filled circles 0.84 s. Not checked: every test's picture for dropped commands.
- **touchpaint: built, not run.** It needs a person to draw on the screen. It is the only program that uses `Adafruit_FT6206`.
- **`pdqgfx_libmode`: built, not run.** It keeps the library's own SPI mode 2, to test the SPI mode entry in the [FAQ](FAQ.md).

Three points in those results need care:

- **`micros` runs 1,725 microseconds ahead of `millis` (0.17%).** The sketch reads `millis()`, then prints about 19 characters before it reads `micros()`, which at 115,200 baud takes about 1.65 ms. That nearly accounts for the gap. Not done: reading both counters back to back.
- **The fills are slower than the hand-written driver.** Not tested against a changed loop.
- **`shimcheck` treats S1 as active-low.** It toggles the LED only when `digitalRead(PIN_S1) == HIGH`. S1 on this LaunchPad is active-high, so a press leaves the pin high and the sketch would not show a press. Measured: the unpressed reading `S1=1`. Not measured: S1 pressed, because nobody pressed it on the board.

## Run on the LP-MSPM0G5187 (measured 2026-11-09)

`BOARD=mspm0g5187 ./build.sh` builds the layer for the MSPM0G5187 with a board folder (`boards/mspm0g5187/`: `arduino.syscfg`, `variant.h`, `board.sh`, `pincm_table.h`) and no display or touch libraries. Two changes to the shared layer: the serial interrupt handler takes its name from SysConfig's `ARD_UART_INST_IRQHandler` (the G5187 calls the block UC0, not UART0), and `wiring.cpp` uses a board folder's `pincm_table.h` when there is one. Blink (unchanged) and the new `boardcheck` sketch were flashed with DSLite and ran: PA17 (`LED_BUILTIN`) toggled with half periods of 1,000 ms and 500 ms (debug-port reads), and `boardcheck` printed `boardcheck: Arduino layer on LP-MSPM0G5187` and then `millis=1000 micros=1001729 led=0` once a second on the backchannel UART at 115200. Sizes: Blink 2,824 B flash, boardcheck 3,952 B, 6,348 and 6,360 B RAM (2,048 B stack and 4,096 B heap included). `AnalogReadSerial` does not compile (no `analogRead`), as on the G3507. Not run on this board: SPI and Wire (no module attached). Logs: mspm0g5187/bench/arduino/.

## LAUNCHXL-F2800137 (C28x): not supported

The layer's ArduinoCore-API needs C++11 and TI's C2000 compiler (cl2000 25.11.1.LTS) is C++03 only; the first error is `String.h` line 75, in all five API files. The board layer (SysTick, IOMUX table, DriverLib UART) was not rewritten for C2000Ware. Details: [`boards/f2800137/PORT.md`](boards/f2800137/PORT.md); log: `c2000-launchpad/bench/arduino/`.

## Build and flash

Build all four programs into `out/`. It takes about a minute the first time.

```bash
./build.sh
```

For a smaller build (see Sizes):

```text
OPT=-Os FORCE=1 ./build.sh
```

`build.sh` hard-codes the install paths and versions on this Mac: lines 8 to 10 set `SDK=/Applications/ti/mspm0_sdk_2_11_00_07`, `CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS` and `SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh`. Edit them if your versions or install folder differ. The installs are described in [mac-setup](../../mac-setup/README.md).

Flash one program with OpenOCD (replace `<out>` with one of the paths in the table below). The command has no `verify` step on purpose: on these images `program ... verify` reported a CRC mismatch (exact text not saved), while programming without it and running worked. **Unknown (suspected: Third-party behavior).** OpenOCD's checksum check also fails on other MSPM0 images and falls back to a full read-back, see the [FAQ](../../mspm0g3507/FAQ.md). These images were not read back, so a real mismatch is not ruled out.

```text
~/zephyr-toolchain/xpack-openocd-0.12.0-7/bin/openocd -s ~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" -f ~/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg -c "adapter serial MG350001" -c init -c "reset halt" -c "program <out>" -c "reset run" -c shutdown
```

Two parts of that command are specific to this bench. `MG350001` is the serial number of this bench's debug probe, so use your own probe's serial number. The OpenOCD folder, its scripts and the board file come from the project's Zephyr setup, described in zephyr/README.md.

| Program | `<out>` | What it is |
|---|---|---|
| `shimcheck` | `out/shimcheck/shimcheck.out` | This repo's own check of the layer, no display library. Flash this first. |
| `pdqgfx` | `out/pdqgfx/pdqgfx.out` | Arduino_GFX's `PDQgraphicstest` example. Draws on the screen, prints timings to Serial. |
| `pdqgfx_libmode` | `out/pdqgfx_libmode/pdqgfx_libmode.out` | The same sketch with the library's own SPI mode choice (mode 2), to test the SPI mode entry in the [FAQ](FAQ.md). Optional, not run. |
| `touchpaint` | `out/touchpaint/touchpaint.out` | Adafruit_FT6206's `CapTouchPaint` example with Arduino_GFX as the display. Not run. |

Serial is on `/dev/cu.usbmodemMG3500011` on this bench (the name carries the probe serial), 115200 baud, 8N1 (the XDS110 backchannel).

## What the layer covers

The API half is the **unmodified ArduinoCore-API 1.5.2** (Arduino's own portable layer: `Print`, `Stream`, `String`, `IPAddress`, the `pinMode` and `delay` declarations). Only what sits below it was added, in `core/`:

| Piece | File | What it does |
|---|---|---|
| Start-up | `main.cpp`, `wiring.cpp` | `main()` calls `init()`, `setup()`, then `loop()` forever. `init()` runs SysConfig's setup and starts SysTick. |
| GPIO | `wiring.cpp` | `pinMode` (INPUT, INPUT_PULLUP, INPUT_PULLDOWN, OUTPUT, OUTPUT_OPENDRAIN), `digitalWrite`, `digitalRead`, port-register macros for libraries with a fast-pin path. |
| Time | `wiring.cpp` | `millis`, `micros` (SysTick, 1 ms tick plus the counter, pending-tick safe), `delay`, `delayMicroseconds` (cycle loop), weak `yield`. |
| Random | `wiring.cpp` | `random`, `randomSeed` on the C library `rand`. |
| Serial | `HardwareSerial.*` | UART0, `begin(baud)` and `begin(baud, config)` for all 5 to 8 bit, parity, 1 or 2 stop-bit settings. Polled transmit, interrupt receive into a 128-byte ring. `print`, `println`, `write`, `available`, `read`, `peek`, `flush`. |
| SPI | `SPI.*` | SPI1, controller only. `beginTransaction` sets clock (fastest not above the request, from 32 MHz), mode 0 to 3 and bit order. `transfer` of one byte, two bytes, or a buffer (in place, full duplex). |
| Wire | `Wire.*` | I2C controller bit-banged on two GPIO pins. `begin`, `setClock`, `beginTransmission`, `write`, `endTransmission(stop)` with the usual 0/1/2/3 codes, `requestFrom`, `read`, `peek`, `available`. |
| Pin map | `variant.h` | One file per board, `boards/<board>/variant.h`. |
| SysConfig | `syscfg/arduino.syscfg`, `gen/` | 32 MHz clock, SPI1 on PB9/PB8/PB7, UART0 at 115200 on PA10/PA11. Nothing else. |

**Not implemented.** `analogRead`, `analogWrite`, `tone`, `pulseIn`, `shiftIn/Out`, `attachInterrupt` (and so pin interrupts), `Serial1`, a second SPI, `Wire` target mode (`begin(address)`, `onReceive`, `onRequest`), SPI `setClockDivider/setBitOrder/setDataMode`. These were not written, a scope choice, and whether the chip can support each was not checked. A sketch that calls one fails to compile or link. The exception: ArduinoCore-API forces `usingInterrupt`, `notUsingInterrupt`, `attachInterrupt` and `detachInterrupt` onto the SPI class. Those four stop the program at a breakpoint (`compat_unsupported`) instead of pretending to work. A pin number that does not exist (PB28 to PB31, 60 and up) stops at a breakpoint too (`compat_bad_pin`). Both leave a name or number in a RAM variable you can read with the debugger.

Behaviors a reader should know:

- `SPI.transfer(buf, n)` writes the bytes it receives back into `buf`, like every Arduino core. If `buf` is in flash (a library passing a `const` table through the in-place call), the write-back is skipped, because a store to flash would fault or be lost.
- `digitalWrite(PB6, HIGH)` waits 10 microseconds after raising the pin. This is how Trap 1 is guaranteed for the display's chip select (`CS_GAP_PIN` in `variant.h`). It applies to that pin only.
- `Serial` has no `Serial.printf`. ArduinoCore-API's `Print` does not define one, and neither does this layer.

## Libraries (source unmodified)

Four libraries are in `libraries/`: Arduino_GFX 1.6.8, Adafruit_FT6206 1.1.1, Adafruit_BusIO 1.17.4 and ArduinoCore-API 1.5.2. Their commits, licenses and checksums are in `../../third-party/arduino/SOURCES.txt` and `CHECKSUMS.sha256`.

Why this touch library: `Adafruit_FT6206` already lists the FT6336U chip ID (0x64) as valid, so no chip-ID work was needed. It needs `Adafruit_BusIO`. `strange-v/FT6X36` and `Adafruit_GFX` were cloned, not needed, and removed.

**Library source edits needed: zero.** Every `.cpp` in `Arduino_GFX` (all databus, display and canvas drivers, not just the ones used), `Adafruit_BusIO` (including its software-SPI file) and `Adafruit_FT6206` compiled as shipped, with 2 warnings in total (both "variable length arrays are a Clang extension" in `Arduino_TFT.cpp`). Compiled is not run: only Arduino_GFX has run on the board. The linker drops what a sketch does not call. `ArduinoCore-API` is compiled for `Print`, `Stream`, `String`, `IPAddress` and `Common` only.

## Sketches

| Sketch | From | Changed from upstream | Ran on the board |
|---|---|---|---|
| `sketches/pdqgfx` | Arduino_GFX `examples/PDQgraphicstest` | `PDQgraphicstest.ino` is byte-identical. Two of the example's own "edit me" headers changed: `Arduino_GFX_pins.h` (a new `ARDUINO_ARCH_MSPM0` pin block) and `Arduino_GFX_display.h` (ST7796 4 inch IPS line, rotation 4, plus a 5-line subclass that asks for SPI mode 0, see the [FAQ](FAQ.md)). | Yes. The `pdqgfx_libmode` build of it: no |
| `sketches/touchpaint` | Adafruit_FT6206 `examples/CapTouchPaint` | Diff in `CapTouchPaint.diff` (65 lines). Includes and constructor lines now use Arduino_GFX, `tft` is a reference to it, colors map to `RGB565_*`, the touch chip gets a reset pulse and the backlight is turned on, and the two `map()` flip lines are removed (with rotation 4 the touch chip reports screen coordinates, as `display-test` showed). The painting logic is untouched. | No |
| `sketches/shimcheck` | this repo's | Not an upstream example. Its S1 test assumes an active-low button, see the results above. | Yes |

Rotation 4 gives MADCTL 0x48, the same setting `display-test` used, so touch and pixels agree without flipping.

## Sizes

Measured with `tiarmsize` (TI's size-report tool) on the linked `.out`, TI Arm Clang `-O2` (the default). Flash is the `text` column. RAM is `data + bss`, and 6,144 bytes (6 kB) of that is the stack (2,048) and heap (4,096) the build reserves, so the true static use is small.

| Program | Flash | % of 128 kB | RAM incl. 6 kB reserve | `-Os` flash |
|---|---|---|---|---|
| `shimcheck` | 6,784 | 5% | 6,452 | not measured |
| `pdqgfx` | 77,432 | 59% | 7,373 | 50,888 |
| `pdqgfx_libmode` | 77,160 | 59% | 7,373 | not measured |
| `touchpaint` | 59,096 | 45% | 7,453 | 35,400 |

Where the bytes went (from the linker map, `tools/size_by_group.py`; flash only):

| Who | `pdqgfx` | `touchpaint` |
|---|---|---|
| Arduino_GFX | 48,513 | 44,713 |
| TI C and C++ runtime (libc, libc++, math, builtins) | 14,816 | 5,218 |
| Sketch | 7,342 | 1,296 |
| **This repo's shim** (`core/`, SysConfig output, start-up) | **3,693** | **5,855** |
| ArduinoCore-API | 2,505 | 723 |
| Adafruit_BusIO + Adafruit_FT6206 | 0 | 764 |
| TI driverlib | 362 | 362 |

Static RAM by the same split: shim 180 to 276 bytes, TI runtime about 950 bytes (the C library's standard-I/O buffers, pulled in by `printf` code), Arduino_GFX 10 bytes. The 128 kB flash and 32 kB RAM limits are not close. Arduino_GFX is big because its classes' virtual tables keep every drawing method alive even when a sketch calls few of them. No config macro was needed; `LITTLE_FOOT_PRINT` and a font trim were not run.

## Problems while building

Nine build and run-time problems came up, seven compile or link errors and two defaults changed before running. None needed an edit to a library source file. Each is an entry with its fix in [FAQ.md](FAQ.md).

## Answered and still open

Answered by the board run:

- The touch chip answers through `Wire`: the three ID registers read `0x11 0x64 0x12`. This used this repo's `Wire` layer, not `Adafruit_FT6206`.
- The SPI loop keeps up at 8 MHz, but it is slower than the hand-written driver (0.51 s per fill against 0.365 s). The cause is suspected, not tested.
- With Trap 1's 10 microsecond wait on every `digitalWrite(PB6, HIGH)`, the screen drew the test. If a screen still drops commands, raise `CS_GAP_US` in `variant.h`.

Still open:

- **Unknown.** `Serial` receive was written but nothing sends to it in these sketches, so it is untested.
- **Unknown.** `touchpaint` and `pdqgfx_libmode` have not run, so `Adafruit_FT6206` has not run either.
- **Unknown.** `digitalRead` with a pressed button has not been seen.


## Files

| Path | What it is |
|---|---|
| `build.sh` | Builds everything for one board into `out/<board>/` (git-ignored). Compiles each library once into `out/<board>/obj/`. |
| `core/` | The shim, board-independent: `Arduino.h`, `wiring.cpp`, `HardwareSerial.*`, `SPI.*`, `Wire.*`, `main.cpp`. |
| `boards/<board>/` | Everything one LaunchPad changes: `variant.h` (pin map), `pins_internal.h`, `arduino.syscfg` and its generated `gen/`. One folder per board; `BOARD=` picks it. |
| `boards/mspm0g3507/arduino.syscfg`, `gen/` | SysConfig input and its output (clock, SPI1, UART0) for the LP-MSPM0G3507. Regenerated by `build.sh` when the `.syscfg` is newer. |
| `../../third-party/arduino/` | The four unmodified libraries, shared by every board, with `SOURCES.txt` and `CHECKSUMS.sha256`. |
| `sketches/` | Three sketch folders: `pdqgfx`, `touchpaint` and `shimcheck`. `pdqgfx` is built twice, as `pdqgfx` and `pdqgfx_libmode`, so `build.sh` makes four programs. |
| `tools/ino2cpp.py` | Turns a `.ino` into a `.cpp` with prototypes, like the Arduino IDE. |
| `tools/size_by_group.py` | Splits flash and RAM by shim, library and TI runtime from a map file. |
