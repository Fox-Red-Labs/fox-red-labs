# NXP FRDM-MCXA153 on a Mac: changed LED blink in 1.4 minutes, CoreMark and SDK sweep

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer: 1.4 minutes from the edit to a proven 250 ms blink with NXP's SDK and the open-source pyOCD flasher, once they are installed.** Measured on 2026-10-11 on an Intel Mac with macOS 15.7.9 (Apple Silicon not run); every number is in [RESULTS-frdm-mcxa153.md](RESULTS-frdm-mcxa153.md) and `data/results/frdm-mcxa153.csv`.

That time was measured with GCC 12.2.0 from the Zephyr SDK (renamed `arm-none-eabi-*` so NXP's build finds it). The Arm GNU Toolchain 14.2 in step 2 built the same program; you do not need the Zephyr SDK.

First-time downloads took about 7 minutes: the SDK 89 seconds and 2.3 GB, the toolchain 10 seconds and 1.0 GB, pyOCD with its pack about 5 minutes and 110 MB (the pip installs were not timed). NXP's own flasher, LinkServer, was not installed: its download link redirects to an NXP login page, so this path uses pyOCD, which needs no login (see the [FAQ](FAQ.md)).

This board is a 96 MHz Cortex-M33 with exactly the memory of TI's LP-MSPM0G3507, a Cortex-M0+ board (128 kB flash, 32 kB RAM), so it makes a close comparison: about 1.6 times the CoreMark per MHz with the same GCC (1.3 times against TI's own compiler on the G3507; the flags and the memory system differ too, so not the core alone). It also lacks three things (see the [FAQ](FAQ.md)): no floating-point unit, no DSP instructions (the extra multiply-accumulate instructions signal code uses), and no AES or random-number block (a CRC unit only).

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. Words used below: **MCU-Link** is the debug probe built into the board; **pyOCD** is an open-source flasher that talks to it; the **MCUXpresso SDK** is NXP's software kit of drivers and examples; **west** is the Zephyr project's tool that fetches and builds it.

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-frdm-mcxa153.md` (the measured results); `prebuild.sh` (builds every target that needs no board; it writes outside the repository); `zephyr-coremark/` (CoreMark under Zephyr). Results are rows in `data/results/frdm-mcxa153.csv` at the top of the repository, not in this folder.

**Not in this folder:** NXP's MCUXpresso SDK and Zephyr (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add frdm-mcxa153 tools data third-party/coremark mspm0g3507/mcu-bench
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact |
|---|---|
| Chip and core | NXP MCXA153VLH: Arm Cortex-M33 (r1p0 as read from the chip) at 96 MHz from an internal oscillator, the FRO (NXP data sheet MCXAP64M96FS3 Rev. 5.1) |
| Memory | 128 kB flash, 32 kB RAM (data sheet) |
| Peripherals | 16-bit ADC (4 million samples a second at 12-bit); motor-control PWM block with an encoder input; three UARTs, two SPI, one I2C, one I3C (data sheet) |
| Not on the chip | No floating-point unit, no DSP instructions, no AES or random-number block; a CRC unit only (`FAQ.md`) |
| Debugger | MCU-Link on USB-C J15, the top port marked MCU-LINK; the chip's own USB on USB-C J8, which shows no probe (board manual UM12012 Rev. 2.0, `FAQ.md`) |
| LEDs and buttons | RGB LED; reset, ISP and wake buttons (UM12012) |
| Other on the board | An I3C temperature sensor; Arduino Uno and mikroBUS headers (UM12012) |
| Out of the box | Ships with a program that blinks the red LED once a second; a USB-A to USB-C cable is in the box; the Mac sees an NXP MCU-LINK (USB 1fc9:0143) and a serial port (`README.md`) |

## Ten-minute path (NXP SDK and pyOCD, measured 2026-10-11: 1.4 minutes after the tools are installed)

All steps run in the Mac's Terminal.

1. Plug the cable into **J15**, the top USB-C port marked MCU-LINK (not J8). Leave the jumpers as they came. The Mac sees an NXP MCU-LINK (USB 1fc9:0143) and a serial port.
2. Install the tools. You need macOS with Python 3 (3.14 was used), `cmake` and `ninja` (`brew install cmake ninja`), and a GCC for Arm (the download below is the Intel Mac build, x86_64; Apple silicon was not run): NXP names the Arm GNU Toolchain 14.2.Rel1 (138 MB download from <https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz>, no login, 1.0 GB installed). Then the SDK for this board from public GitHub, no account (89 s, 2.3 GB), and pyOCD with the board's pack (about 110 MB; the first `pack find` takes about 5 minutes):

```bash
curl -LO https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz
mkdir -p ~/arm-gnu-toolchain && tar -xf arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi.tar.xz -C ~/arm-gnu-toolchain
export ARMGCC_DIR=$(echo ~/arm-gnu-toolchain/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi)
mkdir -p ~/mcuxsdk && cd ~/mcuxsdk && python3 -m venv .venv && source .venv/bin/activate   # the SDK's own virtual environment: west and the SDK's Python packages go here
pip install -U west
west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git --mr v26.09.00 .
west update_board --set board frdmmcxa153
pip install --only-binary=cbor2 cbor2 && pip install -r mcuxsdk/scripts/requirements.txt
deactivate
python3 -m venv ~/pyocd-venv && ~/pyocd-venv/bin/pip install pyocd
~/pyocd-venv/bin/pyocd pack find mcxa153 && ~/pyocd-venv/bin/pyocd pack install MCXA153VLH
```

3. Set the delay in NXP's blink (run these from `~/mcuxsdk` with the SDK's virtual environment active: `source ~/mcuxsdk/.venv/bin/activate`) to 250 ms, build it (30 s) and flash it (11 s). (`ARMGCC_DIR` was set in step 2.)

```text
# SysTick is the Cortex-M's built-in timer the example waits on
sed -i '' 's/SysTick_DelayTicks(1000U)/SysTick_DelayTicks(250U)/' mcuxsdk/examples/demo_apps/led_blinky/led_blinky.c
west build -b frdmmcxa153 mcuxsdk/examples/demo_apps/led_blinky --toolchain armgcc --config release -d /tmp/blink -p always
~/pyocd-venv/bin/pyocd flash -t mcxa153 /tmp/blink/led_blinky.elf
```

You do not need a separate `west update`: `west update_board` does the fetch (89 s here). The red LED now blinks four times as fast. When it changes speed, the path works. If `west update_board` says `No such file or directory: 'west'`, or pip fails building `cbor2`, or `pyocd` says `Target type mcxa153 not recognized`, the [FAQ](FAQ.md) has the fix for each.

NXP's own first-hour path is MCUXpresso for VS Code with LinkServer; neither was run to a blink here: LinkServer's download link redirects to an NXP login, and the VS Code extension installs without a login (30 MB) but it was not opened. Details in the [FAQ](FAQ.md).

## One-command build

Everything that needs no board (blink, CoreMark at -O3, -O2 and -Os and at 48 MHz, the four room-occupancy models, a FreeRTOS timing image, a CRC image, the control-loop math, the build pass of every NXP example for this board, Zephyr's blinky), into `<scratch>/runs/arrivals/frdm-mcxa153/`:

```bash
./prebuild.sh
```

It needs the whole repository (it reuses the shared benchmark sources), the tools from step 2 and `hal_nxp` for the Zephyr step (`west update hal_nxp` in a Zephyr checkout, 1.3 GB). It stops with `prebuild: Mac is busy` if the Mac's load is high (`IGNORE_LOAD=1` builds anyway) and prints the install steps if the SDK is missing. The example sweep takes about half an hour. `ARM_TC` and `ARM_PFX` select another GCC, for example the Arm GNU Toolchain 14.2.

## What was measured

Measured 2026-10-11 on an Intel Mac, macOS 15.7.9. Every number and method: [RESULTS-frdm-mcxa153.md](RESULTS-frdm-mcxa153.md).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Changed blink, command line | 1.4 minutes of machine time; 30 s build, 10.8 s flash | NXP SDK v26.09.00, pyOCD, GCC 12.2.0, tools installed | measured | `time_to_blink`, `blink_build`, `blink_flash` (cli, pyocd) in `data/results/frdm-mcxa153.csv` |
| CoreMark | 282 per second (2.94 per MHz) | GCC 12.2, -O3, 96 MHz; about 1.6 times the LP-MSPM0G3507's 1.82 per MHz with the same GCC, 1.3 times its TI Arm Clang figure of 2.30 (flags and memory system differ too) | measured | `coremark`, `coremark_per_mhz` (gcc-bare-o3) |
| CoreMark, NXP's published figure | 381 per second (3.97 per MHz) | compiler not named; the best measured here is 74 percent of it | claim | NXP data sheet MCXAP64M96FS3 Rev. 5.1 |
| Room-occupancy model (8k) | 12.6 ms, 20 of 20 correct | -O3 | measured | `ai_occ_latency` (occ_cnn_8k-c-o3) |
| One field-oriented-control step, float | 5.4k cycles | GCC 12.2 -O2, code in flash; speed only | measured | `foc_step` (sw-flash) |
| One field-oriented-control step, NXP's fixed-point library | 448 cycles | speed result only; its output was not compared with the float step | measured | `foc_step` (q15-rtcesl) |
| SDK examples built from a terminal | 96 of 98 | MCUXpresso SDK v26.09.00 | computed | `examples_built` in `data/results/frdm-mcxa153.csv` |
| SDK examples that ran without a fault | 42 of 43 that can run alone | | measured | `examples_ran_clean` in `data/results/frdm-mcxa153.csv` |
| Sonnet 5.5 agent, changed blink | 4.8 minutes with this repo's docs, 5.0 minutes without | middle of three runs | measured | `agent_minutes` in `data/results/frdm-mcxa153.csv` |
| Sonnet 5.5 agent, CoreMark | 5.1 minutes with the docs, 8.0 minutes without | middle of three runs | measured | `agent_minutes` in `data/results/frdm-mcxa153.csv` |
| Sonnet 5.5 agent, recovering a chip whose debug port is locked out | 17 of 18 runs passed across the three jobs; the one miss was a without-docs run of this question | three runs per job with the docs and three without, each result checked independently | measured | `agent_minutes`, `agent_cost` in `data/results/frdm-mcxa153.csv` |

Not measured: the UART and ADC loops (they need bench wires) and power (needs a meter).

## If you see this

- If you see `No such file or directory: 'west'` from `west update_board`, put the SDK's virtual environment first on `PATH`: `export PATH=~/mcuxsdk/.venv/bin:$PATH`.
- If you see `Target type mcxa153 not recognized` from `pyocd`, install the pack: `pyocd pack find mcxa153`, then `pyocd pack install MCXA153VLH`.
- If you see `CMake Error ... ***Please set ARMGCC_DIR in environment variables***`, set `ARMGCC_DIR` to the toolchain folder that contains `bin/arm-none-eabi-gcc`, in the terminal you build from.

The rest, with causes (cbor2 build failure, no probe found, a locked debug port): [FAQ](FAQ.md).

## Limits

- UART and ADC loop tests: not run; they need bench wires.
- Power and the ADC-triggered control loop: not measured; they need a meter.
- NXP's own first-hour path, MCUXpresso for VS Code with LinkServer: not run to a blink; LinkServer's download redirects to an NXP login.
- Apple Silicon: not run.

## Where these facts come from

Checked 2026-10-11 on an Intel Mac, macOS 15.7.9 (Apple Silicon not run). Tools: NXP MCUXpresso SDK v26.09.00 (`mcux-26.09.00`); GCC 12.2.0 from the Zephyr SDK for the timed run, Arm GNU Toolchain 14.2.Rel1 for the build in step 2; pyOCD with the MCXA153VLH pack; Python 3.14; west. Chip and board facts: NXP data sheet MCXAP64M96FS3 Rev. 5.1 and board manual UM12012 Rev. 2.0. Sources: `data/results/frdm-mcxa153.csv`, `RESULTS-frdm-mcxa153.md`, `AGENTS.md`, `FAQ.md`.
