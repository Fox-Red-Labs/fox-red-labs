# uLisp: a Lisp prompt on the MSPM0G5187, MSPM0G3507 and MSPM0G3519

uLisp 4.9a (MIT), David Johnson-Davies' Lisp interpreter for ARM boards, builds unmodified for three MSPM0 LaunchPad kits and runs on the LP-MSPM0G5187. It gives a Lisp prompt on the kit's USB serial port, with 2,800 objects of workspace (21.9 kB), `defun`, floats, strings, a LED toggle with `digitalwrite`, and its own timer. The image is 98.8 kB of flash (77 percent of the G5187's 128 kB) and 23.1 kB of RAM, with 8.9 kB left for the stack. `(tak 18 12 6)` takes 7.5 s at 80 MHz. **Status: also run on the LAUNCHXL-F2800137 (19 of 19 REPL checks, tak 8.1 s at 120 MHz; section below) and the LP-EM-CC2340R5 (19 of 19 REPL checks, tak 10.9 s at 48 MHz; section below). Run on the LP-MSPM0G5187 (19 REPL checks and 5 benchmark runs pass). The G3507 and G3519 images build and are not flashed.**

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-21 on an Intel Mac, macOS 15.7, GNU Arm Embedded 14.3.1 (C++17), MSPM0 SDK 2.11.00.07, uLisp ARM Release 4.9a (commit `d6427ec6`), one LP-MSPM0G5187 (probe <probe serial>).

## Ten-minute path

```bash
export PATH=/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin:$PATH   # any arm-none-eabi-g++ 12 or newer
ports/ulisp/build.sh                                                     # about 20 s; writes ports/ulisp/build-g5187/ulisp.elf
python3 tools/bench/run.py --board mspm0g5187 --image ports/ulisp/build-g5187/ulisp.elf --secs 3 --expect "uLisp"
screen /dev/cu.usbmodemMG0200011 115200                                  # your port name carries your probe serial
```

The prompt shows the free workspace:

```text
uLisp 4.9a
2800> (+ 1 2)
3
2800> (defun fib (n) (if (< n 3) 1 (+ (fib (- n 1)) (fib (- n 2)))))
fib
2750> (time (fib 20))
6765
Time: 1.2 s
```

Press `~` to stop a running program. Other kits: `BOARD=g3507 ports/ulisp/build.sh` or `BOARD=g3519 ports/ulisp/build.sh` (build only; flash them with DSLite as in `ports/wasm3/README.md`).

## What is here

| Part | File |
|---|---|
| uLisp 4.9a, unmodified | `third-party/ulisp-arm/` (MIT, pinned commit, hashes) |
| Build copy: one board block and the pin-mode keywords added, function prototypes written the way the Arduino IDE does | `patches/apply.py` |
| Arduino shim: `Serial`, `millis`, `micros`, `delay`, `pinMode`, `digitalWrite`, `digitalRead`, stubs for analog, tone, SPI and I2C | `shim/` (about 160 lines) |
| Board layer: 80 MHz clock, UART at 115200 with a receive interrupt, three LEDs | `boards/hal.c`, `boards/main.cpp`, `boards/mspm0.ld` |
| Build for three kits | `build.sh` |
| REPL check and benchmarks over serial | `tests/repl_check.py` |

## LP-EM-CC2340R5 (TI Arm Clang, 48 MHz Cortex-M0+)

The same sketch, build copy and shim run on the LP-EM-CC2340R5 with the SimpleLink Low Power F3 SDK (NoRTOS), built by `ports/ulisp/boards/cc2340r5/build.sh` (build only; flash `out/ulisp.hex` with DSLite as in `cc2340r5/README.md`). The board layer is `boards/cc2340r5/board.c` (UART2 at 115200 with a 512-byte driver receive ring, SysTick at 1 kHz for milliseconds, red LED DIO14 and green LED DIO15 as pins 13 and 14), with a `compat.h` for `strcasecmp`. The image is 96.2 kB of flash (19 percent of 512 kB) and 24.0 kB of static RAM with the 2,800-object workspace, which leaves 11.1 kB for the stack. 19 of 19 REPL checks pass. `(tak 18 12 6)` takes 10.8 to 10.9 s, `(fib 23)` 7.5 to 7.6 s and `(q2 7 8)` 27.9 s, about 14 percent faster than the G5187's times scaled by 80/48. Rows: `data/results/cc2340r5.csv`; logs: `cc2340r5/bench/ulisp/`.


## LP-EM-CC2755P10 (TI Arm Clang, 96 MHz Cortex-M33)

The same sketch, build copy and shim run on the LP-EM-CC2755P10 with the SimpleLink Low Power F3 SDK (NoRTOS), built by `ports/ulisp/boards/cc2755p10/build.sh` (build only; flash `out/ulisp.hex` with DSLite as in `cc2755p10/README.md`, after `cc2755p10/bench/hexcheck.py` says SAFE). The board layer is `boards/cc2755p10/board.c`, a copy of the CC2340R5 one with three changes: Cortex-M33 hard-float compile flags, no `PowerCC23X0.h`, and LED pins named by `CONFIG_GPIO_LED_0` and `CONFIG_GPIO_LED_1`, because on this chip the GPIO driver index is the DIO number (red DIO16, green DIO12); indices 0 and 1 are DIO0 and DIO1, and DIO1 is the UART TX pin, so a copy that kept 0 and 1 ran but printed nothing. The image is 94.4 kB of flash (10 percent of 928 kB) and 24.2 kB of static RAM with the 2,800-object workspace; the linker gives the rest of the 162 kB, 136.9 kB, to the stack. 19 of 19 REPL checks pass in each of three runs. `(tak 18 12 6)` takes 3.1 to 3.2 s, `(fib 23)` 2.2 s and `(q2 7 8)` 8.1 s: 3.4 times faster than the CC2340R5 for twice the clock. Rows: `data/results/cc2755p10.csv`; logs: `cc2755p10/bench/ulisp/`.

## LP-MSPM33C321A (TI Arm Clang, 160 MHz Cortex-M33)

The same sketch, build copy and shim run on the LP-MSPM33C321A with the MSPM33 SDK DriverLib and no RTOS, built by `ports/ulisp/boards/mspm33c321a/build.sh` (build only; flash `ulisp.out` with `mspm33c321a/tools/run_image.py <image> 1 --serial <probe serial>`, after the backup in `mspm33c321a/README.md`). `ulisp.syscfg` is TI's `sysctl_mclk_syspll` clock setup (160 MHz) plus the back-channel UART (UC12, PA10 TX, PA11 RX, 115200) and the two LEDs (red PC26 is Arduino pin 13, green PC27 is pin 14). `board.c` fills a 512-byte receive ring from the UART interrupt and counts milliseconds in SysTick. The image is 87.7 kB of flash (8 percent of 1 MB) and 23.2 kB of static RAM with the 2,800-object workspace, plus a 16 kB stack. 19 of 19 REPL checks pass in each of three runs, and the stack guard prints `Error: stack overflow` and the next line evaluates. `(tak 18 12 6)` takes 2.5 to 2.6 s, `(fib 23)` 1.7 s and `(q2 7 8)` 6.5 s. That is 1.2 times faster than the LP-EM-CC2755P10 (same core, 96 MHz) for 1.67 times the clock, so the figure scaled by clock (1.9 s for tak) was missed by 37 percent; the 2 flash wait states at 160 MHz are the suspected cause, not measured. Rows: `data/results/mspm33c321a.csv`; logs: `mspm33c321a/bench/ulisp/`.

If the prompt answers only after a second or later bytes, the UART receive interrupt is set to the FIFO threshold only: add `RX_TIMEOUT_ERROR` to `UART1.enabledInterrupts` in the `.syscfg`, so a single typed byte raises the interrupt (first image: 1 of 24 checks failed in each run for this reason).

## LP-AM243 (TI Arm Clang, 800 MHz Cortex-R5F)

The same sketch, build copy and shim run on the LP-AM243 as a RAM load through the debug port (nothing is written to its flash), built by `ports/ulisp/boards/am243x/build.sh` with the MCU+ SDK; the board layer is `boards/am243x/board.c` and `main.cpp` (console on the on-board probe's UART, a millisecond timer, LEDs read back at the GPIO register). 20 of 20 REPL checks pass in each of three loads. `(tak 18 12 6)` takes 0.91 s, `(fib 23)` 0.70 s, `(fib 20)` 0.16 s and `(q2 7 8)` 2.6 s, the same in every load: about 3.4 times the CC2755P10's speed, less than the clock ratio, and the cause is open (the -Os build, the polled console, or the clock assumption). Rows: `data/results/am243x.csv`; logs: `am243x/bench/ulisp/`.

## LAUNCHXL-F2800137 (TI C2000 compiler, C28x 120 MHz)

Runs. `boards/f2800137/build.sh` builds the same build copy with cl2000 as C++ after changes for a 16-bit `char` and a 16-bit `int` (every `int` in the sketch becomes `long`, `uint8_t` from a typedef, `sizeof` counted in 16-bit words, a stack that grows up): 141,866 B of flash and 28,698 B of RAM with 2,000 objects of workspace. The image is written to flash from a raw binary (`boards/f2800137/mkraw.sh`, then `c2000-launchpad/tools/c2kflashback.js`), because CCStudio's `loadProgram` of the `.out` timed out. 19 of 19 REPL checks pass in each of 3 runs. `(tak 18 12 6)` takes 8.1 to 8.2 s, `(fib 23)` 5.3 to 5.4 s and `(q2 7 8)` 20.4 s at 120 MHz: 1.3 times sooner than the CC2340R5 but with 2.5 times the clocks. Rows: `data/results/f2800137.csv`; what was changed: [`boards/f2800137/PORT.md`](boards/f2800137/PORT.md); logs: `c2000-launchpad/bench/ulisp/`.

## Size

| Image | Flash | Static RAM (workspace included) | Workspace | Stack left | Fits |
|---|---|---|---|---|---|
| G5187 | 98.8 kB of 128 kB | 23.1 kB of 32 kB | 2,800 objects (21.9 kB) | 8.9 kB | yes |
| G3507 | 98.8 kB of 128 kB | 23.1 kB of 32 kB | 2,800 objects | 8.9 kB | yes, not flashed |
| G3519 | 99.0 kB of 512 kB | 71.6 kB of 128 kB | 9,000 objects (70.3 kB) | 56.4 kB | yes, not flashed |
| L1306 | 97.9 kB needed of 64 kB | 1.0 kB of 4 kB (24 objects, 192 B) | 24 to 40 objects | 3.0 to 3.1 kB | no: 34.7 kB too big for the flash; the RAM fits |
| CC2340R5 | 96.3 kB of 512 kB | 24.0 kB of 36 kB | 2,800 objects (21.9 kB) | 11.1 kB | yes, flashed and run |
| CC2755P10 | 94.4 kB of 928 kB | 24.2 kB of 162 kB | 2,800 objects (21.9 kB) | 136.9 kB | yes, flashed and run |
| F2800137 | 141.9 kB of 256 kB | 28.7 kB of 36 kB | 2,000 objects (16.0 kB) | 12.3 kB (6,144-word stack) | yes, flashed and run |

Source: the size tool and linker symbols. An object is 8 bytes. The workspace is the one setting that trades speed of garbage collection and program size against stack: on the 32 kB parts 2,800 objects leave 8.9 kB for the C stack, and uLisp reports `Error: stack overflow` instead of crashing when a recursion runs it out (`(defun r (n) (+ 1 (r n)))` then `(r 1)` prints the error and the next line works).

## Speed

uLisp's published `(tak 18 12 6)` on the Arduino Zero (ATSAMD21, Cortex-M0+, 48 MHz) is 11.4 s, which is about 547 MHz-seconds. At 80 MHz the same work should take about 6.8 s; a range of 5 to 10 s counts as agreement.

| Test | LP-MSPM0G5187, 80 MHz | Published, for scale |
|---|---|---|
| `(tak 18 12 6)` | **7.5 s** (answer 7) | Arduino Zero 48 MHz: 11.4 s. Raspberry Pi Pico 200 MHz: 2.6 s. Arduino Mega 2560: 55.6 s |
| `(fib 23)` | 5.2 s (answer 28657) | Arduino Mega 2560: 30.5 s |
| `(q2 7 8)` | 19.2 s (answer 31) | Arduino Zero 48 MHz: 24.3 s. Pico 200 MHz: 5.7 s |
| `(fib 20)` | 1.2 s (answer 6765) | |
| garbage collection, full workspace | 1.07 ms (the `(gc)` report) | Arduino Zero: 2.1 ms (2,500 objects) |

Per MHz, the G5187 (600 MHz-seconds for tak) sits between the Zero (547) and the Pico (520). Each time came from uLisp's own `(time ...)`, the same value on two full runs. The published figures are from the uLisp site's [Performance](http://www.ulisp.com/show?36M3) table and [Benchmarks](http://www.ulisp.com/show?1EO1) page (read 2026-10-21); other boards' figures use their own builds, so the comparison is a scale, not a head-to-head.

## LED

`(pinmode :led-builtin :output)` then `(digitalwrite :led-builtin t)` sets and reads back (`(digitalread :led-builtin)` gives `t`). Pin 13 is PA17, one of the kit's three LEDs; pins 14 and 15 are PB13 and PA24. The repository's check `tools/check/blink.py` read PA17 toggling (6 changes in 16 samples, 150 ms half period) while a build with `AUTOEXEC` ran the loop at start-up. The script printed FAIL for that run: it reads the output-enable register once at the instant its debugger connection resets the chip, before any code has run, so it cannot pass a program that sets a pin up after reset. The toggling itself is in the sample line.

## Limits

- Only the three LEDs are real pins. `digitalwrite` on any other number does nothing and `digitalread` gives `nil`, so the BoosterPacks stacked on this kit are never driven. `analogread` returns 0 (no ADC channel is wired in the shim).
- No sound: `tone` and `note` are empty on purpose (quiet hours).
- Serial input has a 512-byte buffer filled by the UART interrupt. Pasting a long program in one burst works when uLisp is idle at the prompt.
- `save-image` and `load-image` are not available (no flash storage layer; the 128 kB flash is mostly the interpreter). The assembler (`defcode`) is off: it needs a RAM code block that the shim does not set up.
- SPI and I2C calls compile and do nothing. SD card and graphics options are off.
- The shim's random numbers are xorshift seeded from the microsecond clock when the prompt returns.

## Build options

`WORKSPACE=<objects> BOARD=g5187 ports/ulisp/build.sh` changes the workspace. `AUTOEXEC='(lisp text)' OUT=<folder> ports/ulisp/build.sh` makes an image whose prompt runs that text 500 ms after reset, as if typed; it also sets LED 1 as an output at reset. It exists so a debugger check can see a program run without anyone typing.

## Problems hit

- uLisp lists the boards it supports and stops with `Board not supported!` for any other. The build copy gets one `FRL_MSPM0` block and the pin-mode keywords for it; every other line is the upstream file.
- The sketch relies on the Arduino IDE writing function prototypes. `patches/apply.py` does the same (about 490 of them), skipping the SD-card helpers.
- The prompt stayed silent at first: the shim's `available()` read the serial buffer once and never again. Looking at the program counter with OpenOCD showed it waiting in the shim's read call.
- Doubled characters in typed text (`nn` for `n`) came from reading the UART FIFO in both the interrupt and the main loop. Only the interrupt reads it now.
- The G5187's SDK names the UART instance `UC0` (vector `UC0_IRQHandler`), not `UART0`.
- A debugger connection through OpenOCD resets the chip, so a REPL connection cannot be left in a state and then inspected; use the `AUTOEXEC` image.

- CC2340R5: TI Arm Clang reads a plain `register int *sp asm ("sp")` as an unset value, so uLisp's stack check never fired and an endless recursion silenced the console. `patches/apply.py` with `FRL_ENDSTACK` set reads the stack pointer with an explicit `mov` and points the check at the linker's stack floor `__stack`. The compiler's `strings.h` does not compile as C++, so `compat.h` declares `strcasecmp` and `board.c` defines it.

On the LP-MSPM0L1306 (64 kB flash, 4 kB RAM) the code does not fit. `ports/ulisp/boards/mspm0l1306/build.sh` (build only; own board layer: UART0 PA8 and PA9, LED1 PA0, 32 MHz) gives 100,244 B of flash with a 24-object workspace, 34,708 B over the chip's 65,536 B (the link stops with 'region FLASH overflowed'); `-Os -flto` saves 800 B. The RAM would fit: 1,004 B of static RAM with 24 objects, 1,132 B with 40, leaving about 3 kB for the stack. Nothing was flashed. Rows: `data/results/mspm0l1306.csv`; logs: `mspm0l1306/bench/ulisp/`.
