# Use the code and tools you already have

The text of site/software.html, from `tools/render_software.py` and `site/data/ports.json`. Every port in full: [ports.md](ports.md); code across TI families: [portability](../portability/README.md).

## Tools you already use

- **Zephyr**: Runs in the LP-MSPM0L1306's 4 kB of RAM, with the board file.
- **FreeRTOS**: 11 small tasks fit in the LP-MSPM0L1306's 4 kB of RAM.
- **ESPHome**: With the cc23xx platform, an LP-EM-CC2340R5 joins Home Assistant as a Zigbee device.
- **GCC and make**: TI's MSPM0 DriverLib examples build with GCC from a terminal, and ran on the LP-MSPM0L1306.
- **AI assistants**: Claude Code or Codex, with an AGENTS.md in 25 board folders.

## Bring your code

- **ESPHome Zigbee device on the LP-EM-CC2755P10** (Works): An ESPHome YAML with cc27xx: builds and flashes a Zigbee device that joins Home Assistant on the LP-EM-CC2755P10.
- **ESPHome Zigbee device on the LP-EM-CC2340R5** (Works): The same YAML with cc23xx: runs a Zigbee end device on the LP-EM-CC2340R5.
- **ESPHome Wi-Fi device on the LP-EM-CC35X1** (Works): A Wi-Fi ESPHome device runs on the LP-EM-CC35X1 with ESPHome's native API: a client on the board got 20 of 20 switch commands confirmed.
- **Zigbee smoke and CO alarm on the LP-EM-CC2340R5** (Works): A Zigbee smoke and CO alarm joins Home Assistant as one device on the LP-EM-CC2340R5, with no custom Home Assistant code.
- **Matter smoke and CO alarm over Thread on the LP-EM-CC2755P10** (Works): TI's Matter lighting app rebuilt as a smoke and CO alarm boots on the LP-EM-CC2755P10, exposes the smoke alarm cluster and changes state from a UART line 20 of 20 times.
- **Matter thermostat over Wi-Fi on the LP-EM-CC35X1** (Works): A Matter thermostat and occupancy sensor runs on the LP-EM-CC35X1, pairs into Apple Home and takes setpoint changes from the phone.
- **Nordic's nRF Blinky and UART services on the LP-EM-CC2340R5** (Works): TI's basic_ble with two 8-line service descriptions shows Nordic's LED Button and UART services to a Mac on the LP-EM-CC2340R5.
- **BitNetMCU handwriting AI on MSPM0** (Works): BitNetMCU handwriting models read all 10 test digits right on the LP-MSPM0G3507 and LP-MSPM0L1306, the same answers as on a Mac.
- **BitNetMCU in 1 kB of RAM on the LP-MSPM0C1104** (Works): The 1k handwriting model reads all 10 test digits right in 804 B of RAM on the LP-MSPM0C1104.
- **BitNetMCU on a C2000 control chip (LAUNCHXL-F2800137)** (Works): BitNetMCU's 64-wide CNN reads all 10 test digits right on the LAUNCHXL-F2800137.
- **Arduino display libraries on the LP-MSPM0G3507, and the core layer on the LP-MSPM0G5187** (Works): Arduino_GFX's graphics test draws on an ST7796S screen on the LP-MSPM0G3507, through a 649-line Arduino API layer under the unmodified libraries.
- **h264bsd video decoder on TI microcontrollers** (Works): h264bsd, an H.264 baseline decoder, runs unmodified on the LP-MSPM33C321A and LP-AM13E230 with every frame checksum equal to the Mac's.
- **Post-quantum key exchange and signatures on seven boards: LP-CC1354P10-1, LP-EM-CC2340R5, LP-AM243, LP-MSPM0G3519, LP-MSPM33C321A, LP-EM-CC2755P10 and LP-MSPM0G5187** (Works): ML-KEM-768 key exchange and ML-DSA-44 signatures pass NIST's known-answer vectors on seven boards, from a 36 kB Cortex-M0+ to an 800 MHz Cortex-R5F; the LP-CC1354P10-1 also agrees a key with a Mac.
- **Cmsemicon CMS32 examples on the LP-MSPM0L1306** (Works): Cmsemicon CMS32 examples run on the LP-MSPM0L1306 through one glue layer: 34 of 34 ran; 21 checked, 13 show nothing a probe can see.
- **ST STM32G4 HAL examples on the LP-AM13E230** (Works): ST's STM32G4 HAL GPIO and UART examples run on the LP-AM13E230 through TI's HAL layer, with 3 and 7 lines added to ST's main.c.
- **Five STM32CubeG0 examples through TI's migration skill on the LP-MSPM0G3507: 3 ran as written, 2 after fixes** (Works): TI's migration skill (a tool inside CCStudio that rewrites ST code for a TI chip) moved five STM32CubeG0 examples to the LP-MSPM0G3507: 3 ran as written, 2 after fixes.
- **Sub-1 GHz CC1407P radio on an LP-MSPM0G5187, heard by an LP-CC1354P10-1** (Works): TI's CC1407P sub-1 GHz radio driver runs unchanged on the LP-MSPM0G5187, and an LP-CC1354P10-1 receives its packets: 21,658 of 21,658.
- **Zephyr in 4 kB of RAM on the LP-MSPM0L1306** (Works): Zephyr runs on the LP-MSPM0L1306 in 4 kB of RAM: hello_world, blinky and CoreMark.
- **Zephyr on the LP-AM13E230** (Works): Zephyr's hello_world reaches main and an image toggles the LED on the LP-AM13E230.
- **FreeRTOS in 4 kB of RAM on the LP-MSPM0L1306** (Works): FreeRTOS runs on the LP-MSPM0L1306 in 4 kB of RAM: the blink uses 1 kB.
- **MSPM0 examples moved up and down the family** (Works): MSPM0 examples move between family members with only the SysConfig device changed (SysConfig is TI's pin and peripheral setup tool): all 133 images that built and write nothing outside main flash and SRAM ran on the LP-MSPM0L1306.
- **MSPM0 examples on the LP-MSPM33C321A** (Works): MSPM0 DriverLib examples run on the LP-MSPM33C321A: 39 of the 40 ported ones ran without a fault.
- **MSPM0 DriverLib programs on the LP-AM13E230** (Works): MSPM0 DriverLib GPIO, UART and timer PWM programs run on the LP-AM13E230 with their app code unchanged and a 13-line shim header.
- **MSP430FR2433 code examples on the LP-MSPM0L1306** (Works): Three MSP430FR2433 FRAM examples ran on the LP-MSPM0L1306: a persistent counter, a watchdog wake from LPM3 and a UART echo in LPM3.
- **MCU+ SDK examples on the IWRL6432BOOST radar** (Works): MCU+ SDK examples build in a radar SDK project, and uart_echo and gpio_led_blink ran on the IWRL6432BOOST.
- **A radar frame parser on the LP-AM13E230 and SK-TDA4VM** (Works): A radar frame parser in 882 bytes of C gives the same output on the LP-AM13E230 and SK-TDA4VM as the Mac parser.
- **An MSPM0 FFT example on the SK-TDA4VM** (Works): TI's MSPM0 FFT example runs unedited on the SK-TDA4VM and prints the same peak as on an R5F core and on a Mac.
- **C2000 current-loop code on the LP-AM13E230** (Works): The same current-loop step, trigger to new PWM, runs about twice as fast on the LP-AM13E230 as on the LAUNCHXL-F2800137.
- **TI's MSPM0 DriverLib examples built and run with GCC** (Works): TI's MSPM0 DriverLib examples build and run with GCC from a terminal: all 224 no-RTOS examples with a GCC makefile built unchanged.
- **minmea GPS sentence parser on MSPM0** (Works): minmea, an NMEA GPS parser, runs on the LP-MSPM0G3519 with its files unchanged and a 21-line shim for one missing call.
- **TI AM13 Hall FOC adapted to DRV8376EVM** (Works): TI's Hall FOC firmware drives a motor closed loop through a DRV8376EVM, unloaded at 12.4 V, on the LP-AM13E230.
- **OpenOCD flash driver for the LP-EM-CC2340R5 and LP-EM-CC2755P10** (Works): OpenOCD erases, writes, verifies and restarts a blink on the LP-EM-CC2340R5 over the XDS110, with no CCStudio or UniFlash.
- **ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards** (Works): The ESP32-BLE-Gamepad descriptor and reports match the library's output byte for byte on four boards, and 15 of 15 button changes from the LP-EM-CC2340R5 reached a Mac as gamepad reports.
- **STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards** (Works): MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A.
- **STM32 UART Xmodem bootloader (ferenc-nemeth/stm32-bootloader) on the LP-MSPM0G3519** (Works): ferenc-nemeth's STM32 Xmodem bootloader takes an application over UART on the LP-MSPM0G3519, writes it to flash and jumps to it.
- **DNS ad blocker (sinkhole) on the LP-EM-CC35X1** (Works): The LP-EM-CC35X1 answers DNS on UDP port 53: names on a blocklist get 0.0.0.0 and the rest is forwarded to your gateway.
- **Doom engine (doomgeneric and rp2040-doom's design) on the LP-EM-CC35X1 and the LP-MSPM33C321A** (Works): The rp2040-doom design plays all three shareware demos on the LP-EM-CC35X1 and the LP-MSPM33C321A, with frame checksums equal to a Cortex-M33 emulator.
- **MicroPython 1.29.0 on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM0G5187** (Works): A new MicroPython port boots on the LP-MSPM0G3519 with the MicroPython banner and drives a 3.5 inch touch screen from Python.
- **Peanut-GB Game Boy emulator on MSPM0 and MSPM33** (Works): Peanut-GB plays Tobu Tobu Girl on the LP-MSPM33C321A with the Educational BoosterPack screen at the console's own speed.
- **2048 and Minesweeper on the LP-MSPM0G3519 with a 3.5 inch touch display (also built for the LP-MSPM0G3507 and LP-MSPM33C321A)** (Works): 2048 and Minesweeper run on the LP-MSPM0G3519 with a 3.5 inch touch display.
- **Fixed-weight DSP (FIR, filter bank, DFT) on the LP-MSPM0G5187** (Works): A 64-tap FIR filter runs on the LP-MSPM0G5187's NPU (its neural-network accelerator), compiled with NNC, TI's neural-network compiler, 29.7 times faster than the same graph on the CPU, with byte-for-byte equal output.
- **llama2.c (TinyStories 260K, int8) on the LP-MSPM33C321A** (Works): llama2.c's TinyStories model writes a short story on the LP-MSPM33C321A, with token ids equal to the Cortex-M33 emulator run.
- **wasm3 (WebAssembly interpreter) on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM33C321A** (Works): wasm3 runs a WebAssembly module on the LP-MSPM0G3519 and prints PASS on the serial line.
- **CHIP-8 interpreter and the chip8-test-suite on the LP-MSPM0G3519 touch panel** (Works): A CHIP-8 interpreter runs the chip8-test-suite ROMs on the LP-MSPM0G3519 touch panel, with the frame buffer equal to the host's for five of six.
- **minimp3 MP3 decoder on the LP-MSPM33C321A and LP-MSPM0G3519** (Works): minimp3 decodes MP3 clips bit-exact against the Mac on the LP-MSPM33C321A and LP-MSPM0G3519.
- **liblc3 (LC3, the Bluetooth LE Audio codec) on the LP-MSPM33C321A and LP-MSPM0G3519** (Works): liblc3 (LC3, the Bluetooth LE Audio codec) encodes and decodes frames on the LP-MSPM33C321A with the CRC-32 of every frame equal to the Mac build.
- **Plaits synthesis engines (Mutable Instruments, MIT) on the LP-MSPM33C321A** (Works): All 24 Plaits synthesis engines render bit-exact against the Mac on the LP-MSPM33C321A.
- **libopus 1.5.2 (Opus, RFC 6716) on the LP-MSPM33C321A** (Works): libopus encodes and decodes voice and music on the LP-MSPM33C321A, with packets and PCM equal to a Mac build in 12 of 12 runs.
- **uLisp Lisp interpreter on the LP-MSPM0G5187, LP-MSPM0G3507, LP-MSPM0G3519, LP-EM-CC2340R5, LP-EM-CC2755P10, LP-MSPM33C321A, LP-AM243 and LAUNCHXL-F2800137** (Works): uLisp, an Arduino sketch, gives a Lisp prompt on the serial port of the LP-MSPM0G5187, LP-EM-CC2340R5 and LP-EM-CC2755P10: 19 of 19 REPL (type-and-run loop) checks pass on each.
- **Rust (embassy-mspm0) on the LP-MSPM0L1306** (Works): A Rust blinky and a serial hello run on the LP-MSPM0L1306 with embassy-mspm0.
- **MicroPython 1.29.0 on the LP-EM-CC2340R5 through upstream's Zephyr port** (Works): MicroPython's own Zephyr port runs on the 36 kB LP-EM-CC2340R5: a >>> prompt, with Pin, ADC, PWM, SPI and I2C from Python.
- **MicroPython 1.29.0 on the LP-MSPM33C321A through upstream's Zephyr port** (Works): MicroPython's own Zephyr port runs with its stock prj.conf on the LP-MSPM33C321A: a >>> prompt, Pin, float math and SPI from Python.
- **Arduino sketches on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core: Blink and AnalogReadSerial** (Works): Arduino's stock Blink and AnalogReadSerial run unchanged on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core.
- **Renesas FSP examples (UART, ADC, I2C, GPT timer) on the LP-MSPM0G3519** (Works): Four of five Renesas FSP examples (FSP is Renesas's driver layer) run on the LP-MSPM0G3519 on a layer that maps FSP calls onto DriverLib, TI's register-level driver library.
- **Raspberry Pi Pico SDK examples on the LP-MSPM0G3519** (Works): Raspberry Pi pico-examples (serial hello, I2C bus scan, blink, PWM fade, ADC read) run unchanged on the LP-MSPM0G3519 over a thin layer on DriverLib.
- **Zephyr on the AM62L's Cortex-A53: from on-chip SRAM, and on one core beside Linux** (Works): Zephyr's upstream am62l_evm board runs with no DDR, no TF-A and no TIFS: a 124 KB hello world in the 144 KB of on-chip SRAM, started over JTAG at EL3.
- **TI's Jailhouse hypervisor on the AM62L: bare-metal and Linux cells beside Linux** (Works): TI's documented Jailhouse flow from Linux SDK 12.01.00.05.03 ran as written on the AM62L EVM: the hypervisor enabled in 0.27 s, the uart and gic demo cells printed within a second, and a second Linux reached a login about 20 s after its cell started.
