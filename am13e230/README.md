# TI LP-AM13E230 LaunchPad development kit: Arm motor-control chip with a math unit and a neural accelerator, measured

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Run 2026-10-08 on a Rev E2 board** (prototype XAM13E23019 silicon, AM13E2x SDK 26.01.00.03.STS, short-term support, TI Arm Clang 5.1.1.LTS). Box to changed blink took 22 s from the command line and 60 s through CCStudio's AI tools. CoreMark gives 872.4 per second at 200 MHz and one motor-control step takes 117 cycles with the TMU: numbers in [RESULTS-am13e230.md](RESULTS-am13e230.md). Power, the neural accelerator and the UART loop did not run; the list is there too.

Problems and fixes: [FAQ.md](FAQ.md). 1 kB = 1,024 bytes.

Terms: **TMU** is a trig math unit (sine, cosine, divide in a few clock cycles); **NPU** is a neural-network accelerator; **FOC** (field-oriented control) is the math a motor drive runs every PWM period; **DSLite** is the command-line flasher inside CCStudio; **NONMAIN** is the chip's boot configuration area.

## What is in this folder, and what is not

**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-am13e230.md` (the measured results); `blink.sh` and `prebuild.sh` (build scripts); `AM13E23019_AL130001.ccxml` (a connection file that works, with its probe serial); `tools/` (helper scripts for this board). Folders: `bldc-drv8376/` a BLDC motor drive, `foc-drv8376/` field-oriented control, `motor-l293d/` a brushed-motor drive, `radar-parser/` the radar frame parser, `stepper-uln2003/` a stepper drive, `stm32g4-hal/` ST HAL port comparison, `drv8323rs-regs/` a gate-driver register tool, `zephyr/` Zephyr builds. Results are rows in `data/results/am13e230.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below. Raw run logs and benchmark capture folders stay with the maintainers: the numbers they produced are rows in the results file.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add am13e230 tools data third-party/coremark
```

`docs/` is left out of that clone (236 MB, mostly pictures). Add the pages a link points to with `git sparse-checkout add docs`.


## The board

AM13E23019: Arm Cortex-M33 at 200 MHz on this board, with a floating-point unit, a TMU reached as custom instructions, and a TinyEngine NPU; 512 kB of flash, 128 kB of RAM; five motor PWM modules, three 12-bit ADCs at 7.81 million samples a second, programmable-gain amplifiers and fast comparators for current sensing. The LaunchPad adds an isolated debug probe, a CAN-FD transceiver, a 5 V encoder header, red and green LEDs, wake and bootloader buttons, a power-measurement header (J10) and 40-pin BoosterPack headers. There is no motor driver on the board, and none was bought.

Check the back of the board first: `MCU178E1` printed there means revision E1, which has different pins from everything here (user guide section 3.1).

## Ten-minute path

You need: the board and its USB-C cable (in the box), a Mac with CCStudio 21.0.1 (it brings TI Arm Clang 5.1.1.LTS and DSLite), SysConfig 1.28.0 and the AM13E2x SDK 26.01.00.03 installed in `/Applications/ti`.

1. Plug in the board. It shows up as an XDS110 probe with two serial ports (`/dev/cu.usbmodem<serial>1` is the board's UART at 115200).
2. Back up the flash: `am13e230/tools/identify.sh <serial>` makes the connection file and prints which chip revision arrived. Then run `python3 tools/frl/frl.py backup --board am13e230 --probe <serial>`. The board ships blank, so the backup holds no factory image.
3. Build TI's blink example (one command, below) and flash it with DSLite. LED4 (red) blinks.
4. Change the blink rate and flash again. The faster blink is the finish line.

## One-command build

Build TI's blink with your own period and flash it over the board's USB cable (about 6 seconds in all):

```bash
am13e230/blink.sh 250
```

The number is the blink period in milliseconds (TI's example uses 500). It builds with TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0, flashes with DSLite, and prints the build and flash seconds. To prove the LED is toggling without looking at it, run `am13e230/tools/led_check.sh`: it reads the LED pin (PA5) six times over the debug probe, and changing values mean a blink.

To build everything else (CoreMark two ways, TI's ACI motor-control benchmark, the C2000 bench's FOC step with and without the TMU, and the four room-occupancy models) into a folder outside the repository (the script prints where):

```bash
am13e230/prebuild.sh --wait
```

The SDK's own build of all 100 LaunchPad examples also needs CMake, which is not installed; the script prints how to get it. It waits for a quiet Mac and installs nothing.

## If you use CCStudio's AI tools

Load the program, then check the target before you restart it. The order is: `debugProject`, read the target state, and if it says Running, `pause`; then `restart`; then `continue`. If you send `restart` while the target runs, it stays halted after `continue` and every read afterwards fails with SWD error -614. The fix is to unplug the USB cable and plug it back in. The program is already in flash by then, so the board still blinks after the replug.

## What runs on this board (2026-10-09 and 10)

Each line is a folder with its own README, build script and proof, and an experiment page with the numbers.

- **Spin the kit stepper, no soldering:** `stepper-uln2003/` (on-off, [experiment: spin the kit stepper, no soldering](../docs/experiments/2026-10-09-am13e230-stepper-uln2003.md)) and `stepper-uln2003/microstep/` (sine duties from the PWM units, 138 cycles an update with the TMU, [experiment: microstepping through the PWM units](../docs/experiments/2026-10-09-am13e230-microstep-tmu.md)).
- **Kit DC motor through an L293D:** `motor-l293d/`, built, not run on a motor.
- **MSPM0 code on this chip:** three MSPM0 programs with no application edits through a 13-line shim, [experiment: MSPM0 driverlib through a shim](../docs/experiments/2026-10-09-am13e230-mspm0-driverlib.md); shim in `../portability/shim/mspm0_on_am13e230/`.
- **TI's STM32 HAL compatibility layer:** `stm32g4-hal/`, TI's examples and ST's own GPIO and UART examples through it, [experiment: STM32G4 HAL compatibility layer](../docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md).
- **Radar host parser:** `radar-parser/`, the IWRL6432 frame parser running here from replayed captures, 62 of 62 frames, [experiment: radar frame parser](../docs/experiments/2026-10-09-am13e230-radar-host-parser.md).
- **Upstream Zephyr:** `zephyr/`, hello_world and an LED proof flashed by OpenOCD built from source, [experiment: upstream Zephyr](../docs/experiments/2026-10-09-am13e230-zephyr-upstream.md).
- **Occupancy models on the NPU:** the four occupancy models, 4.7 to 8.8 times faster than TI's CPU code on the first three and 2.4 times on the largest, [experiment: occupancy models on the NPU](../docs/experiments/2026-10-09-am13e230-npu-nnc.md).
- **How fresh AI agents did here:** 18 Sonnet 5.5 runs of blink, CoreMark and a pinout question, with and without these notes; the `agent_*` rows are in `../data/results/am13e230.csv`.
- **The one SDK example that faults** (TI's fastrts math) and its fix: [FAQ](FAQ.md).

Needs a person at the bench: the SPI pair test for the STM32 layer (three jumpers and a button), a look at the stepper shaft under the microstep image, the real UART wire from the radar, and the motor current loop (a shunt resistor per coil).

## What was measured

Run 2026-10-08 on a Rev E2 board. All numbers are in [RESULTS-am13e230.md](RESULTS-am13e230.md).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Box to changed blink, command line | 22 s | `am13e230/blink.sh 250`, then 6 pin reads of PA5 (LED4) | measured | `time_to_blink` (cli) in `data/results/am13e230.csv` |
| Box to changed blink, CCStudio's AI tools | 60 s | same blink, through CCStudio's AI tools | measured | `time_to_blink` (ccs-mcp) |
| CoreMark | 872.4 per second (4.36 per MHz) | 200 MHz, TI Arm Clang -O3, bare metal | measured | `coremark`, `coremark_per_mhz` (ticlang-bare-o3) |
| CoreMark, TI's own published figure | 870 | TI's example | claim | [RESULTS-am13e230.md](RESULTS-am13e230.md) |
| One motor-control (FOC) step with the TMU | 117 cycles | 200 MHz, from flash | measured | `foc_step` (hw-tmu-flash) |
| The same step without the TMU | 655.6 cycles | 200 MHz, from flash | measured | `foc_step` (sw-flash) |
| Room-occupancy model occ_cnn_8k, NPU against CPU code | 0.63 ms against 5.1 ms | 200 MHz | measured | `ai_occ_latency` (occ_cnn_8k-nnc-npu, occ_cnn_8k-c-o3) |
| Eighteen fresh AI agents, three jobs | 3 of 3 with these notes on blink and CoreMark, 2 of 3 without | 2026-10-10, each result checked independently | measured | `agent_pass` rows in `data/results/am13e230.csv` |

Did not run: power at J10. The list is in `RESULTS-am13e230.md`. Later runs added the UART loop (`uart_max_clean`, 6.25 Mbaud clean, UC0 to UC1), TI's own AI examples on the neural accelerator (12 of 13 timed) and the SDK example build and run, all in `data/results/am13e230.csv`.

## If you see this

- If you see `SWD error -614` (or -615) on every read, unplug the USB cable, plug it back in and read again. After `restart` was sent while the target ran, the debug port stops answering until the board is power-cycled. Read the target state after loading, `pause` if it says Running, then `restart` and `continue`. For -615 on every try, use `am13e230/AM13E23019_AL130001.ccxml` instead of your own connection file.
- If you see `usage fault (CFSR 0x80000)` in the SDK's `mathlib/fastrts` example, add a `_system_pre_init` function that turns the TMU on, or build the example at -O0. CFSR 0x80000 means an instruction for a coprocessor that is off.
- If you see `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here` from `west build -b lp_am13e230 samples/basic/blinky`, use `am13e230/zephyr/led-proof`, which writes the GPIO registers directly, or TI's own SDK example. Zephyr's board for this kit has no LED, GPIO or serial driver.

The rest of the errors, with causes: [FAQ.md](FAQ.md).

## What is not measured

Power at J10 (it needs the shunt lifted and a meter), energy per inference, the ADC maximum rate, and interrupt latency and GPIO toggle time. Each is a `waiting` row in `data/results/am13e230.csv` with the reason.

## Where these facts come from

Run 2026-10-08 (checked 2026-10-10) on an Intel Mac (macOS 15.7.9) with CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, AM13E2x SDK 26.01.00.03.STS and DSLite, probe firmware 3.0.0.41, on a Rev E2 board (prototype XAM13E23019 silicon). Files: `data/results/am13e230.csv`, `am13e230/RESULTS-am13e230.md`, `am13e230/AGENTS.md`, `am13e230/FAQ.md`.

## AI-agent jobs, timed

Three AI-agent jobs, timed on 2026-10-10, each run with these notes (told to start from `llms.txt`) and without: change TI's blink so LED4 toggles every 400 ms, run TI's CoreMark and report the score, and say which header pins carry the six MCPWM4 outputs. With the notes, blink and CoreMark passed every run in 2.2 to 9.9 minutes; without them, 2 of 3 passed, in 7.2 to 11.7 minutes. The pin question passed every run either way. Every result was checked independently: the blink by sampling the LED pin about every 10 ms (median half-period 400 ms in all passes), CoreMark against TI's 870 (871.5 to 872.0), the pins against the user guide. The failures: one run without the notes never connected (SWD error -615 on a connection file it wrote, see the FAQ), and one used an older compiler and reported 777 instead of 872.
