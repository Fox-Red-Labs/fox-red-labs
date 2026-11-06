# AGENTS.md for the NXP FRDM-MCXA153: what an AI coding assistant needs to know

Know this first: the chip is a Cortex-M33 with no floating-point unit, no DSP instructions and no AES block, and that NXP's own flasher needs a login, so the path that works here uses the open-source pyOCD. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [FRDM-MCXA153 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **MCU-Link** is the debug probe built into the board; **pyOCD** is an open-source flasher that talks to it; the **MCUXpresso SDK** is NXP's software kit of drivers and examples; **west** is the Zephyr project's tool that fetches and builds it; **CoreMark** is a standard CPU speed test; **FOC** is field-oriented control, the math that drives a motor. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `frdm-mcxa153/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | NXP MCXA153VLH: Arm Cortex-M33 (r1p0 as read from the chip) at 96 MHz from the internal FRO (`frdm-mcxa153/README.md`) |
| Memory | 128 kB flash, 32 kB RAM (`frdm-mcxa153/README.md`) |
| Not on the chip | No floating-point unit, no DSP instructions, no AES or random-number block; a CRC unit only (`frdm-mcxa153/README.md`, `frdm-mcxa153/FAQ.md`) |
| Debugger | MCU-Link on USB-C J15 (top port, marked MCU-LINK); J8 is the chip's own USB and shows no probe (`frdm-mcxa153/README.md`, `frdm-mcxa153/FAQ.md`) |
| LEDs and buttons | RGB LED; reset, ISP and wake buttons; ships blinking the red LED once a second (`frdm-mcxa153/README.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MCUXpresso SDK v26.09.00 (2.3 GB, 89 s), GCC 12.2.0 or Arm GNU Toolchain 14.2.Rel1, pyOCD with the `MCXA153VLH` pack, Python 3.14, cmake, ninja (`frdm-mcxa153/README.md`) |
| Set the compiler | `export ARMGCC_DIR=<folder containing bin/arm-none-eabi-gcc>` (`frdm-mcxa153/README.md`, `frdm-mcxa153/FAQ.md`) |
| Build NXP's blink | From `~/mcuxsdk` with its virtual environment active: `west build -b frdmmcxa153 mcuxsdk/examples/demo_apps/led_blinky --toolchain armgcc --config release -d /tmp/blink -p always` (30 s) (`frdm-mcxa153/README.md`) |
| Flash | `~/pyocd-venv/bin/pyocd flash -t mcxa153 /tmp/blink/led_blinky.elf` (10.8 s) (`frdm-mcxa153/README.md`, `data/results/frdm-mcxa153.csv` blink_flash) |
| Check | The red LED blinks four times as fast after changing `SysTick_DelayTicks(1000U)` to `250U`; 1.4 minutes from the edit (`frdm-mcxa153/README.md`, `data/results/frdm-mcxa153.csv` time_to_blink) |
| Everything prebuilt | `./prebuild.sh`; stops with `prebuild: Mac is busy` unless `IGNORE_LOAD=1` (`frdm-mcxa153/README.md`) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `No such file or directory: 'west'` from `west update_board` | `west` is started by name from the shell | `export PATH=~/mcuxsdk/.venv/bin:$PATH` (`frdm-mcxa153/FAQ.md`) | Command runs |
| `Failed to build installable wheels` naming `cbor2` | Python 3.14 on an Intel Mac builds cbor2 from source | `pip install --only-binary=cbor2 cbor2` first (`frdm-mcxa153/FAQ.md`) | Requirements install |
| `Target type mcxa153 not recognized` | pyOCD has no built-in support for this chip | `pyocd pack find mcxa153`, then `pyocd pack install MCXA153VLH` (`frdm-mcxa153/FAQ.md`) | Flash runs |
| `CMake Error ... ***Please set ARMGCC_DIR in environment variables***` | Compiler folder not set | Set `ARMGCC_DIR` in the build terminal (`frdm-mcxa153/FAQ.md`) | Build runs |
| `No available debug probes are connected` | Cable in J8, not J15 | Use J15 and run `pyocd list` (`frdm-mcxa153/FAQ.md`) | `NXP Semiconductors MCU-LINK FRDM-MCXA153` listed |
| `SWD/JTAG communication failure (WAIT ACK)` or `(FAULT ACK)` after a custom program | Unknown; an ADC-triggered test program left the debug port answering FAULT or WAIT | Hold SW2 (ISP), press and release SW1 (reset), release SW2, then `blhost` from SPSDK at 57,600 baud on the MCU-Link serial port (`frdm-mcxa153/FAQ.md`) | The bootloader answers |

## Rules for the assistant

1. Do not wait for LinkServer: its download redirects to an NXP login. Use pyOCD (`frdm-mcxa153/README.md`).
2. CoreMark at 96 MHz with GCC 12.2 -O3: 282 per second, 2.94 per MHz. NXP's published 381 per second names no compiler (`data/results/frdm-mcxa153.csv` coremark, `frdm-mcxa153/RESULTS-frdm-mcxa153.md`).
3. FOC step in float is 5.4k cycles; NXP's fixed-point library is 448 cycles, a speed result only (`data/results/frdm-mcxa153.csv` foc_step).
4. UART and ADC loops and power have no result (`frdm-mcxa153/README.md`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- An ADC-triggered test program left the debug port answering FAULT or WAIT at every power-up (pyOCD SWD/JTAG communication failure), even under reset (`frdm-mcxa153/FAQ.md`, 2026-10-11)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug and replug USB | hands | failed | `frdm-mcxa153/FAQ.md`, 2026-10-11 |
| Connect under reset (pyOCD, OpenOCD) | remote | failed | `frdm-mcxa153/FAQ.md`, 2026-10-11 |
| Hold SW2 (ISP), press and release SW1 (reset), let go of SW2, then reprogram with blhost over the MCU-Link serial port at 57,600 baud (ROM bootloader K3.2.0 answered blhost; the debug port still refused) | hands | proven | `frdm-mcxa153/FAQ.md`, 2026-10-11 |

<!-- recovery:end -->
