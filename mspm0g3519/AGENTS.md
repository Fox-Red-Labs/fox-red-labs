# AI assistant notes for the TI LP-MSPM0G3519 LaunchPad development kit

It behaves like the LP-MSPM0G3507 at 80 MHz with four times the memory, so bare-metal code ported from that board runs, and that Zephyr's blinky for it does not run. This file is the short list of numbers and traps, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0G3519 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C library for the chip's parts; **CoreMark** is a standard CPU speed test; **ECC** is error-correcting memory; **NMI** is the interrupt that cannot be turned off; **MATHACL** is the chip's math accelerator; **OpenOCD** is the open-source debug and flash tool. 1 kB = 1,024 bytes. Checked 2026-10-08 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, UniFlash 9.6.0 and Zephyr 4.4.2, on one LP-MSPM0G3519 (probe firmware 3.0.0.43 after an update).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm0g3519/` or `data/` are in the Fox Red Labs repo; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0G3519: Arm Cortex-M0+, 80 MHz, 512 kB dual-bank flash, 128 kB RAM (`mspm0g3519/README.md`) |
| On the chip | Two 4 Msps ADCs, 12-bit DAC, math accelerator, AES with key storage, true random-number generator, two CAN-FD ports (`mspm0g3519/README.md`) |
| Flash wait states | `FLASHWAIT` reads 2 at both 32 MHz and 80 MHz (`mspm0g3519/RESULTS-mspm0g3519.md`) |
| Serial | UART0 on PA10 and PA11; the Zephyr backchannel uses jumpers J12 and J13 on the XDS side (`mspm0g3519/RESULTS-mspm0g3519.md`) |
| Analog test wire | DAC on PA15 to ADC on PA25 (`mspm0g3519/RESULTS-mspm0g3519.md`) |
| Debugger | XDS110 on the board with EnergyTrace; a new board shipped on probe firmware 3.0.0.31 (`mspm0g3519/FAQ.md`) |
| Display, when fitted | Waveshare 3.5 inch panel: ST7796S screen, FT6336U touch at I2C address 0x38 over software I2C (`mspm0g3519/RESULTS-mspm0g3519.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, UniFlash 9.6.0, probe firmware 3.0.0.43 (`mspm0g3519/README.md`) |
| Probe serial and firmware | `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` |
| Back up | No `backup.py` recipe. Plan used on this board: DSLite `memory` reads of main flash (`-r 0x0,0x20000` words, 512 kB), data flash (0x41D00000, 16 kB) and NONMAIN (0x41C00000), each read twice and compared. Copies are in `mspm0g3519/data/` |
| Build TI's blink | `cp -R <SDK>/examples/nortos/LP_MSPM0G3519/driverlib/gpio_toggle_output .` then `../mspm0-first-blink/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang` (3.0 to 4.1 s) |
| Flash | `<UniFlash>/dslite.sh --mode flash -c MSPM0G3519_<serial>.ccxml -e -f -v gpio_toggle_output/ticlang/gpio_toggle_output.out` (4.6 to 4.8 s; the `.ccxml` is `mspm0g3519/MSPM0G3519_MG000001.ccxml` with your serial). OpenOCD also works, 0.6 s |
| Check | `python3 tools/check/blink.py --board mspm0g3519 --probe <serial>` reads only PB22, PB26 and PB27; it printed FAIL on TI's shipped image, which drives none of them, and PASS on the blink image the 2026-10-13 front-door run built (PB27 toggled). For your own LED pin, read the pin or look |
| Restore | `<UniFlash>/dslite.sh --mode flash -c <ccxml> -e -f -v backup-flash.bin,0x0` then a power cycle, read back and compare |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Zephyr blinky builds and flashes, nothing blinks; program counter at `z_SysNmiOnReset`, `SYSCTL.NMIIIDX` reads 6 | NMI index 6 is SRAMDED (SRAM double-error); it follows DSLite's start-after-flash path, why is not found | Pulse nRST after the DSLite flash (`mspm0_board_reset` in OpenOCD); the image then runs (`portability/arduino/zephyr/README.md`, 2026-10-25) | LED blinks |
| Zephyr prints `FLASH: 256 KB` and `RAM: 64 KB` | The board file describes bank 0 only; the chip has 512 kB and 128 kB | Size a Zephyr application for 256 kB flash and 64 kB RAM | The build summary fits |
| `adc12_internal_temp_sensor_mathacl` prints about 507 | The raw code is about 815 as on the G3507, but the factory trim constant is 1881 here and 795 there | Treat the raw code as relative; cause unknown | Code is near 815 |
| UART0 loopback PA10 to PA11 fails at 5.5 and 6.0 Mbaud, sends nothing at 12 Mbaud | Cause unknown | Use 9,600 baud to 5.33 Mbaud or 6.4 to 10.67 Mbaud, and test your exact rate | Echo matches |
| A Zephyr peripheral pin stays low (or does the wrong thing) although the peripheral runs, for example PWM on PB27 | `lp_mspm0g3519.dts` includes the shared MSPM0G1x0x/G3x0x pin file, whose function numbers differ on the G351x for 136 of 443 entries (`tima1_ccp1_pb27` is function 6 there, COMP2.OUT here; TIMA1.CCP1 is 9) | Define the pin group yourself from SysConfig's `MSPM0G351X.json` (`portability/arduino/zephyr/variants/lp_mspm0g3519/lp_mspm0g3519-zw.overlay`) | The pin reads the expected duty (2026-10-25) |
| Zephyr main on this board: a second interrupt edge mode on a pin fires on both edges, and PWM duty comes out inverted | `gpio_mspm0` ORs edge bits into `POLARITY15_0` and never clears them; `pwm_mspm0` writes the pulse as the low time in edge-aligned down count | Clear the pin's two polarity bits before attaching; write `period - pulse` | The interrupt fires on one edge and the duty is right (2026-10-25) |
| The first I2C transfer to the Waveshare panel's FT6336U fails right after boot | The FT6336U holds the bus busy for about 65 ms after its reset line (PB11) goes high | Hold the reset low 5 ms, release it, wait 400 ms before the first transfer | The device-ID read returns 0x64 (`ports/renesas-ra-fsp/README.md`, run 2026-10-25) |
| An I2C read returns the ID, then the next write never completes and the bus stays busy | The I2C controller's ACK bit was left on for a read, so the last byte is acknowledged and the slave keeps sending | Leave the ACK bit off on reads so the controller sends the NACK that ends them | Writes complete after reads (`ports/renesas-ra-fsp/README.md`, run 2026-10-25) |
| A Pico SDK example linked with TI Arm Clang fails with `symbol "puts" redefined`, or keeps a 256 B stack | The C library keeps `puts` and `fputs` in one object; the SysConfig `device_linker.cmd` sets `--stack_size=256` and overrides an earlier `-Wl,--stack_size` | Replace `fputs` as well; put `-Wl,--stack_size=0x800` after `device_linker.cmd` | Five pico-examples link and run unchanged (`ports/pico-sdk/README.md`, run 2026-10-25) |
| The portable ADC app reads a swinging 900 to 2,600 on PA25 with nothing wired | Something on the header or BoosterPack drives PA25; the G5187's floating PA25 is steady | Read ADC channel 11 (internal temperature sensor, `disChan11 = true`) with the 2.5 V reference; steady at 1,085 to 1,086 | Lines `t=<n> adc=1085 avg=1085` once a second (2026-10-25) |
| `adc_singlechannel` under FreeRTOS prints only its start line | TI's own SDK example for this board does the same; cause not found | Use DriverLib ADC calls | Result lines appear (2026-10-25) |
| An F3 TI Drivers example in the G3519 project runs but the LED stays dark | The MSPM0 GPIO table leaves PA0 as `GPIO_DO_NOT_CONFIG`, so the F3 source's `GPIO_write` drives nothing | Add `GPIO_setConfig` for the LED before the first write | PA0 output enable reads 1 (2026-10-25) |
| No MSPM0 SPI or I2C driver in Zephyr 4.4.2 | Zephyr lacks them | Write a driver first (`docs/experiments/2026-10-10-mspm0g3519-zephyr-lvgl.md`) | The touch chip answers |
| TI's UART console example prints nothing | The example uses UART1 on PA10 and PA11, which reach the probe's USB serial port only through jumpers J12 and J13 on the XDS side | Leave J12 and J13 on XDS, nothing wired to J4 PA10 to PA13, read `/dev/cu.usbmodem<probe serial>1` at 115200 with a held-open capture | Banner `==== MSPM0 Console Test ====`, then `> Testing...N` once a second (2026-10-24) |
| A TI MSPM33 DriverLib example carried over stops with `GPIO_LEDS_USER_LED_2_PORT` undeclared, or a UART example waits forever | The MSPM33 LEDs sit on three ports and the UART examples expect a TX to RX wire | Change the 5 lines to `GPIO_LEDS_PORT`; add `UART1.enableInternalLoopback = true;` to the `.syscfg`; keep PA10 to PA13 reserved (`portability/everywhere/PORTING.md` section 7.2) | LED pins toggle; `rxPacket` reads MSP! |

Traps shared with other boards: TRAPS.md.

## What fits and what does not

Memory was never the limit here (`mspm0g3519/RESULTS-mspm0g3519.md`):

| Program | Flash and RAM | Source |
|---|---|---|
| Zephyr blinky | 11.8 kB flash, 4 kB RAM; builds and flashes, does not run | `data/results/mspm0g3519.csv` |
| LVGL 9.6 touch demo, bare metal | 126.2 kB flash, 25.2 kB RAM | `mspm0g3519/RESULTS-mspm0g3519.md` |
| Zephyr 4.4.2 with LVGL 9.5.0, dummy display | 253.5 kB flash, 49.5 kB RAM; linked, not flashed | `docs/experiments/2026-10-10-mspm0g3519-zephyr-lvgl.md` |
| All four room-occupancy models, up to 30k parameters | all give the right answer on 20 of 20 test vectors | `mspm0g3519/RESULTS-mspm0g3519.md` |

## What ran here

- Interrupt entry is 31 cycles bare metal and 89 clocks in the control loop, where the G3507 took 69; cause not found (`mspm0g3519/RESULTS-mspm0g3519.md`).
- CoreMark 182.73 per second at -O3 and 80 MHz (2.284 per MHz); 78.84 at 32 MHz (`mspm0g3519/RESULTS-mspm0g3519.md`).
- GPIO register write 1.0 cycle; ADC with DMA 3.93 million samples a second (same file).
- Control loop top rate with no missed period: 4.29, 10.79, 37.95 and 70.30 kHz in the four builds (same file).
- Occupancy model with 30k parameters: 129.39 ms at -O3; handwriting model 2.64M cycles at 32 MHz (same file).
- Hardware AES on 1 kB in CBC: 9.6k cycles; hardware CRC-32 1.9k cycles (same file).
- LVGL on the panel: 15 to 19 refreshes a second at 80 MHz, read from firmware counters; the demo was seen on the screen on 2026-10-09 (same file).
- Example sweep: 251 of 259 build; 107 ran with no fault (same file).
- MicroPython full image: 1,112 pystones per second at 80 MHz with `ports/micropython/tests/pystone_lowmem.py` (3 runs, 1,111 to 1,114). Run it with `ports/micropython/tools/paste_raw.py` on `/dev/cu.usbmodemMG0000011`; the 1 ms tick matched the Mac's clock (`data/results/mspm0g3519.csv`).
- `python3 tools/frl/frl.py run --board mspm0g3519 --probe <serial> --pattern 150,150,150,900 --no-backup` took 9.5 s warm and 30.0 s cold to a verified blink on PB26. It needs `FRL_SYSCONFIG` pointed at SysConfig 1.28.0 when 1.26.3 is not installed, and xPack OpenOCD at `~/xpack-openocd-0.12.0-7` (`data/results/mspm0g3519.csv`).
- Not run: power, the Audio BoosterPack tone (the pack is on another board) and AI cost runs (`data/results/mspm0g3519.csv`).

## Check a blink without looking

After a flash, prove the LED pin is toggling through the debugger: `python3 tools/check/blink.py --board mspm0g3519 --probe <your probe serial>` from the repository root (xPack OpenOCD; it reads the GPIO registers for 3 s and never writes). PASS means an LED pin enabled as an output changed state; its pins come from TI's blink example for this board; it ran on this board 2026-10-13 (FAIL on TI's shipped image, PASS on the front-door run's image). This is the check to paste into a report.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| 2048 and Minesweeper on the LP-MSPM0G3519 with a 3.5 inch touch display (also built for the LP-MSPM0G3507 and LP-MSPM33C321A) | Works | 2048 and Minesweeper run on the LP-MSPM0G3519 with a 3.5 inch touch display. The same source builds for the LP-MSPM0G3507 (10.8 kB of flash) and, with a stub screen, for the LP-MSPM33C321A (8.1 kB). | `ports/touch-games/README.md` |
| Arduino sketches on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core: Blink and AnalogReadSerial | Works | Arduino's stock Blink and AnalogReadSerial run unchanged on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core. | `portability/arduino/zephyr/README.md` |
| CHIP-8 interpreter and the chip8-test-suite on the LP-MSPM0G3519 touch panel | Works | A CHIP-8 interpreter runs the chip8-test-suite ROMs on the LP-MSPM0G3519 touch panel, with the frame buffer equal to the host's for five of six. | `ports/chip8/README.md` |
| ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards | Works | The ESP32-BLE-Gamepad descriptor and reports match the library's output byte for byte on four boards, and 15 of 15 button changes from the LP-EM-CC2340R5 reached a Mac as gamepad reports. | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| liblc3 (LC3, the Bluetooth LE Audio codec) on the LP-MSPM33C321A and LP-MSPM0G3519 | Works | liblc3 (LC3, the Bluetooth LE Audio codec) encodes and decodes frames on the LP-MSPM33C321A with the CRC-32 of every frame equal to the Mac build. | `ports/lc3/README.md` |
| MicroPython 1.29.0 on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM0G5187 | Works | A new MicroPython port boots on the LP-MSPM0G3519 with the MicroPython banner and drives a 3.5 inch touch screen from Python. | `ports/micropython/README.md` |
| minimp3 MP3 decoder on the LP-MSPM33C321A and LP-MSPM0G3519 | Works | minimp3 decodes MP3 clips bit-exact against the Mac on the LP-MSPM33C321A and LP-MSPM0G3519. | `ports/minimp3/README.md` |
| minmea GPS sentence parser on MSPM0 | Works | minmea, an NMEA GPS parser, runs on the LP-MSPM0G3519 with its files unchanged and a 21-line shim for one missing call. | `ports/minmea/run-g3519.txt` |
| Peanut-GB Game Boy emulator on MSPM0 and MSPM33 | Works | Peanut-GB plays Tobu Tobu Girl on the LP-MSPM33C321A with the Educational BoosterPack screen at the console's own speed. | `ports/peanut-gb/README.md` |
| Post-quantum key exchange and signatures on seven boards: LP-CC1354P10-1, LP-EM-CC2340R5, LP-AM243, LP-MSPM0G3519, LP-MSPM33C321A, LP-EM-CC2755P10 and LP-MSPM0G5187 | Works | ML-KEM-768 key exchange and ML-DSA-44 signatures pass NIST's known-answer vectors on seven boards, from a 36 kB Cortex-M0+ to an 800 MHz Cortex-R5F; the LP-CC1354P10-1 also agrees a key with a Mac. | `cc1354p10/pqc-join/README.md` |
| Raspberry Pi Pico SDK examples on the LP-MSPM0G3519 | Works | Raspberry Pi pico-examples (serial hello, I2C bus scan, blink, PWM fade, ADC read) run unchanged on the LP-MSPM0G3519 over a thin layer on DriverLib. | `ports/pico-sdk/README.md` |
| Renesas FSP examples (UART, ADC, I2C, GPT timer) on the LP-MSPM0G3519 | Works | Four of five Renesas FSP examples (FSP is Renesas's driver layer) run on the LP-MSPM0G3519 on a layer that maps FSP calls onto DriverLib, TI's register-level driver library. | `ports/renesas-ra-fsp/README.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| STM32 UART Xmodem bootloader (ferenc-nemeth/stm32-bootloader) on the LP-MSPM0G3519 | Works | ferenc-nemeth's STM32 Xmodem bootloader takes an application over UART on the LP-MSPM0G3519, writes it to flash and jumps to it. | `docs/experiments/2026-10-20-porting-pack-race.md` |
| uLisp Lisp interpreter on the LP-MSPM0G5187, LP-MSPM0G3507, LP-MSPM0G3519, LP-EM-CC2340R5, LP-EM-CC2755P10, LP-MSPM33C321A, LP-AM243 and LAUNCHXL-F2800137 | Works | uLisp, an Arduino sketch, gives a Lisp prompt on the serial port of the LP-MSPM0G5187, LP-EM-CC2340R5 and LP-EM-CC2755P10: 19 of 19 REPL (type-and-run loop) checks pass on each. | `ports/ulisp/README.md` |
| wasm3 (WebAssembly interpreter) on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM33C321A | Works | wasm3 runs a WebAssembly module on the LP-MSPM0G3519 and prints PASS on the serial line. | `ports/wasm3/README.md` |
| Arduboy2-compatible layer and three open-source games on MSPM0 and MSPM33 | Could work | Snake, TileWorld and Crate Confusion build unchanged against an Arduboy2-compatible layer for four kits, and Snake draws on the LP-MSPM33C321A's Educational BoosterPack screen. | `ports/arduboy/FIT.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. The Educational, Sharp128 and Audio builds also build for the LP-MSPM0G3519, and the Educational and Sharp128 builds for the LP-MSPM0G3507. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
| ZX Spectrum 48K (floooh/chips Z80 core) on the LP-MSPM0G3519 touch panel | Could work | A ZX Spectrum 48K runs on the LP-MSPM0G3519 touch panel, and its screen memory equals the host machine's after 400 frames. | `ports/zx-spectrum/README.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `mspm0g3519/README.md`, then `mspm0g3519/FAQ.md` for each problem, `mspm0g3519/RESULTS-mspm0g3519.md` for numbers, and `data/results/mspm0g3519.csv` for every row. If this file gets something wrong on your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A running program leaves the factory region unreadable (all zeros) in a low-power state (`tools/frl/target.py`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| OpenOCD reset halt, read, reset run (writes nothing) (the program runs again afterwards) | remote | proven | `tools/frl/target.py`, 2026-10-24 |

Restore: DSLite: dslite.sh --mode flash -e -f -v backup-flash.bin,0x0, then a power cycle, read back and compare. Regions: main flash. (`mspm0g3519/AGENTS.md`)

<!-- recovery:end -->
