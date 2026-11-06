# Port drivers and code to, and across, the MSPM0 family (LP-MSPM0C1104, LP-MSPM0L1306, LP-MSPM0G3507, LP-MSPM0G3519, LP-MSPM0G5187)

Carry the example's SysConfig file with the code and change the device: DriverLib code then moves up the family easily and down it badly. 101 of 170 LP-MSPM0L1306 examples build for the G3507 with only the device changed, 22 of 270 G3507 examples build for the L1306, and all 133 ported images that built ran on the L1306 with no crash (matrix X15 to X17). One application file ran unchanged on the L1306, C1104, G3519 and G5187 with one 39-line DriverLib glue file (X53).

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **DriverLib** is TI's register-level C library for MSPM0; **SysConfig** is TI's tool that writes a chip's pin and clock setup code from a `.syscfg` file; **TI Drivers** is TI's portable driver API (`GPIO_setConfig`, `UART_open`); a **twin** is the same example by name in two SDKs; **glue** is the code that connects an app to one software layer; **UNICOMM** is the combined serial block on newer parts, where UART0 is named `UC0`. 1 kB = 1,024 bytes. **X15** and the other X numbers are row ids in `portability/matrix.csv`. Every port, test and cell on record for these boards, generated: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the parts, the part-to-part map, a ten-minute path, the one-command build, the driver and function map, how to port into the family, how to move a port across it, moving to and from other families, what was measured, errors, sources.

## The board (five parts held)

| Part | Kit | Core and clock | Flash | RAM | In the lab | Source |
|---|---|---|---|---|---|---|
| MSPM0C1104 | LP-MSPM0C1104 | Cortex-M0+, 24 MHz, no PLL | 16 kB | 1 kB | yes | `mspm0c1104/AGENTS.md`, `data/boards.csv` |
| MSPM0L1306 | LP-MSPM0L1306 | Cortex-M0+, 32 MHz | 64 kB | 4 kB | yes | `mspm0l1306/AGENTS.md` |
| MSPM0G3507 | LP-MSPM0G3507 | Cortex-M0+, 80 MHz | 128 kB | 32 kB | results only | `data/boards.csv`, `portability/shim/README.md` |
| MSPM0G3519 | LP-MSPM0G3519 | Cortex-M0+, 80 MHz | 512 kB, dual bank | 128 kB | yes | `mspm0g3519/AGENTS.md` |
| MSPM0G5187 | LP-MSPM0G5187 | Cortex-M0+ with TinyEngine NPU, 80 MHz | 128 kB | 32 kB | yes | `mspm0g5187/AGENTS.md` |

Board folders: [`mspm0c1104/`](../../../mspm0c1104/AGENTS.md), [`mspm0l1306/`](../../../mspm0l1306/AGENTS.md), [`mspm0g3507/`](../../../mspm0g3507/AGENTS.md) (the G3507), [`mspm0g3519/`](../../../mspm0g3519/AGENTS.md), [`mspm0g5187/`](../../../mspm0g5187/AGENTS.md). Each has a `README.md`, `AGENTS.md` and `FAQ.md` for its own bring-up.

### Part-to-part map

What each part has, and what differs in a way that stops a port. A dash means nothing is on record.

| | C1104 | L1306 | G3507 | G3519 | G5187 |
|---|---|---|---|---|---|
| Not on the chip | FPU, DAC, math accelerator, AES; CRC-16 only, no CRC-32 | FPU, divide instruction, math accelerator, AES, random-number generator, general-purpose DAC | FPU | FPU | MCAN, DAC12 |
| On the chip, beyond the core set | - | hardware CRC; comparator with an 8-bit reference DAC | math accelerator, AES, CRC, DAC | two 4 Msps ADCs, 12-bit DAC, math accelerator, AES with key storage, random-number generator, two CAN-FD | NPU, PCM1809 audio ADC on the kit |
| Missing against the G-series, as a port sees it | timer TIMG14 and the ADC offset calibration call exist only here (X3) | PB pins, SPI1, ADC1, math accelerator, AES, DAC12, RTC (X15) | - | - | MCAN (6 examples), DAC12 (4 examples) |
| Interrupt lines | - | only 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25, 31 | - | - | - |
| Interrupt latency, bare metal | - | 25 to 28 cycles | - | - | 36 to 40 cycles, 7 more than the G3507's best |
| SysTick | does nothing (registers read `0 0 0 0`); time with TIMG8 | works | works | works | works |
| DMA | one channel, no UART trigger | - | UART DMA built into the TI Drivers library | - | SysConfig may move a retargeted channel from 0 to 1 |
| Serial block name | UART0 | UART0 | UART0 | UART0 | `UC0` (UNICOMM); a handler with the old name builds and never runs |
| RAM start | - | - | - | 0x20200000 | 0x20200000 |
| Package on the kit | - | 32 pins | - | LQFP-100 (PZ) | - |
| Boot rescue | no ROM bootloader | ROM bootloader on S1 (PA18) | - | - | - |
| Pin-function table | - | - | Zephyr's shared MSPM0G1x0x/G3x0x pin file | 136 of 443 peripheral functions in that shared file differ from SysConfig's `MSPM0G351X.json` | Zephyr main has no board or SoC |

Sources: `mspm0c1104/AGENTS.md`, `mspm0l1306/AGENTS.md`, `mspm0l1306/FAQ.md` (interrupt lines), `mspm0g3507/AGENTS.md`, `mspm0g3519/AGENTS.md`, `mspm0g5187/AGENTS.md`, `docs/guides/chip-capability-map.md` (`UC0`), `portability/everywhere/PORTING.md` sections 1 and 7.2 (MCAN, DAC12, LQFP-100), `portability/three-ways/README.md` (RAM start), `portability/arduino/zephyr/BOARD-mspm0g3519.md` (pin functions audit), `portability/zephyr-mspm0/README.md` (G5187 Zephyr), matrix rows X3 and X15, the C1104 DMA note in `site/data/ports.json` (port `stm32-usart-dma-rx-tx`), the TI Drivers DMA note in `portability/three-ways/README.md` problem 8.

## Path: move one example from the L1306 to the G3519

The steps are recorded pieces put in order, not one timed run.

1. Terminal, no board. Copy the L1306 example `gpio_toggle_output`, change the device, package and board lines in its `.syscfg` to the G3519's, drop any SysConfig line the new device rejects, and build it with the example's makefile. Write down every dropped line and every pin SysConfig moved. The result is one of three: built, built with changed behavior, or failed with a first error.
2. Terminal. Read the generated `ti_msp_dl_config.h` in the kept build: SysConfig picks the pins when assignments are unlocked, so the LED pin may not be the board's LED (`portability/everywhere/PORTING.md` section 7.1).
3. Terminal, board plugged in. Back up before the first write: `python3 tools/benchmark/backup.py save mspm0g3519 --probe <serial>` (the tested recipe in `mspm0l1306/AGENTS.md`; NONMAIN is read, never written back).
4. Terminal. Check the image: `python3 tools/preflight_image.py mspm0g3519 <image.out>` must print SAFE.
5. Terminal. Flash and start it as the board's `AGENTS.md` says. On the G3519, pulse nRST after a DSLite flash or the image stops in the SRAM-error NMI handler (`portability/arduino/zephyr/FAQ.md`).
6. Terminal. Read the LED pin's GPIO output register (DOUT) a few times through the XDS110 probe with OpenOCD; the bit changes when the pin toggles. Then restore the backup.

## One-command build

```bash
portability/three-ways/build_all.sh
```

It builds the same 81-line application for the G3507, L1306 and G5187 with DriverLib, plus the MSPM33 and AM13 DriverLib builds, TI Drivers for the G3507 and L1306, and Zephyr for the G3507 and L1306, then counts lines. About 3 minutes; it touches no board. The G3519 and C1104 builds are `portability/three-ways/driverlib/build.sh g3519` and `portability/three-ways/driverlib/build_c1104.sh`. It does not flash or check anything (`portability/three-ways/README.md`).

## Driver and function map

Rows are peripherals and services, columns are the layers this repo has run on an MSPM0 part. Each cell is the status word, the part it ran on and the file behind it. A dash means nothing is on record. Zephyr cells name the release: 4.4.2 for the L1306 board in this repo, main (4.5.0 development) for the G3519.

| Function | DriverLib | TI Drivers twins | Zephyr (upstream, with open pull requests) | Arduino layer (G3507) | Arduino on Zephyr (G3519) | MicroPython (G3519) | FSP layer (G3519) | Pico layer (G3519) |
|---|---|---|---|---|---|---|---|---|
| GPIO | Works, all five (X53, `mspm0l1306/AGENTS.md`) | Works, G3519: LED blinks ([shim](../../shim/README.md)) | Works, L1306 4.4.2 ([board](../../../mspm0l1306/zephyr/README.md)); G3519 main with an nRST pulse | Works, G3507 ([layer](../../arduino/README.md)) | Works, Blink ([results](../../arduino/zephyr/README.md)) | `Pin` toggle runs; LED not checked ([port](../../../ports/micropython/README.md)) | - (`R_IOPORT_Open` only, [FSP](../../../ports/renesas-ra-fsp/README.md)) | Works, `blink_simple` ([Pico](../../../ports/pico-sdk/README.md)) |
| UART | Works, all five (X53; L1306 clean to 4.5 Mbaud, `mspm0l1306/AGENTS.md`) | Works, G3519: `uart2echo`, `uart2callback` | Works, console on both | Works, `Serial` in `shimcheck` | Works, `Serial` at 9600 | Works, REPL at 115200 | Works, `sci_uart` | Works, `hello_world/serial` |
| I2C | Works, G5187: HDC2080 and OPT3001 ([chip map](../../../docs/guides/chip-capability-map.md)); G3519 block got no answer from the touch chip | G3519: opens; no sensor fitted | Works, G3519 main with pull request #117311: loopback 12 of 12, touch chip read ([zephyr-mspm0](../../zephyr-mspm0/README.md)) | Works, software `Wire` reads the touch chip | Works, `Wire` 100 of 100 reads | Works as `SoftI2C`; hardware `machine.I2C` not written | Works, `iic_master` | Works, `bus_scan` |
| SPI | Works, C1104 Sharp panel at 1.000 MHz, G3519 panel at 40 MHz ([chip map](../../../docs/guides/chip-capability-map.md)) | G3519: starts and waits for a second board | Works, G3519 main: loopback through the Arduino core | Works, Arduino_GFX drew on the screen | Works, loopback 256 of 256 bytes | Works, `machine.SPI` at 20 MHz | not in the layer | - |
| ADC | Works, L1306, C1104, G3519, G5187 (X53) | G3519: prints the start line only, as TI's own example does | Works on main, G3519 (`ti,mspm0-adc12`); 4.4.2 has no MSPM0 ADC driver ([three-ways](../../three-ways/README.md)) | not in the layer (`analogRead` not written) | Works, path only (floating pin) | not written | Works, single scan | Works, path only |
| PWM and timers | Works, G3519: MSPM33 timer and PWM examples ran (X52) | no twin example | Works, G3519 main: TIMA1 on PB27 with pin function 9 | not in the layer (`analogWrite` not written) | Works, `analogWrite` duty read on the pin | not written | Works, `gpt` periodic, PWM, one-shot | Works, `led_fade` |
| DMA | Works: UART DMA receive port ([STM32 port](../../../docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md)); the G5187 retargeted DMA examples wait on the wrong channel (X30) | - | - | not in the layer | - | not written | Could work: two-group DMA chain incomplete | - |
| Interrupts | Works, L1306 25 to 28 cycles, G5187 36 to 40 cycles | G3519: button path not observed | Works, L1306 4.4.2: entry 119 to 137 cycles; G3519 button interrupt | not in the layer (`attachInterrupt` not written) | Works, `attachInterrupt` | - | Works, I2C and UART handlers, timer callbacks | Works, PWM fade from the timer interrupt |
| Flash write | Works, L1306: word write 56 µs, sector erase 1.99 ms ([MSP430 port](../../../ports/msp430-to-mspm0/README.md)) | - | Works, G3519 main with this repo's driver: 18 of 18 checks | - | - | - | - | - |
| Clocks | Works; read `CPUCLK_FREQ` after any change (L1306); TI's G5187 PLL example hangs, its own 80 MHz PLL setup works (X43) | - | Works, L1306 at 32 MHz | Works, 32 MHz | - | Works, 80 MHz after up to 20 PLL lock retries | - | - |
| Low power | Works, L1306 STANDBY0 (CPU clock stops); C1104: 15 low-power examples ran without a crash | - | - | - | - | - | - | - |
| RTOS | Works: FreeRTOS on the L1306, a one-task blink in 0.98 kB with static allocation ([FreeRTOS](../../../mspm0l1306/freertos/README.md)) | Works on FreeRTOS, G3519 | Works, L1306 hello_world in 3.7 kB | bare metal | Works on Zephyr | bare metal | bare metal | bare metal |

Sources, by column: board AGENTS.md files and matrix rows X30, X43, X52, X53; `portability/shim/README.md` (2026-10-25 table); `portability/zephyr-mspm0/README.md`, `mspm0l1306/zephyr/README.md`, `portability/arduino/zephyr/BOARD-mspm0g3519.md`; `portability/arduino/README.md` ("What the layer covers"); `portability/arduino/zephyr/README.md`; `ports/micropython/README.md` ("What is in the port"); `ports/renesas-ra-fsp/README.md` and `portability/fsp-on-mspm0/README.md`; `ports/pico-sdk/README.md` and `portability/pico-on-mspm0/README.md`.

## How to port into this family

1. **Pick the layer by RAM first.** RAM decided most outcomes here (`docs/guides/chip-capability-map.md`).

   | RAM | Parts | Layers that ran or fit | Source |
   |---|---|---|---|
   | 1 kB | C1104 | DriverLib bare metal: the shared app in 184 B, a UART packet port in 632 B, a Sharp panel in 609 B | X53, `site/data/ports.json`, `docs/guides/chip-capability-map.md` |
   | 4 kB | L1306 | DriverLib, FreeRTOS with static allocation, Zephyr with trimmed samples, Rust; TI Drivers on FreeRTOS does not link with TI's settings file | `mspm0l1306/AGENTS.md`, `portability/three-ways/README.md` |
   | 32 kB | G3507, G5187 | TI Drivers on FreeRTOS (`7,059` B of RAM on the G3507), the MicroPython minimal build (72.9 kB of flash on the G3507, built only) | `portability/three-ways/README.md`, `ports/micropython/README.md` |
   | 128 kB | G3519 | Every layer in the map above, including the full MicroPython build (175.6 kB of flash) | `ports/micropython/README.md` |

2. **Pick the layer by peripheral next.** Use the map above: a cell that says Works names the file to copy. For a layer with a "Take it with you" section, copy the files it names ([`portability/README.md`](../../README.md), "Take a port into your own project").
3. **Put board changes in a board folder, never in shared code.** `portability/<layer>/boards/<board>/` holds the pin map and SysConfig file (`docs/repo-layout.md`); examples are `portability/fsp-on-mspm0/boards/mspm0g3519/`, `portability/arduino/boards/mspm0g3507/` and `portability/arduino/zephyr/variants/lp_mspm0g3519/`. For the shared bench tests, add the board's row to [`portability/boards/boards.csv`](../../boards/boards.csv) and run `python3 portability/boards/gen_board_h.py --check`.
4. **Check every pin function against SysConfig's device data**, not against a shared pin file. On the G351x, Zephyr's `tima1_ccp1_pb27` uses function 6, which is COMP2.OUT, so the timer ran and the pin stayed low; TIMA1.CCP1 is function 9 (`portability/arduino/zephyr/BOARD-mspm0g3519.md`). LEDs differ by kit: PA0 and PA26 on the L1306, PA22 on the C1104 (PA4 on board version E2), PB22 on the G3507, PB27 green on the G3519, PA17 on the G5187 (the board AGENTS.md files).
5. **Check interrupt lines.** On the L1306, pending an unimplemented line does nothing and neither CMSIS nor Zephyr's `IRQ_CONNECT` complains (`mspm0l1306/FAQ.md`).
6. **Run the checks before the first flash.** `python3 tools/preflight_image.py <board> <image>` and the board's recovery section in its `AGENTS.md`. Then `python3 tools/results/check.py` after recording rows.
7. **Back up, flash, restore.** `python3 tools/benchmark/backup.py save <board> --probe <serial>`, and `restore` at the end. The C1104 has no ROM bootloader: a ported image that faults before main can lock it, and recovery is TI's DSSM factory reset (mailbox command 0x020A; it erases main flash and resets NONMAIN to defaults, so ask before running it) (`mspm0c1104/FAQ.md`).

## How to move a port across the family

Measured costs, from `portability/matrix.csv`. Up means to a part with more memory and peripherals.

| Move | What changes | Measured | Row |
|---|---|---|---|
| L1306 to G3507 (up) | Device, package and board lines; L-only timer names (TIMG1, TIMG2, TIMG4) and pin names like PA17/OPA1.IN0- fail | 170 tried: 101 unchanged, 132 with glue, 135 after edits; 3 lost their ADC reference or channel silently; built, not run | X17 |
| C1104 to G3507 (up) | Pins unlocked; C-only TIMG14 and the ADC offset calibration call fail | 91 tried: 44 unchanged, 65 with glue | X3 |
| G3507 to L1306 (down) | `SYSCTL.forceDefaultClkConfig` stops 136 retargets, then missing peripherals and pins | 270 tried: 22 unchanged, 26 with glue, 96 after edits; all 82 safe images ran, none crashed | X15 |
| C1104 to L1306 (sideways) | The ported image keeps the C1104 kit's pins: PA22 is the light-sensor amplifier on the L1306 | 91 tried: 40 unchanged, 57 with glue; all 51 safe images ran, none crashed | X16 |
| L1306 to C1104 (down) | Device lines and RAM | 170 tried: 76 build after edits; not run | X18 |
| G3507 to C1104 (down) | Device lines, RAM, missing blocks | 270 tried: 13 unchanged, 16 with glue, 48 after edits; 26 of 48 ran, 1 (`tima_timer_mode_pwm_dead_band`) locked the debugger out | X4 |
| G3519 to G5187 (sideways) | Delete the G3519's copied `ti_msp_dl_config` files; silicon instance names (UART 16, SPI 11, I2C 5, TRNG 3, timers 6 of the first errors) | 79 of 122 DriverLib examples build; 56 ran, 15 match TI | X30, `portability/everywhere/PORTING.md` section 2 |
| G3519 to G5187, plain C | Environment switches only; 32 kB in one bank drops ML-DSA-44 | ML-KEM-768 runs at -Os | X43 |
| One app, every MSPM0 kit here | One SysConfig file per board; the glue file is shared byte for byte | 39 glue lines, 30 to 34 config lines, `2,320` to `2,456` B of flash; serial check passed on silicon | X53 |
| Three ways, G3507 to L1306 | DriverLib 11 SysConfig lines changed or added and 2 dropped; TI Drivers 14 C lines changed and 6 dropped; Zephyr 1 devicetree and 1 Kconfig line | the app and glue file built unchanged | X22 to X24 |

Most of these rows came from a script that rewrites the device lines in each example's `.syscfg`, builds it, and lists every dropped SysConfig line and every moved pin. Dropped settings change behavior without an error, so list them yourself when you port by hand. The most-dropped line is `SYSCTL.forceDefaultClkConfig` (harmless). Whole OPA instances (channel, pin, bandwidth, gain) and ADC references also vanish silently on the way to the C1104, G3519 and G5187.

## Moving to and from the other families

| To or from | Route | Measured | Rows and files |
|---|---|---|---|
| MSPM33 | Carry the `.syscfg`, swap device lines, unlock pins; `portability/everywhere/crossport_mspm0_mspm33.py` | 43 of 83 build with the app untouched, 40 ran, 39 clean; GPIO, SysTick, timers, PWM, SPI, UART and CRC run both ways; I2C (UNICOMM) and ADC (HSADC) need a rewrite | X28, X29, X51, X52, [MSPM33 guide](../mspm33/README.md) |
| AM13 | The AM13 DriverLib redirects MSPM0 names; `portability/everywhere/crossport_mspm0_am13.py` writes the alias header | 3 programs ran with 53 app lines unchanged; 20 of 43 SysConfig ports build, 14 ran clean | X8, X31, [AM13 guide](../am13/README.md) |
| CC23xx and CC27xx (F3 SDK) | TI Drivers twins through the shim headers in `portability/shim/` | 10 of 10 build each way; 5 MSPM0 apps ran on the CC2340R5; 3 F3 apps show behavior on the G3519 | X1, X2, `portability/mspm0-to-wireless.md` |
| CC1354P10 (F2 SDK) | Same twins | 8 tried: 4 with glue | X13 |
| C2000, AM263P, TDA4VM | Plain C only | The q15 FFT example and the BitNetMCU inference code moved as C | X32, X33, X48, [EVIDENCE.md](EVIDENCE.md) |
| Other vendors | Glue layers that implement the other vendor's calls on DriverLib | Cmsemicon CMS32 (34 built and ran, 29 unchanged), Renesas FSP (4 of 5 run), Pico SDK (5 of 5 run), MSP430FR2433 (3 examples ran on the L1306) | X25, X54, X37, `ports/pico-sdk/README.md` |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| L1306 examples built for the G3507, device changed only | 101 of 170 | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1 | measured | `portability/matrix.csv` X17 |
| G3507 examples built for the L1306 | 22 of 270 unchanged, 96 after edits | same | measured | X15 |
| Ported images run on the L1306 | 133 built and ran, none crashed | from the G3507 and C1104 | measured | X15, X16 |
| G3519 examples on the G5187 | 79 of 122 build, 56 ran, 15 match TI | DriverLib | measured | X30 |
| One app on four MSPM0 kits | serial check passed on each; 39 glue lines | DriverLib, bare metal | measured | X53, `portability/three-ways/README.md` |
| TI Drivers twins, F3 apps on the G3519 | 10 of 10 build; 3 pass, 1 toolchain, 6 no-hardware | FreeRTOS | measured | X2, `portability/shim/README.md` |
| Zephyr I2C on the G3519, loopback | 12 of 12 checks | main with pull request #117311, 100 kHz | measured | `portability/zephyr-mspm0/README.md` |
| Zephyr flash driver on the G3519 | 18 of 18 checks | this repo's driver | measured | `portability/zephyr-mspm0/README.md` |
| Pico SDK examples on the G3519 | 5 of 5 run, 0 app edits | 316-line layer | measured | `ports/pico-sdk/README.md` |
| Renesas FSP examples on the G3519 | 4 of 5 run | layer in `portability/fsp-on-mspm0/` | measured | `ports/renesas-ra-fsp/README.md`, X54 |

## If you see this

- If you see `use of undeclared identifier 'UART0'` (or `SPI1`, `TIMA0`, `DMA`) after a retarget, the app names the silicon instance, not SysConfig's alias. Rename it to the alias (`UART_0_INST`) in the app or in a force-included alias header (`portability/everywhere/PORTING.md` section 2).
- If an interrupt handler runs on the G3507 but never on the L1306, the line has no peripheral on the L1306. Use one of lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25 or 31 (`mspm0l1306/FAQ.md`).
- If you see `Could not find MEM-AP` after running a ported example on the C1104, the chip is in a boot loop. Confirm by reading the boot diagnostic register (CFG-AP BOOTDIAG): it cycles through values such as 0x07. Then ask before running the factory reset (`mspm0c1104/FAQ.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3 and 1.28.0, Zephyr 4.4.2 and main at `1405adc9a`, on an Intel Mac with macOS 15.7.9; runs dated 2026-10-06 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/mspm0c1104.csv`, `data/results/mspm0l1306.csv`, `data/results/mspm0g3507.csv`, `data/results/mspm0g3519.csv`, `data/results/mspm0g5187.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/mspm0/EVIDENCE.md`. Checked 2026-10-25.
