# TI LP-AM13E230 LaunchPad development kit: notes for an AI coding assistant

That the board ships with empty flash (only the power LEDs light until you load a program), that a hand-written connection file can fail with SWD error -615 on every try, and that TI's fastrts math example faults unless the TMU is switched on before `main()`. This file is the short list, each item comes from runs on one board unless it says otherwise.

Errors and fixes: the [LP-AM13E230 FAQ](FAQ.md) in the Fox Red Labs repo. Terms: the **TMU** is a trig math unit (sine, cosine, divide in a few clock cycles); the **NPU** is a neural-network accelerator; **FOC** (field-oriented control) is the math a motor drive runs every PWM period; **DSLite** is the command-line flasher inside CCStudio; **NONMAIN** is the chip's boot configuration area; **CoreMark** is a standard CPU speed test. 1 kB = 1,024 bytes. Checked 2026-10-08 to 2026-10-10 on an Intel Mac, macOS 15.7.9, with AM13E2x SDK 26.01.00.03.STS, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, UniFlash 9.6.0, CCStudio 21.0.1 and probe firmware 3.0.0.41, on one Rev E2 LP-AM13E230 (prototype XAM13E23019 silicon).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `am13e230/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | AM13E23019: Arm Cortex-M33 at 200 MHz with FPU, a TMU reached as custom instructions, and a TinyEngine NPU; 512 kB flash, 128 kB RAM (`am13e230/README.md`) |
| Peripherals | Five motor PWM modules, three 12-bit ADCs at 7.81 million samples a second, programmable-gain amplifiers, fast comparators (`am13e230/README.md`) |
| Board | Isolated debug probe, CAN-FD transceiver, 5 V encoder header, red and green LEDs, wake and bootloader buttons, power header J10, 40-pin BoosterPack headers. No motor driver (`am13e230/README.md`) |
| Revision | Read the back: `MCU178E2` is E2 (what was run); `MCU178E1` has different pins (user guide SLVUDH9, section 3.1) |
| LED | LED4 red on PA5 (`am13e230/README.md`) |
| Serial | Backchannel UART at 115200 on `/dev/cu.usbmodem<serial>1` (`am13e230/README.md`) |
| MCPWM4 outputs | All on header J4: 1A pin 40 (PB3), 1B pin 39 (PA7), 2A pin 38 (PB1), 2B pin 37 (PA2), 3A pin 36 (PB2), 3B pin 35 (PA25); 1B, 2A and 3B are routed differently on E1 (`am13e230/FAQ.md`) |

## Start here

Built with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS, DSLite), SysConfig 1.28.0, the AM13E2x SDK 26.01.00.03.STS in `/Applications/ti/am13e230x_sdk_26_01_00_03` and UniFlash 9.6.0; probe firmware 3.0.0.41. Check the revision on the back first: `MCU178E2` is what was run. The probe serial `<serial>` comes from `xdsdfu -e` (in CCStudio's `ccs_base/common/uscif/xds110/` folder).

| Step | Command |
|---|---|
| Identify (read-only; writes the connection file) | `am13e230/tools/identify.sh <serial>` |
| Backup | `python3 tools/frl/frl.py backup --board am13e230 --probe <serial>` reads each region twice and compares. The regions: main flash 0x00000000, 0x20000 words (512 kB); NONMAIN 0x60100000, 8 kB, read only and never written back. By hand: `DSLite memory -c <ccxml> -r <addr>,<words> -o <file>`. The board shipped blank, so there is no factory image to keep (`am13e230/FAQ.md`) |
| Build and flash the blink (period in ms) | `am13e230/blink.sh 250 <serial>` |
| Check | `am13e230/tools/led_check.sh <serial>` reads PA5 (LED4, red) six times; changing values mean a blink |
| Start a flashed image reliably | `DSLite flash -c <ccxml> -e -f -v -r 1 -u -t 60 <image>.out` |
| Restore | `python3 tools/frl/frl.py restore <backup folder> --probe <serial>`. The shipped flash was blank, so a restore here puts back a blank chip; TI's `gpio_toggle_output` was left running |

`blink.sh` runs SysConfig, compiles with `tiarmclang -O2 -mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mfpu=fpv5-sp-d16 -mfloat-abi=hard`, links against the SDK's prebuilt libraries and flashes with DSLite. Box to changed blink took 22 s from the command line (build 2.7 s, flash 3.3 s) and 60 s through CCStudio (`data/results/am13e230.csv`, `time_to_blink`). The SDK's own `make` needs `CCS_PATH=/Applications/ti/ccs2101/ccs` and `SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0`, plus CMake and Ninja (used here: CMake 4.4.4 and Ninja from a pip venv).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| A flash write passes verify (an on-chip check and a read-back) on the MSPM33 or AM13, but the new program never starts: the LED does not change, or the old program still runs | A program that switched the flash caches on (TI's examples do at 160 MHz) leaves old flash lines in them. The debugger reads flash past the caches, so verify passes, and the reset then fetches the old vector table from the cache. Seen 2026-10-22: PC inside the previous image after a correct write | Clear `FRI FRD_INTF_CTRL` (0x4002900C: bit 1 data cache, bit 2 code cache; same address on both chips) while the core is halted, write, then reset. The Digital Lab does it | The LED toggles at the image's rate after the reset |
| MSPM33 or AM13 does not answer the debugger after an interrupted write or an erased vector table, and a reset or replug does not clear it | The boot code faults before the debugger can stop the core (CFG-AP `BOOTDIAG` reads 0x08000007) | Wait-for-debug with the register sequence CCStudio's debug server writes, taken from its own log: ten times (SPREC bit 0, DPREC0 bit 14, SPREC bit 0) at AP 4, then DPREC0 0x002C4027, 0x000C6027, 0x000C602F, DHCSR halt requests with DEMCR VC_CORERESET set, DPREC0 0x000C202F, 0x0048202F, 0x0008602F, 0x00120008. The GEL file's shorter hold-then-0x00120008 is not enough on a chip that faults at boot. The Digital Lab connects this way first; nothing is written and no mass erase is needed | The core halts at its first instruction and flash reads back |
| `Failed: Target is not halted` at the end of a DSLite flash, after `Program verification successful` | DSLite tries a step after the program already started running | Nothing: the image is written and verified | A pin read or 10 ms sampling of PA5 shows the new timing |
| `error -614` on every read (CCStudio) | `restart` was sent while the target was running; the debug port stops answering | Unplug and replug USB. Next time read the state, `pause` if it says Running, then `restart`, then `continue` | A read works |
| Usage fault, `CFSR 0x80000`, in `mathlib/fastrts` | The compiler moves TMU instructions above `Device_Init()` while the TMU is off | Define `int _system_pre_init(void)` that does `SCB->CPACR \|= (3UL << 0); __DSB(); __ISB(); return 1;`. A line at the top of `main()` does not help | CFSR and HFSR read 0 |
| `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here` (`west build -b lp_am13e230 samples/basic/blinky`) | Zephyr's board has no LED, GPIO or serial driver; the 4.4.2 release has no `lp_am13e230` board | Use `am13e230/zephyr/led-proof` or TI's SDK example | The LED toggles |
| `GCONV layer NOT offloaded to TI-NPU` (NNC log) | The layer runs on the CPU, so the 30k model gains only 2.4 times | Expected; do not chase it | Speed-up near 2.4 on the 30k model |
| Instruction bus error, `CFSR 0x100`, on the `type=m33cde` NNC build | NNC 2.1.2's code does not compile cleanly with TI Arm Clang 5.1.1; turning the error into a warning builds a program that faults | Use the NPU build or plain C | The model gives its 20 of 20 answers |

Traps shared with other boards: TRAPS.md.

## Traps

1. **A new board does nothing.** Flash is blank (all 512 kB read 0xFF, twice); only the power LEDs light and the UART is silent until you load a program (`am13e230/FAQ.md`).
2. **DSLite memory reads can leave the core halted.** A polling loop on a result word can end the run. Flash with a system reset, wait, then read once (`am13e230/tools/run_image.py`) (`am13e230/FAQ.md`).
3. **A connection file you write fails with -615.** Use `am13e230/AM13E23019_AL130001.ccxml` with your serial, or set the property `SWD.SWJ_DP_DEFAULT_MODE` to 0.
4. **TI's CoreMark example prints nothing.** Send `printf` to a RAM buffer (a 15-line `ee_printf` replacement, built with `-fno-builtin-printf` and 9k iterations) and read `cm_log:2048` (`am13e230/FAQ.md`).
5. **CoreMark drops about 30% from -O3 to -O2.** Expected: 872.4, 612.7, 477.1 and 438.1 per second at -O3, -O2, -Os, -Oz. Quote the flag (`am13e230/RESULTS-am13e230.md`).
6. **`west flash` needs OpenOCD built from source** (3.5 minutes; `brew install autoconf automake` first); Zephyr's SDK and xPack builds lack the flash driver (`am13e230/FAQ.md`).
7. **NNC has no Intel Mac build.** It was run on GitHub's Linux machines (`am13e230/RESULTS-am13e230.md`).
8. **The stepper:** the kit's 28BYJ-48 on its ULN2003 board runs from J4 pins 40, 38, 36 and 34, 5 V from J3 pin 21, ground from J3 pin 22. If it buzzes without turning, two signal wires are swapped (`am13e230/FAQ.md`).
9. **A deep-sleep image with no GPIO wake source can lock the debugger out.** An MSP430 port in STANDBY0 with only the watchdog interval timer as wake source never woke, and every connect then failed (DSLite -613, -6310, -6311). The SDK names the wake sources: a GPIO for STANDBY, the watchdog for STOP. Keep a button wake and a 5 s awake window at boot in any sleep image. DSLite's `-e` means verbose output, not erase (`ports/msp430-to-mspm0/README.md`, trap 3 and 8). Recovery steps: `docs/remote-board-recovery.md`.

## What fits

TI's SDK builds 100 of 100 LaunchPad examples; 80 were flashed and run for 4 s, 79 clean (no fault registers set), the failure being `mathlib/fastrts` (the usage fault in the table above). Clean does not mean the example did its job (`examples_built` and `examples_ran_clean` in `data/results/am13e230.csv`). All four room-occupancy models fit; the 30k model's plain-C code is 18.7 ms.

## What ran here

- CoreMark -O3: 872.4 per second at 200 MHz (4.36 per MHz), data sheet 870; TI's own files 871.6 (`am13e230/RESULTS-am13e230.md`).
- One FOC step: 117 cycles with the TMU (0.585 µs), 655.6 with the C library. sinf plus cosf: 11.6 cycles with the TMU, 512.7 without (`am13e230/RESULTS-am13e230.md`).
- Occupancy models plain C -O3: 0.489, 1.250, 5.055, 18.71 ms, 20 of 20 vectors. On the NPU 0.107, 0.205, 0.630, 9.535 ms (4.7 to 8.8 times faster than TI's CPU build, 2.4 on the largest) (`am13e230/RESULTS-am13e230.md`).
- UART loop clean up to 6.25 Mbaud. The DAC read back by the ADC was 4.4 to 9.4% high, cause unexplained (`am13e230/RESULTS-am13e230.md`).
- Stepper spun with sine microstepping (138 cycles per update with the TMU), three MSPM0 programs through a 13-line shim, and a radar parser on 62 of 62 replayed frames (`am13e230/README.md`, `docs/experiments/2026-10-09-am13e230-stepper-uln2003.md`, `docs/experiments/2026-10-09-am13e230-mspm0-driverlib.md`, `docs/experiments/2026-10-09-am13e230-radar-host-parser.md`).
- Fresh AI agents on blink and CoreMark: 3 of 3 passed with the repo's notes, 2 of 3 without (`am13e230/README.md`).
- Trigger to new PWM value in the current loop: 1.33 µs with the TMU, 4.74 µs in plain software (`control_loop` rows).
- Not run: power at J10, energy per inference, ADC maximum rate, interrupt latency and GPIO toggle time.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| A radar frame parser on the LP-AM13E230 and SK-TDA4VM | Works | A radar frame parser in 882 bytes of C gives the same output on the LP-AM13E230 and SK-TDA4VM as the Mac parser. | `docs/experiments/2026-10-09-am13e230-radar-host-parser.md` |
| C2000 current-loop code on the LP-AM13E230 | Works | The same current-loop step, trigger to new PWM, runs about twice as fast on the LP-AM13E230 as on the LAUNCHXL-F2800137. | `docs/experiments/2026-10-11-current-loop-three-mcus.md` |
| h264bsd video decoder on TI microcontrollers | Works | h264bsd, an H.264 baseline decoder, runs unmodified on the LP-MSPM33C321A and LP-AM13E230 with every frame checksum equal to the Mac's. | `portability/h264/SOURCE.md` |
| MSPM0 DriverLib programs on the LP-AM13E230 | Works | MSPM0 DriverLib GPIO, UART and timer PWM programs run on the LP-AM13E230 with their app code unchanged and a 13-line shim header. | `docs/experiments/2026-10-09-am13e230-mspm0-driverlib.md` |
| ST STM32G4 HAL examples on the LP-AM13E230 | Works | ST's STM32G4 HAL GPIO and UART examples run on the LP-AM13E230 through TI's HAL layer, with 3 and 7 lines added to ST's main.c. | `docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| TI AM13 Hall FOC adapted to DRV8376EVM | Works | TI's Hall FOC firmware drives a motor closed loop through a DRV8376EVM, unloaded at 12.4 V, on the LP-AM13E230. | `am13e230/foc-drv8376/COMMISSIONING.md` |
| Zephyr on the LP-AM13E230 | Works | Zephyr's hello_world reaches main and an image toggles the LED on the LP-AM13E230. | `docs/experiments/2026-10-09-am13e230-zephyr-upstream.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
<!-- ports:end -->

## Where the evidence lives

`am13e230/README.md`, `am13e230/FAQ.md`, `am13e230/RESULTS-am13e230.md` and `data/results/am13e230.csv`. If you find something this file gets wrong on your board, the FAQ is where it should be corrected first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Deep-sleep lockout: an image in STANDBY0 with only the watchdog interval timer as wake source never wakes; every connect fails (DSLite -613, -6310, -6311; OpenOCD -613; all access ports stalled behind DPIDR 0x6ba02477) (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21)
- Boot fault after an interrupted write or an erased vector table (CFG-AP BOOTDIAG 0x08000007) (`am13e230/AGENTS.md`, 2026-10-22)
- restart sent while the target was running in CCStudio: error -614 on every read (`am13e230/AGENTS.md`, `am13e230/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset toggle and assert (deep-sleep lockout: the target never printed again) | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, 2026-10-21 |
| uhubctl power-off of the probe port, 2 s, 20 s and 150 s (on two hubs; 15 s on the second) (the probe came back, the chip stayed stalled; this board most likely keeps power) | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21 |
| Connect under reset, and a hand-written OpenOCD connection, within 3 s of the probe appearing; 80 fast connects in a row | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21 |
| Unplug USB and replug (the probe enumerated, the chip still did not answer) | hands | failed | `docs/remote-board-recovery.md`, 2026-10-21 |
| Hold S5 (BSL invoke), tap S1 (nRST), release S5; then flash the restore hex and read the first 64 bytes back (for the deep-sleep lockout) | hands | proven | 2026-10-24 |
| Wait-for-debug (SPREC/DPREC0 sequence at AP 4 from CCStudio's own log) (for the boot-fault lockup only; it needs the CS_DAP, which the deep-sleep lockout does not give (Error -613)) | remote | proven | `am13e230/AGENTS.md`, `mspm33c321a/AGENTS.md`, 2026-10-22 |
| Unplug and replug USB after a restart sent while running (error -614 case) | hands | proven | `am13e230/FAQ.md`, `am13e230/AGENTS.md`, date not recorded |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-09. A 5 s cycle, then 15 s off, left the AM13 probe on the bus without enabling it (Error -260); the cable had to be pulled.
- The probe's hub port does not cut power, 2026-10-21. A 15 s power-off of the probe port: the hub reported the port off and the neighbor kept running, but the chip stayed stalled, so the board most likely kept power; the L1306 on its own port did restart.

Restore: DSLite flash of the restore hex; main flash backed up twice before a risky image (main-a.bin, main-b.bin identical). Regions: main flash. (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`)

<!-- recovery:end -->
