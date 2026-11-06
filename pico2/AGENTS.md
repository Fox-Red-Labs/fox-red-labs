# AI assistant notes for the Raspberry Pi Pico 2 H (RP2350)

That the board has no debug probe, so every program goes on through the BOOTSEL USB disk or `picotool`, and every result comes back over USB serial. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [Pico 2 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **Cortex-M33** and **Hazard3** are the two core types (Arm and RISC-V); **BOOTSEL** is the button that starts the chip's built-in USB loader; a **UF2** file is a program packed so it can be copied onto that loader's disk; **picotool** is Raspberry Pi's command-line loader; **pico-sdk** is Raspberry Pi's C kit; **CoreMark** is a standard CPU speed test. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `pico2/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | RP2350A: two Cortex-M33 cores or two Hazard3 RISC-V cores, chosen at build time, up to 150 MHz; from the datasheet (`pico2/README.md`) |
| Memory | 520 kB RAM, 4 MB flash on the board (`pico2/README.md`) |
| Not on the chip | No AES; SHA-256 hardware and a random-number generator are there (`pico2/README.md`, `data/results/pico2.csv` aes_speedup not_supported) |
| Debugger | None. Programs go on by UF2 drag or `picotool`; results come back over USB serial (`pico2/README.md`) |
| LED and button | Green LED on GP25; BOOTSEL button (`pico2/README.md`) |
| Cable | None in the bag; some micro-USB cables carry power only (`pico2/README.md`, `pico2/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | pico-sdk 2.3.1, picotool 2.3.1, GCC 12.2.0 and 14.3.1 (Arm), GCC 16.1.0 (RISC-V) (`pico2/README.md`) |
| MicroPython route | Hold BOOTSEL, plug in, let go, drag `RPI_PICO2-20260824-v1.29.0.uf2` onto the RP2350 disk, run `pico2/micropython/blink.py` and change `HALF_PERIOD_MS`: 65 s scripted (`pico2/README.md`, `data/results/pico2.csv` time_to_blink) |
| Build C | `./prebuild.sh`, with `OUT` set to a folder outside the repo; stops if the Mac is busy (`pico2/README.md`) |
| Flash | Hold BOOTSEL, plug in, drag `blink.uf2` from the output folder onto the RP2350 disk; the C path took 87 s with a 12 s build and a 1 s flash (`pico2/README.md`, `data/results/pico2.csv`) |
| Load without the button | `picotool load -f -x file.uf2`, only when the running program has USB serial; an Arduino-built image needs a 1200-baud serial touch first (`pico2/FAQ.md`) |
| Check | Read the program's own USB serial print; the board has no pin read in this setup (`pico2/README.md`) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| No RP2350 disk after BOOTSEL | The chip reads the button only at power-up | Hold BOOTSEL first, then plug in; try another cable (`pico2/FAQ.md`) | The disk appears |
| `cp: /Volumes/RP2350/blink.uf2: Permission denied` | Unknown (suspected: the disk was busy with the previous restart) | `picotool load -x file.uf2` (`pico2/FAQ.md`) | The load passes |
| `Waiting for device to reboot` and nothing happens | The running program has no USB, so `picotool load -f` cannot restart it, and its exit code is 0 anyway | Replug with BOOTSEL held, then `picotool load -x blink.uf2` (`pico2/FAQ.md`) | New program's output appears |
| `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared` (Zephyr, RISC-V) | The Hazard3 board file lacks the `led0` alias | Add `pico2/zephyr/hazard3-led.overlay` with `-DDTC_OVERLAY_FILE` (`pico2/FAQ.md`) | The build finishes; not flashed |

## Rules for the assistant

1. Check a load by reading the new program's output, not picotool's exit code (`pico2/FAQ.md`).
2. UART-printing examples and Zephyr images carry no USB reset interface; each needs a BOOTSEL replug (`pico2/FAQ.md`).
3. CoreMark at 150 MHz: 437.6 per second on the Cortex-M33 (GCC 12.2.0, -O3), 495.2 on Hazard3 (`data/results/pico2.csv` coremark).
4. Zephyr on the board and power have not run (`pico2/README.md`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A running program with no USB code cannot be restarted into the loader by picotool load -f (exit code 0, board never reboots) (`pico2/FAQ.md`, 2026-10-11)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug USB, hold BOOTSEL, plug in, release; then picotool load -x | hands | proven | `pico2/FAQ.md`, 2026-10-11 |
| uhubctl power cycle of the probe's hub port (the image booted back into itself and never showed; a BOOTSEL replug was still needed) | remote | failed | 2026-10-12 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-12. The cycle restarted nothing the Mac could see: the image has no USB code, so the board never showed.

<!-- recovery:end -->
