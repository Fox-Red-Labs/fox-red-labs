# AI assistant notes for the TI LP-MSPM33C321A LaunchPad

That the board ships with a demo (an OLED video in a 16 MB flash the debugger cannot read) that is not in the SDK, so you save the chip's flash and avoid a few SDK examples that rewrite boot settings before you flash anything. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM33C321A FAQ](FAQ.md) in the Fox Red Labs repo. In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DSLite** is TI's command-line flasher and memory reader; **NONMAIN** is the flash area that holds the chip's boot settings; **CoreMark** is a standard CPU speed test; **FOC** (field-oriented control) is the math a motor drive runs every PWM period; **TrustZone** splits an Arm program into a secure and a non-secure half. 1 kB = 1,024 bytes. Checked 2026-10-08 and 2026-10-09 on an Intel Mac, macOS 15.7, with MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, UniFlash 9.6.0 and CCStudio 21.0.1 (for its compiler only), probe firmware 3.0.0.43, on one LP-MSPM33C321A with the Education BoosterPack stacked.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm33c321a/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM33C321A: Arm Cortex-M33 up to 160 MHz with FPU and TrustZone, 1 MB flash, 256 kB RAM (`mspm33c321a/README.md`) |
| Not on the chip | No math accelerator; sinf plus cosf take 454 cycles at 32 MHz |
| Hardware blocks | AES-256 (41.7 times faster than tiny-AES-c at 1 kB) and a CRC module (2.75 times faster than a table) |
| Board | 0.91 inch OLED, 16 MB QSPI flash, RGB LED, two buttons, 80-pin BoosterPack headers, XDS110 probe with EnergyTrace (`mspm33c321a/README.md`) |
| LEDs | RGB: blue PA2, red PC26, green PC27 (`mspm33c321a/blink.sh`) |
| Education BoosterPack I2C | SCL on PB13 (pin 9), SDA on PB12 (pin 10); light sensor answers at 0x44 (`mspm33c321a/FAQ.md`) |
| Clocks | 32 MHz (0 flash wait states, code cache off) and 160 MHz (2 wait states, caches on), both from TI's examples (`mspm33c321a/FAQ.md`) |

## Start here

Built with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, UniFlash 9.6.0 and CCStudio 21.0.1 (for its compiler and DSLite), probe firmware 3.0.0.43, in the default macOS folders under `/Applications/ti` (edit the top of the scripts if yours differ). The LED is the RGB: blue PA2, red PC26, green PC27.

| Step | Command |
|---|---|
| Probe serial `<serial>` | `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` |
| Identify (read-only; writes the connection file) | `mspm33c321a/tools/identify.sh <serial>` |
| Backup (read each range twice and compare) | `DSLite=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite` then `$DSLite memory -c mspm33c321a/MSPM33C321A_<serial>.ccxml -r 0x10000000,262144 -s 32 -o main.bin`. Repeat with `-r 0x80000000,8192` (data flash) and `-r 0x80101800,512` (boot configuration, read only, never write it). The 16 MB QSPI flash holding the demo video cannot be read by the debugger |
| Build and flash the blink (period in ms) | `mspm33c321a/blink.sh 250 <serial>` |
| Check | `mspm33c321a/tools/led_check.sh <serial>` reads the LED pin over the probe |
| Restore | Write the saved flash back with DSLite and read it again; the comparison must show no difference. Not run end to end here (`mspm33c321a/README.md`) |

`blink.sh` runs TI's makefile with `MSPM33_SDK_INSTALL_DIR`, `SYSCONFIG_TOOL` and `TICLANG_ARMCOMPILER` set and `GCC_ARMCOMPILER=` empty, then `DSLite flash -c <ccxml> -e -f -v <image>.out`. First command to verified changed blink took 19 s: build 4.8 s, flash 11.3 s. The LED itself was not watched by eye.

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Device Connection Error ... low power mode` after the FreeRTOS `blink_led` example | The image leaves the chip asleep and the debugger cannot connect | `xds110reset -s <serial> -a toggle -d 200` (next to `xdsdfu`), then flash a normal image | DSLite connects |
| The same `Device Connection Error` after a flash write was cut off, still there after `xds110reset` and a USB replug | A half-written image faults at boot (CS_DAP register `CFGAP_BOOTDIAG` reads 0x07, a nested fault), so the core never stops for the debugger | Connect with wait-for-debug: in a CCStudio script (`ccs/scripting/run.sh`), `session.settings.set("EnableWaitForDebug", true)` before `target.connect()` holds the core in its reset handler. Then `settings.set("FlashEraseSelection", "Erase MAIN memory only")` and `memory.loadBinaryProgram(<saved main>.bin, 0x10000000n)`. No mass erase or factory reset needed; NONMAIN is not touched | A 1 MB read-back at 0x10000000 hashes the same as the save (2026-10-22: write 12.8 s, read 4.5 s) |
| A flash write passes verify (an on-chip check and a read-back) on the MSPM33 or AM13, but the new program never starts: the LED does not change, or the old program still runs | A program that switched the flash caches on (TI's examples do at 160 MHz) leaves old flash lines in them. The debugger reads flash past the caches, so verify passes, and the reset then fetches the old vector table from the cache. Seen 2026-10-22: PC inside the previous image after a correct write | Clear `FRI FRD_INTF_CTRL` (0x4002900C: bit 1 data cache, bit 2 code cache; same address on both chips) while the core is halted, write, then reset. The Digital Lab does it | The LED toggles at the image's rate after the reset |
| The same wedge, recovered from a web page or a script instead of CCStudio | As the row above | `python3 tools/frl/lock.py exec --serial <serial> --op webflash -- node tools/webflash/node-run.mjs flash --probe <serial> --board mspm33c321a --image prebuilt/mspm33c321a/blink-250-250` connects with the sequence below, writes and starts. Sequence: ten times (SPREC bit 0, DPREC0 bit 14, SPREC bit 0) at AP 4 (CS_DAP PWRAP, IDR 0x002E0002; DPREC0 at register 0x00, SPREC at 0xF0), DPREC0 0x002C4027, 0x000C6027, 0x000C602F, DHCSR 0xA05F0001, DEMCR bit 0 set, DHCSR 0xA05F0003 ten times, DPREC0 0x000C202F, 0x0048202F, 0x0008602F, 0x00120008 (read from CCStudio's own log, 2026-10-22). Clearing bit 14 before the vector catch drops the chip back into the fault | The core halts, flash reads back, the next write starts |
| `Verification failed: Values at address 0x00000020 do not match` | The image linked at 0x20, not 0x10000000; the failed load leaves flash erased | Fix the link line (next row), reset and reflash | Flash verifies |
| `Case insensitivity of options has been deprecated; T must be written as t` | TI Arm Clang 5.1.1 ignores `-T./mspm33c321a.cmd` | Change it to `./mspm33c321a.cmd` in the example's makefile | The map shows code at 0x10000000 |
| FreeRTOS test hangs in an interrupt that calls `xSemaphoreGiveFromISR` | At the config's "priority 1" (raw 0x04) the kernel's assert stops the core | `NVIC_SetPriority(irq, 0x40u >> (8 - __NVIC_PRIO_BITS))`; the prebuilt kernel has no static allocation | The test runs on |
| I2C scan on the Education BoosterPack finds nothing | Several SDK READMEs list PB0 and PB1 | Use SCL PB13 (pin 9) and SDA PB12 (pin 10) | The light sensor answers at 0x44 |
| The Education BoosterPack's microphone pin ignores speech, the light sensor does not answer at 0x44, buttons read pressed | The pack on the board is not a seated Education BoosterPack (2026-10-30; cause on the bench not confirmed) | Build and run `portability/boosterpacks/edumkii/examples/probe` (`APP=probe`) and look for 0x44 before debugging code | The scan lists 0x44 and the microphone pin sits near mid-scale (`mspm33c321a/FAQ.md`) |
| A C++ image that allocates stops after its first console line | SysConfig rewrites `device_linker.cmd`, so the heap stays at the default 2 kB | Add `-Wl,--heap_size=32768` to the link flags | The build log has no ".sysmem ... default size" warning and the image runs (`mspm33c321a/FAQ.md`) |
| A GPIO set as output always reads 0 | The input stage is off | Call `DL_GPIO_initDigitalInputFeatures` | The read follows the pin |
| DSLite flash of an IAR `.out` prints `Flash Programmer: Error in image size. Length of block is 276, but it should be divisible by 16` and the write never finishes | IAR places the 276-byte vector table as its own load block; the programmer writes 128-bit words | Write an Intel HEX padded with 0xFF over the flash footprint (`ielftool --fill "0xFF;0x10000000-<end>" --ihex in.out out.hex`; `tools/toolchains/iar/coremark_iar.sh` does it) and flash the `.hex` | Flash takes about 5 s and verifies (`tools/toolchains/iar/cm_run.py`, 2026-10-24) |
| An IAR `-Ohs` image is about a third slower than TI Arm Clang `-O3` | `-Ohs` alone keeps IAR's size limits on; the flash wait states and cache registers read the same on both builds | Add `--no_size_constraints` to every compile | The score rises to within 10 percent of Clang `-O3` (2026-10-24) |
| `hsadc_max_freq_dma` claims 9.375 MHz and its array holds repeats | Its DMA is in single-block mode and copies the first result 2,000 times | Change the DMA to single-transfer mode and time 2,000 transfers | About 5.0 million samples a second |
| A G3519 DriverLib example carried over: its UART test never finishes, or the build stops at `DL_I2C_ClockConfig`, `ADC12_0_INST_INT_IRQN` or `DL_INTERRUPT_GROUP_1` | The UART example expects a TX to RX wire; I2C is UNICOMM and the ADC is HSADC here | Add `UART1.enableInternalLoopback = true;` to the `.syscfg`; rewrite I2C and ADC code, GPIO, timer, SPI, UART and CRC move as is (`portability/everywhere/PORTING.md` section 7.1) | `rxPacket` reads MSP! |

Traps shared with other boards: TRAPS.md.

## Traps

1. **Never write NONMAIN or the boot configuration (0x80101800).** `driverlib/aesadv_gcm_128_enc_dec`, `driverlib/aesadv_gcm_auto_128_enc_dec`, `driverlib/flashctl_nonmain_memory_write` and the `boot_manager` examples set it. Check an image's map before flashing: `grep -E '^ +(BCR_CONFIG|BSL_CONFIG) ' <image>.map` must show 0 bytes used. Written from the SDK files, not seen on a board (`mspm33c321a/FAQ.md`).
2. **Do not run TI's `qspi_flash_readback` examples.** They erase part of the 16 MB flash that holds the demo video (`mspm33c321a/README.md`).
3. **CoreMark iterations:** pick them so a run lasts 10 to 26 s (the 32-bit cycle counter wraps at 26.8 s at 160 MHz). Used here: 1.6k at 32 MHz and 6k to 9k at 160 MHz (`mspm33c321a/FAQ.md`).
4. **CoreMark per MHz falls from 4.38 at 32 MHz to 3.40 at 160 MHz.** Expected; flash wait states are the suspected cause (`mspm33c321a/FAQ.md`).
5. **The Education BoosterPack covers the serial pins.** This repo's harness keeps results in RAM and reads them over the probe (`mspm33c321a/tools/run_image.py`).

## What fits

All 258 of TI's SDK examples built (the 6 non-secure halves of TrustZone examples after their secure image); 178 of them were flashed and run for 4 s each, and 177 of those left the debugger connected (the exception is trap 3). The four room-occupancy models, 500 to 30k parameters, all fit and match 20 of 20 test vectors (`data/results/mspm33c321a.csv`).

## What ran here

Each number is a row in `data/results/mspm33c321a.csv`.

- Blink: 19 s from first command to pin read; the LED itself was not watched by eye.
- CoreMark -O3: 140.28 per second at 32 MHz (4.38 per MHz), 544.05 at 160 MHz (3.40 per MHz). Dhrystone 1.224 DMIPS per MHz at 32 MHz, 0.876 at 160.
- Occupancy models at 160 MHz, plain C -O3: 0.749 ms (500), 1.856 ms (2k), 7.377 ms (8k), 27.209 ms (30k).
- Post-quantum, TI's examples at 32 MHz: ML-KEM-768 keygen, encaps, decaps 1.48, 1.72, 1.99 million cycles; ML-DSA-44 keygen, sign, verify 2.73, 23.2, 3.47 million; ECDSA P-256 sign 5.21 million.
- FOC step: 531.1 cycles at 160 MHz; 50 kHz control loop 828 cycles mean, 918 worst.
- FreeRTOS task switch round trip: 367 cycles at 32 MHz. 256-point q15 FFT: 21.6k cycles at 160 MHz.
- MicroPython 1.29.0 on Zephyr: banner and REPL, `Pin`, pull-up and pull-down reads, float and `SPI` calls run with MicroPython's own settings (292.3 kB of flash, 83.6 kB of RAM, 48 kB heap) in a Zephyr tree carrying four open TI pull requests; a Python loop pass takes 5.0 µs (803 cycles) against 0.63 µs on the CC2340R5; `machine.ADC` and `machine.PWM` have no Zephyr driver for this chip; I2C reads hang (`ports/micropython/zephyr-mspm33c321a/README.md`).
- Not measured: run and sleep current, UART loop, the touch display.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| 2048 and Minesweeper on the LP-MSPM0G3519 with a 3.5 inch touch display | Works | 2048 and Minesweeper run on the LP-MSPM0G3519 with a 3.5 inch touch display. | `ports/touch-games/README.md` |
| Doom engine (doomgeneric and rp2040-doom's design) on the LP-EM-CC35X1 and the LP-MSPM33C321A | Works | The rp2040-doom design plays all three shareware demos on the LP-EM-CC35X1 and the LP-MSPM33C321A, with frame checksums equal to a Cortex-M33 emulator. | `docs/experiments/2026-10-20-doom-smallest-ti-chip.md` |
| h264bsd video decoder on TI microcontrollers | Works | h264bsd, an H.264 baseline decoder, runs unmodified on the LP-MSPM33C321A and LP-AM13E230 with every frame checksum equal to the Mac's. | `portability/h264/SOURCE.md` |
| liblc3 (LC3, the Bluetooth LE Audio codec) on the LP-MSPM33C321A and LP-MSPM0G3519 | Works | liblc3 (LC3, the Bluetooth LE Audio codec) encodes and decodes frames on the LP-MSPM33C321A with the CRC-32 of every frame equal to the Mac build. | `ports/lc3/README.md` |
| libopus 1.5.2 (Opus, RFC 6716) on the LP-MSPM33C321A | Works | libopus encodes and decodes voice and music on the LP-MSPM33C321A, with packets and PCM equal to a Mac build in 12 of 12 runs. | `ports/opus/README.md` |
| llama2.c (TinyStories 260K, int8) on the LP-MSPM33C321A | Works | llama2.c's TinyStories model writes a short story on the LP-MSPM33C321A, with token ids equal to the Cortex-M33 emulator run. | `ports/llama2c/README.md` |
| MicroPython 1.29.0 on the LP-MSPM33C321A through upstream's Zephyr port | Works | MicroPython's own Zephyr port runs with its stock prj.conf on the LP-MSPM33C321A: a >>> prompt, Pin, float math and SPI from Python. | `data/results/mspm33c321a.csv` |
| minimp3 MP3 decoder on the LP-MSPM33C321A and LP-MSPM0G3519 | Works | minimp3 decodes MP3 clips bit-exact against the Mac on the LP-MSPM33C321A and LP-MSPM0G3519. | `ports/minimp3/README.md` |
| MSPM0 examples on the LP-MSPM33C321A | Works | MSPM0 DriverLib examples run on the LP-MSPM33C321A: 39 of the 40 ported ones ran without a fault. | `portability/everywhere/README.md` |
| Peanut-GB Game Boy emulator on MSPM0 and MSPM33 | Works | Peanut-GB plays Tobu Tobu Girl on the LP-MSPM33C321A with the Educational BoosterPack screen at the console's own speed. | `ports/peanut-gb/README.md` |
| Plaits synthesis engines (Mutable Instruments, MIT) on the LP-MSPM33C321A | Works | All 24 Plaits synthesis engines render bit-exact against the Mac on the LP-MSPM33C321A. | `ports/plaits/README.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| wasm3 (WebAssembly interpreter) on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM33C321A | Works | wasm3 runs a WebAssembly module on the LP-MSPM0G3519 and prints PASS on the serial line. | `ports/wasm3/README.md` |
| Arduboy2-compatible layer and three open-source games on MSPM0 and MSPM33 | Could work | Snake, TileWorld and Crate Confusion build unchanged against an Arduboy2-compatible layer for four kits, and Snake draws on the LP-MSPM33C321A's Educational BoosterPack screen. | `ports/arduboy/FIT.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
<!-- ports:end -->

## Where the evidence lives

`mspm33c321a/README.md`, `mspm33c321a/FAQ.md` and `data/results/mspm33c321a.csv`. If you find something this file gets wrong on your board, the FAQ is where it should be corrected first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- rtos/kernel/blink_led sleeps so deeply that the debug port stops answering (Device Connection Error, low power mode) (`mspm33c321a/FAQ.md`, `mspm33c321a/AGENTS.md`)
- A flash write cut off half-way: the image faults at boot (CFGAP_BOOTDIAG 0x07), the core never stops for the debugger (`mspm33c321a/AGENTS.md`, 2026-10-22)
- An image that writes NONMAIN or the boot configuration at 0x80101800 (`mspm33c321a/FAQ.md`, `mspm33c321a/AGENTS.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset -s <serial> -a toggle -d 200, then flash a normal image (for the deep-sleep lockup) | remote | proven | `mspm33c321a/FAQ.md`, date not recorded |
| xds110reset and a USB replug (for the half-written image) | remote | failed | `mspm33c321a/AGENTS.md`, 2026-10-22 |
| Wait-for-debug (EnableWaitForDebug before connect, or the pwrap.js register sequence), then loadBinaryProgram of the saved main flash (1 MB read-back hashed the same; no mass erase, NONMAIN untouched; also runs from the Digital Lab) | remote | proven | `mspm33c321a/AGENTS.md`, 2026-10-22 |

Restore: CCStudio scripting loadBinaryProgram with FlashEraseSelection Erase MAIN memory only. Regions: main flash at 0x10000000 (1 MB); never NONMAIN or 0x80101800. (`mspm33c321a/AGENTS.md`)

<!-- recovery:end -->
