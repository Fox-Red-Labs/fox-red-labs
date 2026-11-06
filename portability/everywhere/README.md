# Which TI board can run which TI example: the map, the plan and the proof

**Short answer.** Most TI examples move easily inside one driver layer and stop at the wall between layers. This folder holds the map that predicts which of the 3.3k examples in seven TI SDKs another TI board could run (14,545 of 44.2k example-to-board cells are Should work), the scripts that build the cells worth trying, and the results. `inventory.py` reads every example and writes down what it needs. `profiles.json` says what each board has. `feasibility.py` puts the two together and gives every cell a verdict and a named blocker before anything is built (`SUMMARY.md`). The cross-build scripts then try the cells, and the results replace the predictions in `portability/matrix.csv`, one batch at a time.

Proven (2026-10-10 and 11): the eleven TI Drivers examples that the F3 SDK and the Wi-Fi SDK share build unchanged in both directions, 22 of 22. MSPM0 DriverLib examples reach the LP-MSPM33C321A: 20 of 82 drop in unchanged (19 ran, 17 with clean fault registers), and 43 of 83 build when their own SysConfig file comes along (40 ran, 39 clean). Across a second family boundary, 20 of 43 MSPM0 GPIO, UART, SysTick and timer examples build on the LP-AM13E230 through a SysConfig translation, and all 14 runnable images ran clean.

Terms: [docs/vocab.md](../../docs/vocab.md). Verdict words, ladder and rules: [portability/README.md](../README.md). 1 kB = 1,024 bytes. To port something, start with [PORTING.md](PORTING.md).

Contents: what the map says, how it works, results, the queue, known limits, how to run it.

## What the map says on paper (2026-10-10)

From `SUMMARY.md`, generated, not typed. This snapshot is from 2026-10-10: 3.3k examples, 14 boards, 44.2k cells. `SUMMARY.md` is regenerated and now holds newer totals (3.4k examples, 16 target boards, 52.5k cells).

| Verdict | Cells | Share |
|---|---|---|
| Should work (same SDK or a shim, every block present) | 14,545 | 33 percent |
| Could work (a block missing or renamed, or a size wall) | 4,360 | 10 percent |
| Does not exist (the example's driver layer is not on the target) | 22,501 | 51 percent |
| Does not make sense (needs a radio, Wi-Fi, USB or a second core the target lacks) | 2,821 | 6 percent |

Seven findings:

1. **The driver layer is the wall, not the chip.** Half of all cells fail on one rule: the example is written to a driver library that does not exist on the target. MSPM0's DriverLib (1.8k examples, 56 percent of everything TI ships here) has no landing on any wireless part, the AM263P or C2000. TI Drivers (the F3 and Wi-Fi SDKs' layer) has no landing on MSPM33, AM13, AM263P or C2000. AM13's driverlib, the AM263P's MCU+ drivers and C2000's driverlib each exist on exactly one family. Within a layer, portability is high: inside MSPM0, 122 of 122 C1106 examples and 262 of 282 G3507 examples should land on the G3519; inside TI Drivers, 108 of 109 CC2340R5 examples should land on the CC2755P10.
2. **The two shims that exist carry a lot.** The MSPM0-on-AM13 DriverLib shim (13 lines) makes 205 G3507 examples Should work on the AM13E230. The MSPM0-on-F3 TI Drivers shim rescues only the 51 MSPM0 TI Drivers examples, because the other 1.8k MSPM0 examples do not use TI Drivers.
3. **Inside a layer, naming breaks first, not the API.** On the first MSPM0-to-MSPM33 batch, 20 of 82 DriverLib twins build unchanged, and 54 of the 62 failures are a SysConfig instance name or a DMA interrupt name that the two SDKs spell differently for the same example. Only 4 hit a real API difference. MSPM0 to F3 showed the same (board-config names).
4. **Inside one family the same naming habit costs a third of the ports.** G3519 to G5187: 141 of 258 build after the SysConfig device switch. Of the 117 that do not, 41 name a UART, SPI, I2C, TRNG or timer instance by its silicon name (`UART0`, `SPI1`, `TIMG12`) instead of the alias SysConfig generated, and 10 need a block the G5187 does not have.
5. **A SysConfig translation bridges a different module namespace.** MSPM0 to AM13: all 43 single-block SysConfig files translate module by module, 20 build, 14 run. The rest stop on aliases the AM13 generator does not emit and on blocks the AM13 names differently. The same tool shape fits any pair with a shared driver vocabulary.
6. **Plain C reaches the processors no driver layer does.** The MSPM0 CMSIS-DSP FFT example ran unchanged on the AM263P's R5F with one line of glue, and on the SK-TDA4VM's A72 under Linux with a host harness. Both gave the same answer as this Mac. The AM263P and C2000 columns of the map are Does not exist for every driver layer, and plain C is what crosses them.
7. **Size is the next wall after the layer.** The LP-MSPM0C1104's 1 kB of RAM turns 570 TI Drivers cells and 371 FreeRTOS cells into Could work. The map uses measured image sizes where a sweep produced one (341 examples) and a rule otherwise.

## How it works

### 1. Inventory: what each example needs (`inventory.py`, `vocab.json`)

One JSON line per example in `data/inventory.jsonl`. The reliable signal is the example's SysConfig file: every `addModule("/ti/driverlib/ADC12")` or `addModule("/ti/drivers/UART2")` line names a block. `vocab.json` maps the seven SDKs' module names to one tag each (`/ti/driverlib/UART`, `/ti/drivers/UART2`, `/drivers/uart`, `/drivers/uart/uart` and C2000's `SCI_` calls are all `UART`), and maps header paths and folder names to the software layer (`driverlib_msp`, `tidrivers`, `am13_driverlib`, `mcuplus_drivers`, `c2000_driverlib`, `stm32_hal`, `rtos_freertos`) and to the blocks a `.syscfg` file does not show (a `/ble/` folder is `RADIO_BLE`, an `/ai/` folder is `AI_MODEL`). Where an SDK rarely ships a `.syscfg` (C2000Ware, AM13 driverlib), the sources' call prefixes stand in. A native image size is attached where one of this repo's sweeps built the example (341 of 3.3k).

The SDK trees and their layouts are the `SDKS` table at the top of the script: MSPM0 2.11.00.07, MSPM33 1.04.00.00, F3 9.21.00.36, Wi-Fi 10.20.00.39, AM13E230x 26.01.00.03, MCU+ AM263Px 26.01.00.08, C2000Ware 26.02.00.00 (f280013x only). Examples per board: C1104 91, L1306 170, G3507 282, G3519 270, G5187 207, MSPM33C321A 260, CC2340R5 109, CC2755P10 107, CC35X1 32, AM13E230 96, AM263P 262, F2800137 115. The F2 SDK for the LP-CC1354P10-1 and the F28P65x part of C2000Ware are not installed, so those two boards are targets only (their own examples are not in the inventory).

### 2. Profiles: what each board has (`profiles.json`)

One entry per target board: flash, RAM, core, the layers it can run an example on (native, partial, shim, size), what SysConfig modules its own SDK examples exercise (computed by `feasibility.py` from the inventory, so a block counts when TI's own examples use it on that board), hand entries for blocks the examples do not show (the NPU on the G5187 and AM13, Thread on the CC27xx, the HSM) with the public source of each, and blocks to remove when the examples mislead (a CAN example on the CC2340R5 drives an external controller). The `bridges` table lists every (layer, source family, target family) pair with its mechanism (same SDK retarget, a shim in `portability/shim/`, shared names for some blocks only, or nothing) and the `matrix.csv` row that measured it.

### 3. Feasibility: the verdict per cell (`feasibility.py`)

Rules fire in order: a missing radio, Wi-Fi, USB, display, Ethernet, second core or PRU is Does not make sense; a missing layer with no bridge is Does not exist; a missing ordinary block, a block whose API differs on a partial bridge, FreeRTOS or TI Drivers on a board where they fit only on paper, or a native image larger than the target's flash or RAM with 10 percent headroom, is Could work with the blocker named; otherwise Should work. Each cell also carries whether the bridge it rests on is measured (a `matrix.csv` row), inferred from a neighbour, or not run. `SUMMARY.md` is the source-by-target table, the blocker ranking, the build queue (Should work cells grouped by route, not run first) and each board's capability list.

### 4. Cross-build: predictions into results

The queue in `SUMMARY.md` says which batch to run next. Each batch is one script that builds the source board's examples in the target's project, baseline first (the target's own example, untouched) and then swapped (the source's app files dropped in, no edits), and writes one JSON line per build. Existing scripts cover MSPM0 to MSPM0 (`port_mspm0.py`), MSPM0 to F3 and back and F3 to F3 (`portability/crossbuild/`), MSPM0 to AM13 (`portability/shim/mspm0_on_am13e230/`). Also here: `crossbuild_f3_wifi.sh` (F3 and Wi-Fi SDK, both ways). Each finished batch becomes a `matrix.csv` row, and the bridge's evidence in `profiles.json` changes from not run to measured, so the next `feasibility.py` run reflects it.

### 5. Silicon

A batch that builds is run on the target with the board's backup and restore (`tools/board_sweep/run_all.py`, the board folders' `run_sweep.py`). A board that runs something else stays at build level until it is free. The matrix row climbs from built to ran to behavior verified.

## Results

Each row is one batch. The last column is the id of its row in `portability/matrix.csv`. Counts are examples, "built" means built with no edit to the app files unless the row says otherwise, and "ran" means flashed and read back on the board with backup and restore.

| Batch (date) | What moved | Built | Ran | Matrix id |
|---|---|---|---|---|
| 2026-10-10 | LP-EM-CC2340R5 TI Drivers examples in the LP-EM-CC35X1 projects (Wi-Fi SDK) | 11 of 11 twins | programmed; console output not seen | X26 |
| 2026-10-10 | LP-EM-CC35X1 TI Drivers examples in the LP-EM-CC2340R5 projects (F3 SDK) | 11 of 11 twins; app sources byte-identical across the two SDKs; the F3 image is 2 to 7 kB smaller for 9 of 11 | built only | X27 |
| 2026-10-10 | LP-MSPM0G3519 DriverLib examples in the LP-MSPM33C321A projects (MSPM33 SysConfig and linker kept) | 20 of 82 twins (83 by name; one is a library with no app) | 19 of the 20 (the AES one skipped as a key example): 17 clean, 1 hard fault (`sysctl_lfxt_standby`, a bus fault in the MSPM0 low-power sequence), 1 unreadable while asleep | X28 |
| 2026-10-10 | The same examples carried with their own `.syscfg` (device header swapped, pins unlocked, MSPM0 clock-tree lines dropped, each rejected line commented and listed) | 43 of 83 | 40 of the 43 (3 skipped as key, flash-protection and NONMAIN examples): 39 clean, 1 hard fault (`timx_timer_mode_periodic_standby`) | X29 |
| 2026-10-10 | Every LP-MSPM0G3519 example retargeted to the LP-MSPM0G5187 in SysConfig (`port_mspm0.py`) | 141 of 258 with a makefile route (90 with pin moves only, 51 with a dropped setting listed) | not run | X30 |
| 2026-10-11 | LP-MSPM0G3519 DriverLib examples carried to the LP-AM13E230 by SysConfig translation (`crossport_mspm0_am13.py`) plus the 13-line shim; the 43 use only GPIO, UART, SysTick, a timer or SYSCTL | 20 of 43 | 14 of 14 runnable images ran clean; the LED-rotating examples were seen toggling PA5 to PA8 | X31 |
| 2026-10-11 | TI's MSPM0 CMSIS-DSP FFT example on the LP-AM263P Cortex-R5F, app unchanged in the MCU+ `empty` project with a one-line init shim | 1 of 1 | peak bin 5, the example's own expected answer ([am263p-fft](am263p-fft/README.md)) | X32 |
| 2026-10-11 | The same example on the SK-TDA4VM Cortex-A72 with the board's gcc, a 14-line CMSIS-core stand-in and an 11-line host harness | 1 of 1 | `gFFTmaxFreqIndex=5 gFFTmaxValue=4023 PASS`, the same as on this Mac ([host-fft](host-fft/README.md)) | X33 |
| 2026-10-11 | The IWRL6432 radar frame parser (written for the AM13) on the SK-TDA4VM A72, run on a 60 s recorded capture | 1 of 1 | 231 of 231 frames identical to the Mac reference ([host-radar](host-radar/README.md)) | X34 |
| 2026-10-11 | AM263P MCU+ driver examples in the radar SDK's projects (MMWAVE-L-SDK 05.05.04.02) | 11 of 23 as is; 19 of 23 with one generated mapping header ([mcuplus-compat](mcuplus-compat/README.md)) | `uart_echo` and `gpio_led_blink` on the IWRL6432BOOST, flashed with arprog | X35 |
| 2026-10-11 | The reverse: radar SDK examples in AM263Px projects | 11 of 19 as is; 15 of 19 with the other header | built only | X36 |

Of the 62 MSPM0-to-MSPM33 failures in the unchanged batch, 43 are SysConfig instance names that differ (the LED is `USER_LED_1` on MSPM0 and `PASS_LED` on MSPM33), 11 are DMA names and 4 are real API differences (I2C clock config, interrupt groups, TIMA0, BCR). A one-line rename in the `.syscfg` would put about 60 of 82 through.

The F3 and Wi-Fi twins are adcsinglechannel, empty, exception, gpiointerrupt, i2ccontroller, pwmled1, sha2hash, spicontroller, spiperipheral, uart2echo and watchdog. The Wi-Fi SDK's other 21 examples per board (CAN, I2S, PDM, PSA crypto, ITM) have no F3 twin by name. The F3 SDK's other 25 driver examples (AES, NVS, SD, display, log, i2ctarget) have none in the Wi-Fi SDK.

## The queue, in order

From `SUMMARY.md`, not run bridges with the most Should work cells first:

1. MSPM0 DriverLib on the LP-MSPM33C321A: driverlib measured both ways of porting (drop-in and configuration port, both run). Next: the DMA cells (teach the port to add an MSPM33 DMA instance and alias `DMA` to `DMA0`, which the map says frees 11 more), the crystal-pin conflicts (7, a board-file question), the crypto (39 twins), edgeAI (5), cmsis_dsp, lin and boot_manager categories, then the reverse direction.
2. The rest of MSPM0 DriverLib on the LP-AM13E230: PWM through mcpwm, I2C and SPI, then DMA as DMA0.
3. F3 and Wi-Fi SDK examples on the LP-CC1354P10-1 (F2 SDK) once the F2 SDK is back on the disk.
4. AM263Px and radar: run the eight examples the mapping header newly builds (the watchdog first, to prove the start fix), then an adapter for the two GPIO interrupt models.
5. The within-MSPM0 cells the port tool has not run (G5187 and C1106, H3216, L2117, L2228 as sources onto the five MSPM0 boards here).
6. Silicon runs for every batch that built, board by board.

## Known limits of the map

- A tag comes from TI's SysConfig file or folder name, so an example that opens a block without SysConfig (register writes in C) can hide a need; the sources' call prefixes catch most of these but not all.
- "Could work" with a missing block is a prediction about the example as shipped; many examples use a block for the demo (an LED on a second port) that the port would simply drop. The within-MSPM0 sweeps measured that: SysConfig drops such settings and the image still builds , which is why those rows count "behavior changed" builds separately.
- Image sizes exist for 341 examples; the other cells are judged on blocks and layer only.
- Non-TI boards (ST, NXP, Raspberry Pi, Espressif) are not targets in this map. Plain C and Zephyr examples move to them by the same rules. Adding one means one new profile.

## How to run it

To port something, start with `PORTING.md`: the method, which tool fits which pair of boards, and the traps that cost runs.

| Tool | What it does |
|---|---|
| `inventory.py`, `vocab.json` | every example in the eight SDKs and what it needs |
| `profiles.json`, `feasibility.py` | what each board has; a verdict per example and board |
| `crossport_mspm0_mspm33.py` | MSPM0 example to the MSPM33C321A with its own SysConfig file |
| `crossport_mspm0_am13.py` | MSPM0 example to the AM13E230 through a SysConfig translation and generated alias header; `--run` flashes and restores |
| `crossbuild_f3_wifi.sh` | F3 SDK and Wi-Fi SDK twins, both ways |
| `crossbuild_am263p_mmwave.sh` | AM263Px and radar SDK twins, both ways; `COMPAT=`, `ONLY=`, `MODES=` options |
| `crossbuild_mspm0_mspm33.sh` | the plain drop-in of MSPM0 apps into MSPM33 projects |
| `mcuplus-compat/` | generated mapping headers between the AM263Px and radar SDKs |
| `port_runlist.py`, `run_cc35x1.sh`, `dss_run_read.py` | run lists for `tools/board_sweep/run_all.py`; Wi-Fi Toolbox programming; one-run load, run and read |
| `am263p-fft/`, `host-fft/`, `host-radar/` | plain C examples on the AM263P R5F, a Linux board and the Mac |

```bash
cd portability/everywhere
python3 inventory.py                      # data/inventory.jsonl, prints examples per board
python3 feasibility.py                    # data/map.jsonl, data/summary.json, SUMMARY.md
./crossbuild_f3_wifi.sh <scratch> <results.jsonl> f3_to_wifi
./crossbuild_f3_wifi.sh <scratch> <results.jsonl> wifi_to_f3
```

Builds need TI Arm Clang 5.1.1 (CCStudio 21.0.1), SysConfig 1.26.3 for the F3 SDK and 1.28.0 for the Wi-Fi SDK, and the Wi-Fi Toolbox 4.3.23, all in `/Applications/ti`. Nothing writes into `/Applications/ti`.
