# AI assistant notes for the TI LP-MSPM0L1306 LaunchPad development kit

The 4 kB of RAM runs out long before anything else, and that a few silent traps (stack size, Zephyr's heap, missing interrupt lines, one ignored SysConfig line) cost hours if nobody warns it. This file is the short list, each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0L1306 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C library for the chip's parts; **NONMAIN** is the flash area that holds the chip's boot settings; **CoreMark** is a standard CPU speed test; **MessagePack** is a compact binary format for structured data; **MEM-AP** is the debug port the debugger uses to reach the CPU; **static allocation** means RAM is reserved when the program is built, not requested while it runs. 1 kB = 1,024 bytes. Checked 2026-10-06 on an Intel Mac, macOS 15.7.9, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0+dev and Zephyr 4.4.2, on one LP-MSPM0L1306.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm0l1306/` or `data/` are in the Fox Red Labs repo; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0L1306: Arm Cortex-M0+, 32 MHz from the internal oscillator, one flash wait state above 24 MHz (`mspm0l1306/README.md`) |
| Memory | 64 kB flash, 4 kB RAM. Flash was never the limit; RAM always was (`mspm0l1306/README.md`) |
| Not on the chip | No FPU, no divide instruction, no math accelerator, no AES, no random-number generator, no radio, no general-purpose DAC (only the comparator's 8-bit reference DAC). Hardware CRC is there (`data/results/mspm0l1306.csv`) |
| Interrupt lines | Only 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25 and 31 exist (`mspm0l1306/FAQ.md`) |
| LEDs | LED1 red on PA0 (on when the pin is low); LED2 color on PA26 red, PA13 green, PA27 blue (`mspm0l1306/zephyr/README.md`) |
| Buttons | S1 on PA18 (held at reset, starts TI's serial bootloader), S2 on PA14, S3 reset (`mspm0l1306/FAQ.md`) |
| Sensors | Light sensor (photodiode into op-amp 0, PA22, PA24, PA25); thermistor on PA15 (`data/results/mspm0l1306.csv`) |
| Serial | UART0 on PA8 (TX) and PA9 (RX). Jumpers J16 and J17 on XDS (right, as shipped) send it to the USB serial port; on BP (left) to the header pins (`mspm0l1306/FAQ.md`) |
| Debugger | XDS110 on the board, USB, two serial ports, no driver needed on a Mac (`mspm0l1306/README.md`) |

A labeled picture of every part: `mspm0l1306/img/board-tour.png`.

## Start here

| Step | This board |
|---|---|
| Whole route | `./start.sh mspm0l1306 blink --probe <serial>` does the rows below in order with no assistant (`docs/frl.md`); stops with FRL-E002 if several probes are plugged in and none is named |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS (from CCStudio 21.0.1), SysConfig 1.26.3, xPack OpenOCD 0.12.0-7 (the first with `ti_mspm0.cfg`; `tools/frl` runs later versions only when named, as unvalidated), UniFlash 9.6.0 (`mspm0l1306/README.md`) |
| Probe serial | `python3 tools/frl/frl.py detect`, or `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | `python3 tools/benchmark/backup.py save mspm0l1306 --probe <serial>` (tested recipe: main flash and NONMAIN read twice and compared; NONMAIN is never written back) |
| Build TI's blink | `cp -R <SDK>/examples/nortos/LP_MSPM0L1306/driverlib/gpio_toggle_output .` then `make -C gpio_toggle_output/ticlang MSPM0_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS GCC_ARMCOMPILER=` (3.3 s) |
| Flash | `openocd -f interface/xds110.cfg -c "adapter serial <serial>" -c "adapter speed 2500" -f target/ti_mspm0.cfg -c init -c "reset halt" -c "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" -c "reset run" -c shutdown` (0.6 s) |
| Check | `python3 tools/check/blink.py --board mspm0l1306 --probe <serial>` |
| Restore | `python3 tools/benchmark/backup.py restore <backup folder> --probe <serial>` |

The pattern is the delay in `gpio_toggle_output.c`. TI's example drives the red channel of LED2 (PA26); LED1 on PA0 is not an output in it, so a person should watch LED2. A pin read through the probe on 2026-10-13 showed PA26 toggling and PA0 not enabled.

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `wrong # args: should be "error message ?stacktrace?"` after `mspm0_mass_erase` | The script's reset helper throws after the erase has happened | Ignore it and load an image with DSLite or OpenOCD | The next load works |
| `DAP Connection Error. This could be caused by the device having gone to low power mode.` | The image enters SHUTDOWN right after boot, so no debugger can attach | Hold S1 (PA18), press and release S3, keep S1 held 2 more seconds, then flash a non-sleeping image at once. Unplug and replug USB afterwards | OpenOCD attaches within a second |
| `error: 'IOMUX_PINCM38' undeclared here (not in a function)` | Zephyr 4.4.2's GPIO driver has the 80-pin MSPM0L222x pin table | Build with the out-of-tree board in `mspm0l1306/zephyr/` (`-DBOARD_ROOT`) | Blinky builds |
| ``region `RAM' overflowed`` (Zephyr sample) | Sample defaults need 6.1 to 17 kB of RAM | Cut stacks and buffers as in `mspm0l1306/zephyr/extra/README.md` | Image links |
| `run placement with alignment fails for section ".bss" size 0x1458` | TI's `FreeRTOSConfig.h` for this board asks for a 3 kB heap | Use static allocation and small task stacks (`mspm0l1306/freertos/README.md`) | Image links |
| `symbol "interruptVectors" redefined` (the three `bsl_software_invoke_app_demo_*` examples) | The examples clash at link | Skip them. 14 of 170 SDK examples do not build from a terminal | The other 156 build |
| Serial shows the boot text twice | Each new OpenOCD connection resets and restarts the program | Read results in the OpenOCD connection that started it, or use the serial port | One boot line |
| Sleep current stays near 2.6 mA in any power mode, or a SysTick count does not stop in STANDBY0 | A debugger attached since the last power-on reset keeps the chip in SLEEP0; OpenOCD leaves the debug power request set | Unplug and replug USB, or clear the request in OpenOCD with `mspm0x.dap dpreg 0x04 0x00000000` (this resets the chip once more), then measure | Current drops; the CPU clock stops in STANDBY0 (33.8 thousand ticks a second against 15.2 million in SLEEP) |
| Serial port silent on the shipped demo | The out-of-box demo sends nothing until asked | Send `81 AB 6C 69 67 68 74 45 6E 61 62 6C 65 C3 0A` at 115200 baud | Light readings stream |
| Wire from PA8 to PA9 returns nothing | J16 and J17 on XDS route the UART to the USB port | Move both to BP (left), wire J1 pin 4 to J1 pin 3 | Bytes come back |
| Ported example blinks at -O0 but not at -O2 | A global polled without `volatile` | Add `volatile` | LED blinks at -O2 |

Traps shared with other boards: TRAPS.md.

## Rules for the assistant

1. **After any clock change in SysConfig, read `CPUCLK_FREQ`** in the generated `ti_msp_dl_config.h`. With the clock tree on, `system.clockTree["SYSOSC"].frequencySelect` wins and `SYSCTL.SYSOSC_Freq` is ignored without a warning (`mspm0l1306/FAQ.md`).
2. **For sensors, keep the ADC sample window long.** TI's 8-cycle window reads the thermistor 1.7% low against a 64-cycle reading; 2 cycles reaches 1.99 million samples a second but reads 9.9% low (`mspm0l1306/FAQ.md`).
3. **Do not trust the internal temperature sensor.** It read about -165 °C with this chip's factory calibration. Use the thermistor on PA15 (`mspm0l1306/FAQ.md`).
4. **UART:** every byte was right up to 4.5 Mbaud and from 6.4 to 10.67 Mbaud; 5 to 6 Mbaud fails. At 12 Mbaud `DL_UART_configBaudRate` sets a divisor of 0 and nothing is sent, with no error (`mspm0l1306/FAQ.md`).
5. **FreeRTOS:** TI's settings file for this board needs 5.4 kB of RAM. Use static allocation and size each task's stack from stack analysis (44 words worst case for a task that sleeps; 48 words, 192 bytes, ran with 11 tasks; TI's setting is 128): a one-task blink then uses 0.98 kB (`mspm0l1306/freertos/README.md`).

## What fits in 4 kB of RAM

| Program | RAM | Fits? |
|---|--:|---|
| TI blink example | 256 B | yes |
| CoreMark, standard 2 kB data, bare metal | 3.9 kB | yes, 144 B spare |
| 8k-parameter room-occupancy neural net | 2.2 kB plus 548 B stack | yes, 84 to 100 ms per answer |
| 30k-parameter neural net | over 4 kB | no |
| FreeRTOS one-task blink, static | 0.98 kB | yes |
| Zephyr hello_world | 3.7 kB | yes, 264 B spare |
| Zephyr shell, logger, synchronization, as shipped | 6.1 to 17.0 kB | no; trimmed versions fit (the shell has 48 B left) |

Sources: `mspm0l1306/README.md`, `mspm0l1306/zephyr/README.md`, `mspm0l1306/freertos/README.md`.

## Measured speeds, for sanity checks

CoreMark 78.27 per second at 32 MHz with TI Arm Clang -O3 (2.446 per MHz). CoreMark's own printout says `Iterations/Sec : 83` here, because float printing is off to save RAM; compute the score from the tick count instead. Under Zephyr with GCC: 59.70. Interrupt entry: 25 to 28 cycles bare metal, 119 to 137 under Zephyr. A 256-point q15 FFT with TI's CMSIS-DSP: 2.25 ms (`mspm0l1306/README.md`, `mspm0l1306/FAQ.md`, `mspm0l1306/zephyr/README.md`, `data/results/mspm0l1306.csv`).

## Next

Start at `mspm0l1306/README.md` for the full bring-up, and `mspm0l1306/FAQ.md` for each problem with its exact error and fix. If you find something this file gets wrong on your board, the FAQ is where it should be corrected first.

## Check a blink without looking

After a flash, prove the LED pin is toggling through the debugger: `python3 tools/check/blink.py --board mspm0l1306 --probe <your probe serial>` from the repository root (xPack OpenOCD; it reads the GPIO registers for 3 s and never writes). PASS means an LED pin enabled as an output changed state; it passed on this board on 2026-10-13. This is the check to paste into a report.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- An image enters SHUTDOWN right after boot: DAP Connection Error, low power mode (`mspm0l1306/FAQ.md`)
- A window-watchdog example resets the chip every few milliseconds: Could not find MEM-AP (`mspm0l1306/FAQ.md`, `ports/cms32/README.md`)
- A deep-sleep port with only the watchdog timer as wake source (ran, woke every 0.997 s) (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, 2026-10-21)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Hold S1 (PA18), press and release S3 (reset), keep S1 held 2 s, then flash a non-sleeping image at once; replug USB afterwards (for the SHUTDOWN lockup; the chip then starts in the ROM bootloader until a plain power cycle) | hands | proven | `mspm0l1306/FAQ.md`, `mspm0l1306/AGENTS.md`, date not recorded |
| OpenOCD mailbox mass erase (mspm0_mass_erase) (for the watchdog reset loop) | remote | proven | `mspm0l1306/FAQ.md`, date not recorded |
| OpenOCD mailbox mass erase (for the SHUTDOWN lockup: OpenOCD exits when the debug port fails to answer) | remote | failed | `mspm0l1306/FAQ.md`, date not recorded |
| uhubctl power-off of the probe port (the tick counter restarted; the hub location is not recorded) | remote | proven | 2026-10-21 |

USB hub ports recorded for this board:

- The probe's hub port does not cut power, 2026-10-27. SRAM marker test: a word written at 0x20000FF0 through the DAP (no CPU connect, no reset) survived a 15 s `uhubctl -a off`; the chip and the XDS110 kept power. The port does drop the data link: the probe leaves USB within 1 s and is back 1.0 s after `on` on every port of this hub (nine ports tried), and the first debugger connect after a cut answers Error -261, the second works.

<!-- recovery:end -->

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| BitNetMCU handwriting AI on MSPM0 | Works | BitNetMCU handwriting models read all 10 test digits right on the LP-MSPM0G3507 and LP-MSPM0L1306, the same answers as on a Mac. | `ports/bitnetmcu/README.md` |
| Cmsemicon CMS32 examples on the LP-MSPM0L1306 | Works | Cmsemicon CMS32 examples run on the LP-MSPM0L1306 through one glue layer: 34 of 34 ran; 21 checked, 13 show nothing a probe can see. | `ports/cms32/README.md` |
| FreeRTOS in 4 kB of RAM on the LP-MSPM0L1306 | Works | FreeRTOS runs on the LP-MSPM0L1306 in 4 kB of RAM: the blink uses 1 kB. | `docs/experiments/2026-10-06-mspm0l1306-freertos-4kb.md` |
| MSP430FR2433 code examples on the LP-MSPM0L1306 | Works | Three MSP430FR2433 FRAM examples ran on the LP-MSPM0L1306: a persistent counter, a watchdog wake from LPM3 and a UART echo in LPM3. | `ports/msp430-to-mspm0/README.md` |
| MSPM0 examples moved up and down the family | Works | MSPM0 examples move between family members with only the SysConfig device changed (SysConfig is TI's pin and peripheral setup tool): all 133 images that built and write nothing outside main flash and SRAM ran on the LP-MSPM0L1306. | `portability/README.md` |
| Rust (embassy-mspm0) on the LP-MSPM0L1306 | Works | A Rust blinky and a serial hello run on the LP-MSPM0L1306 with embassy-mspm0. | `ports/rust-mspm0/README.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| TI's MSPM0 DriverLib examples built and run with GCC | Works | TI's MSPM0 DriverLib examples build and run with GCC from a terminal: all 224 no-RTOS examples with a GCC makefile built unchanged. | `portability/toolchains/gcc-mspm0/README.md` |
| Zephyr in 4 kB of RAM on the LP-MSPM0L1306 | Works | Zephyr runs on the LP-MSPM0L1306 in 4 kB of RAM: hello_world, blinky and CoreMark. | `docs/experiments/2026-10-06-mspm0l1306-zephyr-4kb.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
| One app, three ways: DriverLib, TI Drivers and Zephyr | Could work | The same 81-line app builds three ways on the LP-MSPM0L1306, and the DriverLib and Zephyr builds ran with the same readings. | `portability/three-ways/README.md` |
<!-- ports:end -->
