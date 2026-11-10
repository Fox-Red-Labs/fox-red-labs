# Raspberry Pi Pico 2 H (SC1632, RP2350): blinking from a Mac and how fast it is

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer.** Drag a MicroPython file onto the RP2350 disk and the LED blinks 65 s after the download starts, then changes rate with one edit (scripted, from the built-in loader). The C path took 87 s.

On paper, from the data sheets: the RP2350 chip has two Arm Cortex-M33 cores or two RISC-V (Hazard3) cores at 150 MHz, 520 kB of RAM and 4 MB of flash, and no debug probe on the board; programs go on by dragging a file onto a USB disk. The LP-MSPM0C1104 has a debug probe on the board, 16 kB of flash and 1 kB of RAM (TI data sheet and `data/boards.csv`).

Terms: [docs/vocab.md](../docs/vocab.md). Problems found: [FAQ](FAQ.md). 1 kB = 1,024 bytes.

Words used below: **Cortex-M33** and **Hazard3** are the two core types (Arm and RISC-V); **CoreMark** is a standard speed test; **BOOTSEL** is the button that starts the chip's built-in USB loader; a **UF2** file is a program packed so it can be copied onto that loader's disk; **MicroPython** is Python for small chips; **pico-sdk** is Raspberry Pi's C kit.

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `prebuild.sh` (builds every target that needs no board; it writes outside the repository); `micropython/blink.py`; `zephyr/hazard3-led.overlay`. Results are rows in `data/results/pico2.csv` at the top of the repository, not in this folder.

**Not in this folder:** the Raspberry Pi Pico SDK, picotool and MicroPython (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add pico2 tools data third-party/coremark
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact |
|---|---|
| Chip and core | RP2350A: two Arm Cortex-M33 cores (with a floating-point unit) or two Hazard3 RISC-V cores, chosen when you build, up to 150 MHz (Raspberry Pi Pico 2 datasheet, release 5, read 2026-10-07) |
| Memory | 520 kB RAM; 4 MB flash on the board (datasheet) |
| Peripherals | 12-bit ADC on three pins; two UARTs, two I2C, two SPI, 16 PWM channels, three PIO blocks (datasheet) |
| Security | SHA-256 hardware and a random-number generator; no AES (datasheet) |
| Debugger | None on the board; programs go on by dragging a UF2 file onto a USB disk, and every result comes back over USB serial (datasheet) |
| LED and button | Green LED on GP25; BOOTSEL button (datasheet) |
| Connector and pins | Micro-USB; 40 pins, fitted on the H version; no cable in the bag (datasheet) |
| Where to buy | Raspberry Pi and DigiKey |

## Ten-minute path (MicroPython: 65 s once the board was in its loader, scripted)

1. Download `RPI_PICO2-20260824-v1.29.0.uf2` (646 kB) from micropython.org/download/RPI_PICO2.
2. Hold BOOTSEL, plug the micro-USB cable into the Mac, let go. A disk named RP2350 appears.
3. Drag the `.uf2` file onto it. The board restarts as MicroPython.
4. Open the board's serial port (Thonny is Raspberry Pi's beginner editor; the timed run wrote `main.py` by script, and the Thonny route was not timed) and run [`micropython/blink.py`](micropython/blink.py). Change `HALF_PERIOD_MS` and run it again: the blink rate changes.

## One-command build (the C path, 87 s to a changed blink)

Everything that needs no board (blink, CoreMark on both kinds of core, the four room-occupancy models, every pico-examples program, Zephyr's blinky), into a folder outside the repo (set with `OUT`):

```bash
./prebuild.sh
```

It stops if the Mac is busy (load average at or above the core count) and prints the install steps if pico-sdk is missing. If you see `Permission denied` copying to /Volumes/RP2350, or no RP2350 disk appears after BOOTSEL, see the [FAQ](FAQ.md). Then hold BOOTSEL, plug in, and drag `blink.uf2` from the output folder onto the RP2350 disk.

## What was measured

Measured 2026-10-11 and 2026-10-12 on an Intel Mac. With no debug probe, every result comes back over USB serial.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| First blink, MicroPython | 65 s | scripted from the built-in loader | measured | `time_to_blink` (micropython) in `data/results/pico2.csv` |
| First blink, C | 87 s; 12 s build, 1 s flash | pico-sdk 2.3.1 already installed | measured | `time_to_blink`, `blink_build`, `blink_flash` (cli, uf2) |
| First blink, Arduino (arduino-pico) | 146 s of machine time | from a fresh install | measured | `time_to_blink` (arduino) |
| CoreMark, Cortex-M33 | 437.6 per second | GCC 12.2.0, -O3, 150 MHz | measured | `coremark` (gcc-bare-o3-m33-gcc12) |
| CoreMark, Hazard3 | 495.2 per second | GCC 16.1.0, -O3, 150 MHz | measured | `coremark` (gcc-bare-o3-riscv) |
| Raspberry Pi examples that build from a terminal | 155 of 155 (Arm), 154 of 154 that apply (RISC-V) | pico-sdk 2.3.1, build logs read | measured | `examples_built` (pico-sdk-2.3.1-arm, -riscv) |
| Raspberry Pi examples that ran and printed | 12 | the rest need a button press per image | measured | `examples_ran_clean` |

Also measured and listed on the results page, on both core types where it matters: Dhrystone, interrupt latency, the control step (the same `foc.c` file as the TI C2000 and MSPM0 boards), the four room-occupancy models, FFT, FreeRTOS, CRC, the ADC top rate and Embench. The blink rate is read from the program's own USB serial print (this board has no pin read in this setup).

## If you see this

- If you see `cp: /Volumes/RP2350/blink.uf2: Permission denied`, use `picotool load -x file.uf2` instead.
- If `picotool load -f` prints `Waiting for device to reboot` and nothing happens, unplug the cable, hold BOOTSEL, plug it back in, then load a program that has USB: `picotool load -x blink.uf2`.
- If a Zephyr build for the RISC-V cores stops with `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared`, add the overlay: `west build -b rpi_pico2/rp2350a/hazard3 samples/basic/blinky -- -DDTC_OVERLAY_FILE=pico2/zephyr/hazard3-led.overlay`.

The rest, with causes (including no RP2350 disk after BOOTSEL): [FAQ](FAQ.md).

## Limits

Not run: Zephyr on the board (the build for the RISC-V cores finishes but was not flashed), most of the example sweep (each image that prints on a UART needs one BOOTSEL replug), the wired loops (UART, ADC against a PWM and RC filter), power (needs the LP-XDS110ET meter) and a sealed-bag first-hour run. The 65 s and 87 s are scripted from the built-in loader.

## Where these facts come from

Checked 2026-10-11 and 2026-10-12 (Arduino and the example sweep) on an Intel Mac running macOS 15.7.9. Tools: pico-sdk 2.3.1, picotool 2.3.1, GCC 12.2.0 and 14.3.1 (Arm cores), GCC 16.1.0 (RISC-V cores), arduino-pico core 6.2.0, MicroPython `RPI_PICO2-20260824-v1.29.0`. Chip and board facts: Raspberry Pi Pico 2 datasheet, release 5, read 2026-10-07. Files: `data/results/pico2.csv`, `AGENTS.md`, `FAQ.md`.
