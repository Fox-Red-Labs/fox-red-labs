# uLisp: a Lisp prompt on the MSPM0G5187, MSPM0G3507 and MSPM0G3519

uLisp 4.9a (MIT), David Johnson-Davies' Lisp interpreter for ARM boards, builds unmodified for three MSPM0 LaunchPad kits and runs on the LP-MSPM0G5187. It gives a Lisp prompt on the kit's USB serial port, with 2,800 objects of workspace (21.9 kB), `defun`, floats, strings, a LED toggle with `digitalwrite`, and its own timer. The image is 98.8 kB of flash (77 percent of the G5187's 128 kB) and 23.1 kB of RAM, with 8.9 kB left for the stack. `(tak 18 12 6)` takes 7.5 s at 80 MHz. **Status: run on the LP-MSPM0G5187 (19 REPL checks and 5 benchmark runs pass). The G3507 and G3519 images build and are not flashed.**

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

## Size

| Image | Flash | Static RAM (workspace included) | Workspace | Stack left | Fits |
|---|---|---|---|---|---|
| G5187 | 98.8 kB of 128 kB | 23.1 kB of 32 kB | 2,800 objects (21.9 kB) | 8.9 kB | yes |
| G3507 | 98.8 kB of 128 kB | 23.1 kB of 32 kB | 2,800 objects | 8.9 kB | yes, not flashed |
| G3519 | 99.0 kB of 512 kB | 71.6 kB of 128 kB | 9,000 objects (70.3 kB) | 56.4 kB | yes, not flashed |

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
