# What can the TI LAUNCHXL-F2800137 and LAUNCHXL-F28P65X LaunchPad development kits do, measured from a Mac terminal?

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer.** It builds, loads and runs TI's examples from a Mac terminal with no board changes: 138 of 139 C2000Ware examples for this chip build by script. Do two things first: check the onboard probe's firmware with `xdsdfu -e` and update it if it is older than 3.0.0.43, and use a connection file (`.ccxml`, the file that tells TI's tools which probe and chip to use) that sets the 2-pin cJTAG mode. The motor-control number: a PWM-triggered sample, motor-control step and PWM update takes 2.3 to 2.6 µs.

Terms: [docs/vocab.md](../docs/vocab.md). Every problem hit on this bench, with its cause and fix: [FAQ](FAQ.md). 1 kB = 1,024 bytes. The **probe** is the XDS110 debugger built into the board; **cJTAG** is the 2-wire debug link it uses; the **TMU** is the chip's trig and divide hardware. **CCStudio** is TI's IDE (only its command-line scripting runner was used), **UniFlash** TI's flashing tool, **SysConfig** TI's tool that writes the pin and clock setup code, **C2000Ware** TI's software kit for C2000 chips, and **`ESTOP0`** the C28x instruction where TI's examples stop for a debugger.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` and `FAQ-f28p65x.md` (every error hit, with its exact text and the fix); `tools/` (build, load, back-up and restore scripts for these boards, and the two connection files). Results are rows in `data/results/f2800137.csv` and `data/results/f28p65x.csv` at the top of the repository.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below; raw run logs, which are lab records and not something to run or flash.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add c2000-launchpad tools data
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | LAUNCHXL-F2800137 | LAUNCHXL-F28P65X |
|---|---|---|
| Chip | TMS320F2800137 (marked `X F2800137PM`, silicon revision B): C28x CPU at 120 MHz with a floating-point unit and the TMU (trig and divide hardware) | TMS320F28P650DK9: dual-core C28x at 200 MHz, one CLA (control law accelerator) per CPU |
| Memory | 256 kB flash, 36 kB RAM | 1.28 MB flash, 248 kB RAM |
| Peripherals | Two 12-bit ADCs, 14 PWM channels, CAN, one motor-encoder input | Not listed here |
| Debugger | Onboard XDS110, 2-pin cJTAG only as shipped | Onboard XDS110 |
| LED | GPIO20 | LED4 on GPIO12 |
| Where to buy | <https://www.ti.com/tool/LAUNCHXL-F2800137> | Not listed here |
| Not on the chip | Not listed here | No radio, no camera interface, no video block |

Sources: `../docs/boards/launchxl-f2800137.json`; user guide SPRUJ32 (<https://www.ti.com/lit/ug/spruj32/spruj32.pdf>); data sheet SPRSP63B (<https://www.ti.com/lit/ds/symlink/tms320f2800137.pdf>); `AGENTS.md` for the F28P65X rows; `FAQ-f28p65x.md`.

### What was found on the board, and the setting to change

- **The F2800137 probe arrived on firmware 3.0.0.19, and TI's tools would not connect until it was on 3.0.0.43. Tag: Tool or docs.** UniFlash's command line printed `A firmware update is required for the XDS110 probe` and stopped with `Failed: Timed out after 45 seconds`; there is no Update button on the command line.
- **A connection file that leaves the debug mode to the device default fails with error -2131, `Unable to access device register`. Tag: Unknown (suspected: Tool or docs).** Set `SWD Mode Settings` = 4 (cJTAG 2-pin advanced), as TI's `TMS320F2800137_LaunchPad.ccxml` in C2000Ware does; `tools/F2800137_CL371081.ccxml` is that file plus the test probe's serial number.
- **4-pin JTAG does not work on this board as shipped. Tag: Setup or handling.** The board wires the probe for 2-pin cJTAG only; TDI and TDO need resistors R44 and R45 fitted (user guide SPRUJ32, section 3.2.1).
- **The chip** is marked `X F2800137PM` and reads back as a TMS320F2800137, silicon revision B (`data/chip-id-0x5D008.bin`). It arrived running TI's out-of-box demo; the flash as received is saved in `data/flash-as-received.bin`, read twice with identical hashes.
- **TI's tool warns `Configuring Debugger (may take a few minutes on first launch)...`.** On this bench that step took 1.8 s; the first launch was not timed, so give the first connection a long timeout.

Details and commands for each: [FAQ, Connecting](FAQ.md#connecting).
## Ten-minute path (from a Mac terminal)

The `tools/` scripts are in this repo's `c2000-launchpad/` folder, and they expect CCStudio in `/Applications/ti/ccs2101` and C2000Ware in `/Applications/ti/c2000ware_core_26_02_00_00`; edit the paths at the top of each script if yours differ.

1. **Install CCStudio 21.0.1** with every device family, which brings the C2000 compiler and SysConfig: [`../mac-setup/README.md`](../mac-setup/README.md), steps 1 to 5 (about 10 GB of disk).
2. **Get C2000Ware** from TI's public GitHub repo (<https://github.com/TexasInstruments/c2000ware-core-sdk>), only the parts this chip needs. These commands give the same files as the checkout used here (its remote, tag and folder list read back with `git`; an earlier list also named `kernel/freertos_tool`, which does not exist at this tag):

```bash
git clone --filter=blob:none --depth 1 --sparse --branch REL_C2000Ware_v26.02.00.00.LTS https://github.com/TexasInstruments/c2000ware-core-sdk.git /Applications/ti/c2000ware_core_26_02_00_00
```

```bash
git -C /Applications/ti/c2000ware_core_26_02_00_00 sparse-checkout set .metadata boards device_support/f280013x driverlib/.meta driverlib/f280013x libraries utilities/clb_tool/clb_syscfg utilities/cmd_tool utilities/dcsm_tool/dcsm_syscfg utilities/transfer
```

3. **Plug in the board and update the probe** if `xdsdfu -e` shows firmware older than 3.0.0.43: the two commands are in the [FAQ](FAQ.md#why-does-tis-software-refuse-to-connect-to-my-launchxl-f2800137-and-how-do-i-update-the-probe). Then put your probe's serial number in place of `CL371081` in `tools/F2800137_CL371081.ccxml`.
4. **Copy TI's blink example and build it for RAM**, from the `c2000-launchpad/` folder (the build goes into the copy's `build-RAM-Ooff/` folder):

```bash
cp -R /Applications/ti/c2000ware_core_26_02_00_00/driverlib/f280013x/examples/led /tmp/led_ex1_blinky
```

```bash
tools/c2kbuild.sh /tmp/led_ex1_blinky RAM
```

5. **Load it and watch the LED pin** (GPIO20, sampled 8 times 250 ms apart):

```bash
/Applications/ti/ccs2101/ccs/scripting/run.sh tools/c2krun.js /tmp/led_ex1_blinky/build-RAM-Ooff/led_ex1_blinky.out blink
```

This is a RAM load: the program is gone at power-off. To keep it, build with `FLASH` in place of `RAM` and run the same command on `build-FLASH-Ooff/led_ex1_blinky.out`; the debugger then writes it to flash. That is how the bench and sweep loaded their flash images; the blink example from flash was not timed.

`c2kbuild.sh` runs SysConfig, compiles with TI's projectspec flags (CPU1_LAUNCHXL configuration) and links. Separate UniFlash command-line calls each reconnect, so they cannot watch a running program; use the scripting runner for that.

For the LAUNCHXL-F28P65X, the same steps use `tools/c2kbuild_f28p65x.sh <example folder> RAM` (CPU1 only) and `tools/c2krun_f28p65x.js <prog>.out blink`, which samples GPIO12 (LED4); its connection file is `tools/F28P650DK9_CL650001.ccxml`. The F28P65X blink was built and seen changed by eye, not timed (results in `data/results/f28p65x.csv`).

## One-command build

From the `c2000-launchpad/` folder, one script runs SysConfig, compiles with TI's projectspec flags (CPU1_LAUNCHXL configuration) and links the copied example (steps 4 and 5 above give the copy and the load):

```bash
tools/c2kbuild.sh /tmp/led_ex1_blinky RAM
```

It took 4.6 s for TI's blink on the F2800137 (observed 2026-10-06; not saved). It does not load the program: use the scripting runner in step 5, because separate UniFlash command-line calls each reconnect and cannot watch a running program. To build every C2000Ware example for the chip, loop `tools/c2kbuild.sh` over the example folders: 138 of 139 build.

## Putting it back on TI's demo

To return the LAUNCHXL-F2800137 to TI's out-of-box demo, write the saved flash image back with `tools/c2kflashback.js` and reset. This was done twice; the flash read back identical to the saved image each time (steps in the [FAQ](FAQ.md#how-do-i-put-the-launchxl-f2800137-back-on-tis-out-of-box-demo-and-how-do-i-know-it-worked)).

## What was measured

LAUNCHXL-F2800137 unless the row says F28P65X. 120 MHz; RAM means code in zero-wait RAM, flash means 2 wait states with TI's prefetch and cache. Cycle counts for math functions include the benchmark loop's own overhead. Sources are rows in `data/results/f2800137.csv` and `data/results/f28p65x.csv`.

| What | Number | Conditions | Level | Source row |
|---|---|---|---|---|
| Blink, TI's `led_ex1_blinky`, terminal build and RAM load | 4.6 s build, 3.9 s load | Observed 2026-10-06, output not saved. Sweep: 4.1 s load and the LED pin seen toggling. Box to blink with a flash write: not measured | measured | `blink_build`, `blink_flash` |
| sinf, atan2f, divide, cycles per loop iteration, TMU flags | 8.0, 17.0, 8.0 from RAM; 12.0, 20.0, 12.0 from flash | Includes 7 (RAM) or 9 (flash) cycles of loop overhead | measured | `math_divide`, `foc_step`, `control_loop`, `isr_entry` |
| sinf, atan2f, divide, C library | 82.0, 697.0, 241.2 from RAM | Same loop | measured | `math_divide`, `foc_step`, `control_loop`, `isr_entry` |
| One field-oriented control (FOC) step, the standard motor-control math, float | 194 cycles with TMU flags (RAM), 221 (flash); 349 without them (RAM) | Per loop iteration | measured | `math_divide`, `foc_step`, `control_loop`, `isr_entry` |
| PWM trigger to new PWM value, FOC step in between | 2.3 to 2.6 µs (means), 12 to 13% of a 50 kHz period; 3.7 to 4.1 µs without the TMU flags | End-of-conversion interrupt; across RAM, flash, and flash with the loop copied to RAM | measured | `math_divide`, `foc_step`, `control_loop`, `isr_entry` |
| CoreMark, patched for the 16-bit byte (**unofficial**) | From flash 141.0 per second, 1.175 per MHz (51% of the MSPM0G3507's 2.30, also from flash); from RAM 155.3, 1.294 per MHz | Unmodified, it does not validate | measured | `coremark`, `dhrystone` |
| Dhrystone 2.1 | 0.362 DMIPS per MHz from flash (50% of the MSPM0L1306's 0.731, also from flash); 0.442 from RAM | Rules followed | measured | `coremark`, `dhrystone` |
| Room-occupancy models occ_cnn_500, 2k, 8k, 30k (636 to 32.8k weights), plain C, from flash | 1.8, 4.8, 17.6, 65.4 ms per inference | 20 of 20 test vectors right in every build | measured | `ai_occ_latency` |
| Same models built by TI's Neural Network Compiler (NNC), from flash | 1.12, 2.60, 9.05, 30.8 ms; 38 to 53% fewer cycles than the fastest plain-C build from flash | 20 of 20 test vectors right | measured | `ai_occ_latency` |
| Float FFT, TI's FPU DSP library | Complex 256 points 0.107 ms; 1,024 points 0.52 ms from RAM, 0.52 to 0.60 ms from flash | | measured | `fft256`, `adc_max_rate` |
| Embench-IoT | 19 of 19 fit and run; 15 pass their own check | | measured | `embench_fit` |
| ADC rate, no wiring | 2.10 million samples a second (Msps) at TI's sample window; 3.98 Msps at 9 cycles | 9 cycles (75 ns) is the data sheet's minimum | measured | `fft256`, `adc_max_rate` |
| TI's C2000Ware examples | 138 of 139 build from a terminal | TI's own flags plus three additions the projects do not list. Running is not passing; most need wiring to show they work | measured | the example build count in this README |
| TI's examples that report fail | 4 clock-check (DCC) examples fail as shipped; all 4 pass with a define or one SysConfig line. All 11 AI examples pass their golden-vector check | | measured | [FAQ](FAQ.md) |
| F28P65X CoreMark, patched | 258.86 per second from RAM (1.294 per MHz); 211.79 from flash (1.059 per MHz) | 200 MHz | measured | `coremark`, `coremark_per_mhz` in `data/results/f28p65x.csv` |
| F28P65X Dhrystone | 0.444 DMIPS per MHz from RAM; 0.279 from flash | TI C2000 compiler -O3, no inlining | measured | `dhrystone` in `data/results/f28p65x.csv` |
| F28P65X room-occupancy models | 0.893 ms (occ_cnn_500) to 47.9 ms (occ_cnn_30k, from flash) | 20 of 20 vectors; occ_cnn_30k did not fit the shared RAM layout | measured | `ai_occ_latency` in `data/results/f28p65x.csv` |
| F28P65X serial port loopback | Clean to 12.5 Mbaud | Internal loopback | measured | `uart_max_clean` in `data/results/f28p65x.csv` |

## If you see this

- If you see `A firmware update is required for the XDS110 probe` and then `Failed: Timed out after 45 seconds` from UniFlash's command line, update the probe with `xdsdfu` (the board arrived on 3.0.0.19; TI's tools need 3.0.0.43). Check first with `xdsdfu -e`. The two update commands are in the [FAQ](FAQ.md#why-does-tis-software-refuse-to-connect-to-my-launchxl-f2800137-and-how-do-i-update-the-probe).
- If you see `Error -2131 @ 0x0) Unable to access device register` from a hand-written connection file, put the property `SWD Mode Settings` = 4 (cJTAG 2-pin advanced) in its connection section, or start from `tools/F2800137_CL371081.ccxml`.
- If you see `Failed: An invalid processor ID has been found.` on the LAUNCHXL-F28P65X, start from C2000Ware's own `device_support/f28p65x/common/targetConfigs/TMS320F28P650DK9.ccxml`, copy it and add only the probe's serial number ([FAQ-f28p65x.md](FAQ-f28p65x.md)).

The rest of the errors, with causes: [FAQ.md](FAQ.md) (F2800137) and [FAQ-f28p65x.md](FAQ-f28p65x.md) (F28P65X).

## Where to look next

1. [FAQ.md](FAQ.md): every error, with its cause and fix.
2. `data/results/f2800137.csv` and `data/results/f28p65x.csv`: every number on this page, with its conditions.

## Where these facts come from

Checked 2026-10-06 (F2800137) and 2026-10-08 (F28P65X) on an Intel Mac, macOS 15.7.9: CCStudio 21.0.1 (C2000 compiler 25.11.1.LTS, SysConfig 1.28.1, scripting), UniFlash 9.6.0, C2000Ware 26.02.00.00 LTS core from TI's public GitHub repo (427 MB on disk including its 87 MB git folder; no myTI login), probe firmware 3.0.0.43. Files: `data/results/f2800137.csv`, `data/results/f28p65x.csv`, `c2000-launchpad/AGENTS.md`, `c2000-launchpad/FAQ.md`, `c2000-launchpad/FAQ-f28p65x.md`.
