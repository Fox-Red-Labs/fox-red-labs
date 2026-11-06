# AGENTS.md for the TI LP-MSPM0G3507 LaunchPad: what an AI coding assistant needs to know

Know this first: the chip has no floating-point unit and no neural accelerator, TI's examples end in a breakpoint that crashes a board with no debugger attached, and Zephyr, Arduino and display work on it each have a few silent traps. This file is the short list, each item from runs on one borrowed board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0G3507 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C library for the chip's parts; **MATHACL** is the chip's math accelerator; **CoreMark** is a standard CPU speed test; **LVGL** is a graphics library for small screens; **west** is Zephyr's build and flash tool; **NONMAIN** is the flash area that holds the chip's boot settings. 1 kB = 1,024 bytes. Checked 2026-10-05 and 2026-10-06 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, CCStudio 21.0.1, xPack OpenOCD 0.12.0-7 and Zephyr 4.4.2, on one borrowed LP-MSPM0G3507 (probe <probe serial>).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm0g3507/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0G3507: Arm Cortex-M0+, 80 MHz, 128 kB flash, 32 kB RAM (`mspm0g3507/mcu-bench/README.md`) |
| On the chip | Math accelerator, hardware AES and CRC, DAC; no floating-point unit (one float multiply-add costs about 290 cycles) and no neural accelerator (`mspm0g3507/mcu-bench/README.md`) |
| LEDs | An RGB LED; TI's blink already sets up all three colors |
| Buttons | S1 on PA18, active-high (`mspm0g3507/FAQ.md`) |
| Serial | UART0 on PA10 and PA11 through the probe; PA10 also reaches the serial bridge via jumper J21. The port ending in 1 (`/dev/cu.usbmodemMG3500011`) is the chip's UART at 115200 8N1 (`mspm0g3507/FAQ.md`) |
| Test wires | PWM check PA12 to PA13 (J4 pins 32 and 31) |
| Debugger | XDS110 on the board; a Mac sees a probe and two serial ports with no driver. Check it with `xdsdfu -e` (`mspm0g3507/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, CCStudio 21.0.1, xPack OpenOCD 0.12.0-7, Zephyr 4.4.2 |
| Probe serial | `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` |
| Back up | No tested backup command; read the whole flash with DSLite before the first write. No `backup.py` recipe exists for this board |
| Build TI's blink | `cp -R <SDK>/examples/nortos/LP_MSPM0G3507/driverlib/gpio_toggle_output .` then `mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang` (TI's makefile with the default macOS paths). Or the scripted Zephyr route below |
| Flash | OpenOCD with the Zephyr tree's board file: `openocd -s <OCD>/openocd/scripts -f boards/ti/lp_mspm0g3507/support/openocd.cfg -c "adapter serial <serial>" -c init -c "reset halt" -c "program <file>.out verify" -c mspm0_board_reset -c shutdown` |
| Zephyr route | `west build -p always -b lp_mspm0g3507 samples/basic/blinky -d ../build-blinky`, then `west flash -d ../build-blinky --openocd "$OCD/bin/openocd" --openocd-search "$OCD/openocd/scripts"`, then `mspm0_board_reset` (`mspm0g3507/zephyr/setup-intel-mac.sh`; about 3 min from clean) |
| Check | `python3 tools/check/blink.py --board mspm0g3507 --probe <serial>` (pins from TI's blink example; not run on this board) |
| Restore | Not tested |

In CCStudio with Claude Code driving, the blink was running 149 s after the board was seen (`data/results/mspm0g3507.csv`).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `west sdk install` prints an empty list on an Intel Mac | Zephyr SDK 1.0.1 has no Intel macOS build | Use the Arm compiler from SDK 0.17.4 with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile` and the prebuilt xPack OpenOCD (`mspm0g3507/zephyr/README.md`) | `west build` finds the compiler |
| Zephyr blinky does not blink after a debugger soft reset | The chip sits in the NMI handler | Run `mspm0_board_reset`, or press reset | LED blinks |
| An `aes_*`, `crc_*`, `dma_*`, `mathacl_*` or `adc12_*_dma` example HardFaults with no debugger | TI ends them with `__BKPT(0)` | Attach the debugger with halting on, or ignore the end state | No fault after the work is done |
| Board gone after `sysctl_power_policy_sleep_to_standby` or `timx_timer_mode_pwm_x_trig_stop_restore` | The low-power example cuts the debug link | Unplug and replug USB | `xdsdfu -e` lists the probe |
| `fatal error: 'main.h' file not found` (ported STM32 project) | The initializer does not put `Inc/` on the include path | Add `-I"${PROJECT_ROOT}/Inc"` | Build proceeds |
| `error: unknown type name 'uint32_t'` in `Src/system_stm32g0xx.c` | The port removed ST's header that pulled in `stdint.h` | Add `#include <stdint.h>` | Build proceeds |
| `make` in `Debug/` stops with `invalid option -- O` | Apple's `make` is too old for the generated makefile | Use `ccs/utils/bin/gmake -B all` from CCStudio | Build finishes |
| `Claude requested permissions to use mcp__ccs-project__changeSysConfigVersion, but you haven't granted it yet.` | A headless run treats the "ask" list as denied | Leave CCStudio's `.claude/settings.local.json` out of the run, or run the skill interactively and approve | The step runs |
| `fatal error: 'Print.h' file not found` (Arduino_GFX) | Only old Arduino cores ship that header | Add ArduinoCore-API's `api/deprecated/` and `api/deprecated-avr-comp/` to the include path | Sketch builds |
| `stack_free=0` in the LVGL stats line | A 2 kB stack is too small | Use 6 kB; the measured peak was 3.9 kB | `stack_free` stays above 0 |

Traps shared with other boards: TRAPS.md.

## What fits and what does not

| Program | Result | Source |
|---|---|---|
| Zephyr blinky | 11.7 kB flash, 3.9 kB RAM | `data/results/mspm0g3507.csv` |
| LVGL demo, trimmed | 125.4 kB of 128 kB flash at 32 MHz; 25.1 kB RAM (12 kB heap, 6 kB stack, 6.2 kB draw buffer) | `mspm0g3507/FAQ.md` |
| Same LVGL demo with a second font | 146.7 kB: does not fit | `mspm0g3507/FAQ.md` |
| Arduino_GFX screen sketch | 75.6 kB | `mspm0g3507/FAQ.md` |
| Code from outside flash | not possible: the data sheet lists no external memory bus | `mspm0g3507/FAQ.md` |

## Traps

1. **SysConfig stays at 32 MHz when you ask for 80.** A leftover `SYSCTL.forceDefaultClkConfig` is accepted without a warning. Remove it, use the clock tree, and read `CPUCLK_FREQ` in `ti_msp_dl_config.h` (`mspm0g3507/FAQ.md`).
2. **A ported button never fires.** S1 is active-high; ST's active-low setup (pull-up, falling edge) cannot trigger. Use pull-down and rising edge (`mspm0g3507/FAQ.md`).
3. **PWM on PA10 and the serial port collide.** PA10 is also the USB-serial transmit pin (`mspm0g3507/FAQ.md`).
4. **`DL_SPI_setDataSize` does nothing and returns no error while SPI is enabled.** Disable SPI, set the size, enable it. SPI above 16 MHz needs the high-speed pins PA9, PA12 to PA14 and PA19 (`mspm0g3507/FAQ.md`).
5. **The touch chip never answers on the I2C1 block.** Software I2C on the same pins got correct registers every time. Switch the pins to plain GPIO first, or the ID reads 0 (`mspm0g3507/FAQ.md`).
6. **A float FOC or PID loop cannot hold 50 or 100 kHz.** Float FOC tops out at 4.26 kHz, 70.80 kHz with IQmath on MATHACL; Q15 PID held 190.9 kHz (`mspm0g3507/FAQ.md`).

## What ran here

- Blink changed and rebuilt: 2 min 29 s from board plug-in, tools installed (`data/results/mspm0g3507.csv`).
- CoreMark 183.61 per second at -O3 and 80 MHz (2.30 per MHz); 78.82 at 32 MHz (`data/results/mspm0g3507.csv`).
- Sine and cosine in 116 cycles with MATHACL against about 9,900 in software (`mspm0g3507/mcu-bench/README.md`).
- Embench-IoT: 18 of 19 pass; AES 20.3 times and CRC 13.5 times faster than software (`data/results/mspm0g3507.csv`).
- SDK sweep: 222 of 270 examples build; 213 ran, none crashed (`data/results/mspm0g3507.csv`).
- Waveshare 3.5 inch touch display and LVGL: 12 wires, full-screen fill 168 ms at 16 MHz SPI with the CPU at 32 MHz (`mspm0g3507/display-test/README.md`).
- Not run: sleep current (no meter), FreeRTOS, FFT. Dhrystone was left out on purpose (`data/results/mspm0g3507.csv`).

## Check a blink without looking

After a flash, prove the LED pin is toggling through the debugger: `python3 tools/check/blink.py --board mspm0g3507 --probe <your probe serial>` from the repository root (xPack OpenOCD; it reads the GPIO registers for 3 s and never writes). PASS means an LED pin enabled as an output changed state; its pins come from TI's blink example for this board; it has not run on this board. A camera over the bench is optional; this is the check to paste into a report.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| 2048 and Minesweeper on the LP-MSPM0G3519 with a 3.5 inch touch display | Works | 2048 and Minesweeper run on the LP-MSPM0G3519 with a 3.5 inch touch display. | `ports/touch-games/README.md` |
| Arduino display libraries on the LP-MSPM0G3507 | Works | Arduino_GFX's graphics test draws on an ST7796S screen on the LP-MSPM0G3507, through a 649-line Arduino API layer under the unmodified libraries. | `portability/arduino/README.md` |
| BitNetMCU handwriting AI on MSPM0 | Works | BitNetMCU handwriting models read all 10 test digits right on the LP-MSPM0G3507 and LP-MSPM0L1306, the same answers as on a Mac. | `ports/bitnetmcu/README.md` |
| Five STM32CubeG0 examples through TI's migration skill on the LP-MSPM0G3507: 3 ran as written, 2 after fixes | Works | TI's migration skill (a tool inside CCStudio that rewrites ST code for a TI chip) moved five STM32CubeG0 examples to the LP-MSPM0G3507: 3 ran as written, 2 after fixes. | `mspm0g3507/stm32-port-eval/README.md` |
| MicroPython 1.29.0 on the LP-MSPM0G3519 and LP-MSPM0G3507 | Works | A new MicroPython port boots on the LP-MSPM0G3519 with the MicroPython banner and drives a 3.5 inch touch screen from Python. | `ports/micropython/README.md` |
| minmea GPS sentence parser on MSPM0 | Works | minmea, an NMEA GPS parser, runs on the LP-MSPM0G3519 with its files unchanged and a 21-line shim for one missing call. | `ports/minmea/run-g3519.txt` |
| MSPM0 examples moved up and down the family | Works | MSPM0 examples move between family members with only the SysConfig device changed (SysConfig is TI's pin and peripheral setup tool): all 133 images that built and write nothing outside main flash and SRAM ran on the LP-MSPM0L1306. | `portability/README.md` |
| Peanut-GB Game Boy emulator on MSPM0 and MSPM33 | Works | Peanut-GB plays Tobu Tobu Girl on the LP-MSPM33C321A with the Educational BoosterPack screen at the console's own speed. | `ports/peanut-gb/README.md` |
| TI's MSPM0 DriverLib examples built and run with GCC | Works | TI's MSPM0 DriverLib examples build and run with GCC from a terminal: all 224 no-RTOS examples with a GCC makefile built unchanged. | `portability/toolchains/gcc-mspm0/README.md` |
| uLisp Lisp interpreter on the LP-MSPM0G5187, LP-MSPM0G3507 and LP-MSPM0G3519 | Works | uLisp, an Arduino sketch, gives a Lisp prompt on the LP-MSPM0G5187's serial port: 19 of 19 REPL (type-and-run loop) checks pass. | `ports/ulisp/README.md` |
| wasm3 (WebAssembly interpreter) on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM33C321A | Works | wasm3 runs a WebAssembly module on the LP-MSPM0G3519 and prints PASS on the serial line. | `ports/wasm3/README.md` |
| Arduboy2-compatible layer and three open-source games on MSPM0 and MSPM33 | Could work | Snake, TileWorld and Crate Confusion build unchanged against an Arduboy2-compatible layer for four kits, and Snake draws on the LP-MSPM33C321A's Educational BoosterPack screen. | `ports/arduboy/FIT.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
| MSPM0 code on the CC2340R5 and CC1354P10 radio chips | Could work | MSPM0 driver examples build for the CC2340R5 with a 47-line shim, the first 5 ran with the same pin states as TI's own build, and 4 of 4 ran on the CC1354P10. | `portability/README.md` |
| One app, three ways: DriverLib, TI Drivers and Zephyr | Could work | The same 81-line app builds three ways on the LP-MSPM0L1306, and the DriverLib and Zephyr builds ran with the same readings. | `portability/three-ways/README.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `mspm0g3507/FAQ.md` for each problem, then `mspm0g3507/mcu-bench/README.md` for numbers and `data/results/mspm0g3507.csv` for every row. If this file gets something wrong on your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- TI low-power examples (sysctl_power_policy_sleep_to_standby, timx_timer_mode_pwm_x_trig_stop_restore) cut the debug link (`mspm0g3507/AGENTS.md`, `mspm0l1306/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug and replug USB (xdsdfu -e lists the probe again) | hands | proven | `mspm0g3507/AGENTS.md`, date not recorded |

<!-- recovery:end -->
