# MicroPython on the TI MSPM0G3519 and MSPM0G3507

**Short answer.** This folder builds MicroPython 1.29.0 for the Arm Cortex-M0+ MSPM0G3519 and MSPM0G3507 with `arm-none-eabi-gcc` and TI's MSPM0 DriverLib. The full build uses 175.6 kB of flash on the G3519 (512 kB) and gives a Python prompt on the LaunchPad kit's USB serial port at 115200 baud. A minimal build with no floating point uses 73.3 kB of flash on the G3519 and 72.9 kB on the G3507 (128 kB flash, 32 kB SRAM), where 29.1 kB is left for the Python heap. **Status: the full image runs on an LP-MSPM0G3519 (REPL, hardware `machine.SPI`) and drives a touch screen from Python; the G3507 image is built only.**

MicroPython also runs on the LP-EM-CC2340R5 through upstream's Zephyr port: [zephyr-cc2340r5/README.md](zephyr-cc2340r5/README.md).

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-20 on an Intel Mac, macOS 15.7, GNU Arm Embedded 14.3.1 (the copy in STM32CubeCLT 1.22.0), MSPM0 SDK 2.11.00.07, MicroPython v1.29.0 (MIT license).

## LAUNCHXL-F2800137 (C28x): not built

With cl2000 25.11.1.LTS in C11 mode, 132 of the 133 `py/*.c` files compile after stub headers (`alloca.h`, `unistd.h`, `endian.h`) and two typedefs; the board layer, the link and the 16-bit-`char` question are open. Details and the first error per pass: [`boards/f2800137/PORT.md`](boards/f2800137/PORT.md); log: `c2000-launchpad/bench/micropython/`.

## Existing MSPM0 ports

Upstream MicroPython has no MSPM0 port. MicroPython 1.29.0 ships ports for Alif, CC3200, ESP32, ESP8266, i.MX RT, nRF, PIC16, PSOC Edge, Renesas RA, RP2, SAMD, STM32 and Zephyr, plus the unix, WebAssembly, Windows and QEMU hosts and two teaching ports (`minimal`, `bare-arm`). None targets an MSPM0 part. GitHub repository search for "micropython mspm0", "micropython mspm0g3507" and "micropython mspm0g3519" (2026-10-20) returned only the upstream project and unrelated MicroPython tools, and code search for MSPM0 inside the upstream repository returned nothing. The CC3200 port is the only TI chip upstream, and it is a Cortex-M4. This port is therefore new work: it follows the layout of upstream's `ports/minimal/` and shares no code with another vendor's port.

## What it builds

| Image | Flash | Static RAM (data and bss) | Stack | Python heap | Fits |
|---|---|---|---|---|---|
| G3519, full (with hardware SPI) | 175.6 kB (34 % of 512 kB) | 1.7 kB | 4.0 kB | 122 kB (all the rest of 128 kB) | yes |
| G3519, minimal | 73.3 kB (14 %) | 1.5 kB | 1.5 kB | 125 kB | yes |
| G3519, minimal, heap capped at 16 kB | 73.3 kB | 1.5 kB | 1.5 kB | 16.0 kB (19.0 kB of RAM in all) | yes |
| G3507, minimal | 72.9 kB (57 % of 128 kB) | 1.4 kB | 1.5 kB | 29.1 kB (all the rest of 32 kB) | yes |
| G3507, full | 173.9 kB | 1.7 kB | 4.0 kB | 26.3 kB | no, 45.9 kB too big for the flash |
| G5187, minimal (runs on the LP-MSPM0G5187) | 72.9 kB (57 % of 128 kB) | 1.4 kB | 1.5 kB (4.0 kB for the pystone runs) | 29.0 kB (26.6 kB with the 4.0 kB stack) | yes |
| G5187, full | 173.9 kB | 1.7 kB | 4.0 kB | 26.3 kB | no, 45.9 kB too big for the flash |
| L1306, minimal (`make BOARD=l1306 VARIANT=min`) | 72.0 kB, 8.0 kB over the 64 kB flash | 1.4 kB | 1.5 kB | 1.1 kB of the 4 kB | no, the flash is too small |

Source: `build_all.sh`, which reads the linker symbols and the size tool. "Full" is MicroPython's extra-features level with single-precision floats, `math`, `struct`, `json`, `re`, `hashlib`, `uctypes`, `machine` with `Pin`, `SoftI2C` and `SoftSPI`, and `time`. "Minimal" is its smallest level: no floats (so `1/2` raises an error, `1//2` works), terse error messages, `machine.Pin`, `time`, `gc`. MicroPython's own guidance is 256 kB of flash and 16 kB of RAM at least; the minimal build is about 29 % of that flash figure. The heap is whatever the linker leaves after the program data and the stack; `make HEAP_MAX=16384` caps it.

## What is in the port

| Part | State | File |
|---|---|---|
| Startup, vectors, data and bss copy | TI's own `startup_mspm0g351x_gcc.c` or `startup_mspm0g350x_gcc.c` from the SDK, compiled unchanged | `Makefile` |
| Linker script (flash at 0, SRAM at 0x20200000, heap to the stack floor) | written here | `mspm0.ld` |
| Clock: SYSPLL to 80 MHz, bus clock 40 MHz, falls back to 32 MHz if the PLL does not lock | written here from the DriverLib API and the PLL lock check TI's errata asks for | `mphalport.c` |
| REPL on UART0, PA10 (TX) and PA11 (RX), 115200 baud, interrupt-driven receive, Ctrl-C | built, host-checked | `mphalport.c` |
| Heap and garbage collector | built, host-checked | `main.c` |
| `time.sleep_ms`, `sleep_us`, `ticks_ms`, `ticks_us`, `ticks_cpu` (SysTick every 1 ms) | built; the sleep path host-checked with a stand-in tick | `mphalport.c` |
| `machine.Pin` (`IN`, `OUT`, `OPEN_DRAIN`, `PULL_UP`, `PULL_DOWN`, `value`, `on`, `off`, `toggle`), pins named `"PA10"`, `"A10"` or a number | built, constructed and called in the emulator; no pin level has been read from a chip | `machine_pin.c`, `boards/pins_g3519.h`, `boards/pins_g3507.h` |
| `machine.SoftI2C` and `machine.SoftSPI` on any two or three pins | built, constructed and run against idle pins in the emulator (the scan finds nothing, as it should) | shared MicroPython code |
| `machine.freq`, `reset`, `unique_id`, `idle`, `mem8/16/32` | built | `port_machine.inc` |
| `machine.SPI(1)` on SPI1 (SCLK PB31, MOSI PB8, MISO PB7), 100 kHz to 40 MHz, modes 0 to 3, 8 bits; extra `write_repeat(buf, n)`. G3519 full build only | ran on the board: drives the ST7796S panel at 20 MHz | `machine_spi.c` |
| Hardware `machine.I2C`, `UART`, `ADC`, `PWM` | not done (use `SoftI2C`; the touch screen runs on it) | not built |

`machine.SPI` exists on the G3519 full build only (SPI1, fixed pins). `machine.I2C` is not defined: use `SoftI2C`. There is no file system, so `open()` raises an error and a script is typed or pasted at the prompt.

## Build

```bash
export PATH=/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin:$PATH     # any arm-none-eabi-gcc 12 or newer works
ports/micropython/build_all.sh                                             # all five images and their sizes
cd ports/micropython && make BOARD=g3519 VARIANT=full                      # one image: build-g3519-full/firmware.elf and .bin
```

`BOARD` is `g3519`, `g3507`, `g5187` or `l1306` (the L1306 build is size-only: 64 kB of flash is too small; the G5187 names its UART block UC0; the port picks the name per board); `VARIANT` is `full` or `min`. Set `MSPM0_SDK` if the SDK is not under `/Applications/ti/mspm0_sdk_2_11_00_07`. The build needs only Python 3 and the compiler; SysConfig is not used, because the port sets up the clock, UART and pins in code.

## Check on the host

```bash
pip install unicorn            # a Cortex-M0 emulator for Python
python3 ports/micropython/tests/emu_check.py g3519 full        # or: g3519 min, g3519 min-heap16k, g3507 min
```

The check loads the built image, starts it at its reset vector and types a script at the prompt. All four images print the banner (`MicroPython v1.29.0-frl1 on 2026-10-20; LP-MSPM0G3519 with MSPM0G3519`) and pass 7 (minimal) or 11 (full) expected lines: arithmetic, a list comprehension, `machine.Pin` construction and printing, `machine.freq()` reading 80000000, `time.sleep_ms`, `gc.mem_free()` (122 kB free on the G3519 minimal image, 27.8 kB on the G3507), a `ZeroDivisionError` traceback, and for the full build floats, `math.sqrt`, `SoftI2C`, `SoftSPI`.

What this does and does not show. The emulator runs the real firmware instructions, so the reset path, clock code, linker layout, heap, garbage collector and REPL are exercised. The chip's peripherals are stubs written for the check (UART transmit register, clock status, frequency counter, GPIO input reads high), interrupts do not run, console input is handed to the firmware directly, and nothing models the G3519's error-correcting SRAM. A pass here is not a pass on the chip.

## Flash and run

Back the board up first (`mspm0g3519/AGENTS.md`, "Back up"), keep the XDS110 backchannel jumpers J12 and J13 fitted so PA10 and PA11 reach the USB serial port, then:

```bash
DSLite flash -c mspm0g3519/MSPM0G3519_MG000001.ccxml -e -f -v ports/micropython/build-g3519-full/firmware.bin,0x0     # raw binary padded to 16 bytes, see First boot below
python3 -m serial.tools.miniterm /dev/cu.usbmodemMG0000011 115200      # then press Ctrl-D for a soft reset to see the banner again
```

Then, in the terminal: `import machine; p = machine.Pin("B27", machine.Pin.OUT); p.toggle()` and read the pin with a meter or an LED on PB27. Expect the banner, a `>>>` prompt, `machine.freq()` equal to 80000000 and `gc.mem_free()` near 118 kB. If the console is silent, the first thing to test is the error-correcting SRAM (below).

## Run on the LP-MSPM0G5187 (minimal image, measured 2026-11-09)

`make BOARD=g5187 VARIANT=min` builds the same 72.9 kB minimal image for the MSPM0G5187 (same Cortex-M0+ at 80 MHz, 128 kB flash, 32 kB SRAM). It booted on the first flash: banner `MicroPython v1.29.0-frl1 on 2026-11-09; LP-MSPM0G5187 with MSPM0G5187` at 115200 on the backchannel UART (UC0, PA10 and PA11), `machine.freq()` 80000000, 28,992 B of heap free, `machine.Pin` on A17 and A24 toggling (read through the debug port). Pin names come from `boards/pins_g5187.h` (made by `tools/gen_pins.py`).

| Check | Result |
|---|---|
| Flash as a raw binary padded to 16 bytes at 0x0 with DSLite, verify | passed |
| Pystone, `tests/pystone_lowmem_min.py`, 4 kB stack image, 3 runs | 680, 791 and 794 pystones per second |
| Same script, default 1.5 kB stack image | `RuntimeError: maximum recursion depth exceeded` |
| The G3519 full image, `tests/pystone_lowmem.py` | 1,112 to 1,114 (not the same script or image; compare only within one script) |

Two things the minimal level changes: it has no slice syntax, so `x[:]` is a `SyntaxError` (the `_min` copy of the benchmark writes `list(x)`), and its 1.5 kB stack leaves 1 kB for Python calls, which the benchmark exceeds; `make BOARD=g5187 VARIANT=min STACK_LEN=0x1000` takes 2.5 kB from the heap. `make ... AUTORUN='<python text with \n escapes>'` runs that text once at start-up; the LED check uses it because the debugger resets the chip when it attaches. Logs: mspm0g5187/bench/micropython/.

## Risks to watch on the first flash

| Risk | Why | What the port does | If it shows |
|---|---|---|---|
| SRAM double-error interrupt (NMI) | The G3519's first SRAM bank has error correction, and Zephyr's blinky stopped in the NMI handler with `SRAMDED` set (`docs/experiments/2026-10-08-zephyr-lp-mspm0g3519.md`). A byte write to a word never written first is a candidate cause. | `main()` writes zero to every word of the heap and the unused stack with 32-bit stores before any byte is written | Read `SYSCTL.NMIIIDX`; if it is 6, move the heap to the second bank (`__heap_start__ = 0x20210000` in `mspm0.ld`) |
| PLL does not lock | The SYSPLL lock check can need retries | Retries 20 times, then stays on the 32 MHz oscillator; `machine.freq()` then returns 32000000 | Baud rate stays right because it is computed from the real clock |
| Flash wait states | Set to 2 at both 32 MHz and 80 MHz, which is what the data sheet and TI's examples use | fixed | None expected |

## Where things are

- `third-party/micropython/` is MicroPython v1.29.0, unmodified (`SOURCE.md`).
- `RESULTS-ROWS.csv` holds the measured rows.

## First boot on the LP-MSPM0G3519 (2026-10-20)

The full build runs on the board. Flash it as a raw binary padded to 16 bytes at 0x0 with DSLite (`DSLite flash -c mspm0g3519/MSPM0G3519_MG000001.ccxml -e -f -v firmware.bin,0x0`); loading the ELF stopped at 91 percent with a verify mismatch at 0x27FB2, and OpenOCD read device ID 0 after the previous sleeping image.

| Check | Result |
|---|---|
| Banner on the backchannel UART at 115200 | `MicroPython v1.29.0-frl1 on 2026-10-20; LP-MSPM0G3519 with MSPM0G3519` |
| `machine.freq()` | 80000000 |
| `gc.mem_free()` after boot | 121.8 kB |
| Float (`2**0.5 * 1000`) | 1414.2135 |
| Empty `for` loop, 10,000 passes inside `exec` | 100.2 ms (compile time included) |
| `machine.Pin('B27')` toggle loop | ran with no error; the LED was too small to judge on a camera |

No SRAM error-correction fault showed. Image sha256 `df01a0fb08aebba6...` (ELF).

## Drive a touch screen from Python

The Waveshare 3.5 inch panel (ST7796S screen over SPI, FT6336U touch over I2C) runs from MicroPython on the LP-MSPM0G3519 with two short Python files: `lib/lcd35.py` (screen: init, `fill`, `rect`, `text` with a 5x7 font; touch: `ids`, `read`) and `lib/paint.py` ("touch to paint": six colors, a clear button, a dot where a finger touches). There is no file system, so the files are sent through the raw REPL.

Wiring (panel pin, LaunchPad header pin, chip pin): SCLK J1.7 PB31, MOSI J2.15 PB8, MISO J2.14 PB7, CS J2.13 PB12, DC J4.39 PB9, backlight J4.38 PC4, reset J4.37 PC5, touch SDA J1.10 PC3, SCL J1.9 PC2, INT J2.11 PB14, touch reset J2.12 PB11, power 3V3 and GND.

```bash
python3 ports/micropython/tools/paste_raw.py --timeout 10 ports/micropython/lib/lcd35.py paint_run.py   # paint_run.py is lib/paint.py without its "from lcd35 import *" line
```

The port holds the fill loop in C (`spi.write_repeat(row, 480)`); the screen logic is Python.

| Measure (checked 2026-10-20, 80 MHz CPU, one board) | Result |
|---|---|
| Full-screen fill, 320 by 480 pixels, SPI at 20 MHz | 147.4 ms (300 kB sent; 123 ms is the wire time at 20 MHz) |
| The same fill with one `spi.write` call per row from a Python loop | 158.7 ms (11 ms for 480 calls, 23 us each) |
| SPI at 40 MHz (the chip's maximum) | 74.4 ms, but the panel ignored the writes: its old picture stayed on screen. 20 MHz is the highest rate that draws |
| 17 characters at scale 2 (12 by 16 pixels each) | 65 ms |
| One touch read (`readfrom_mem_into`, 5 bytes, software I2C at the 400 kHz setting) | 1.43 ms |
| Touch chip answers | vendor 0x11, chip 0x64 (FT6336U), firmware 18 |
| Free heap with both files loaded | 68 kB |

Camera check (2026-10-21): the panel showed the Python-drawn red fill with a green and a blue box, and the paint screen's color bar. A finger drag has not been checked. The paint demo lives in RAM, so a power cycle returns to the prompt: send the two files again.

Limits: no file system and no frozen module (the files are re-sent after every reset); `machine.SPI` takes only SPI1 and its three pins; touch uses `SoftI2C` because the hardware I2C block did not get an answer through this module in an earlier bare-metal test (`mspm0g3507/display-test`).

## Porting lessons

- No MSPM0 port exists upstream. Following the layout of `ports/minimal/` and compiling TI's own startup file unchanged keeps the port small.
- A hand-written clock start-up has to carry the SYSPLL lock-and-retry loop that SysConfig otherwise writes; the baud divisor must use the real bus clock, which is half the CPU clock once the PLL runs.
- PINCM numbers do not follow pin order (PA2 is PINCM7, PB0 is PINCM12). Generate the pin table from SysConfig's device data.
- `import machine` fails at run time with a bare `ImportError` unless the Makefile includes `extmod/extmod.mk` and links `$(PY_O)`. Set `MICROPY_GIT_TAG` or the banner takes its version from the folder's git state.
- Flash a raw binary padded to 16 bytes with DSLite. Loading the ELF stopped near 91 percent with a verify mismatch, and OpenOCD read device ID 0 after a sleeping image.
- A pass in a Cortex-M0 emulator exercises start-up, heap and REPL only. Peripherals are stubs there, so the first board boot is still a separate test.
- `#ifdef GPIOC` is false in this SDK: `GPIOC` is a constant pointer, not a macro. Port C was never powered and every "PC" pin silently drove or read GPIOB's matching bit. Test on a port C pin the day a port has two or more GPIO ports.
- An output pin has its input buffer off, so `Pin.value()` read 0 whatever was written. Return the driven level for output pins.
- SPI1 on the G3519 takes its clock from MCLK (80 MHz), not the 40 MHz bus clock: the divisor maths changed the speed by 2x. Time one known transfer and compare with the wire time.
- A panel that ignores writes keeps its old picture, which reads as a pass. Reset the panel and write a picture that cannot be the old one before judging a speed.
- Raw REPL (Ctrl-A) is the way to load Python on a board without a file system; send it in 128-byte chunks with a short gap because the UART ring is small.
