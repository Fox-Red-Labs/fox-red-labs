# What should an AI coding assistant know before working on a TI LP-EM-CC2340R5 LaunchPad development kit?

That the chip is a CC2340R52 with 36 kB of RAM, not the 64 kB CC2340R53 the board name suggests, and that the board has no USB port, so nothing works until a separate XDS110 probe is clipped on. This file is the short list of what broke and what worked, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-EM-CC2340R5 FAQ](FAQ.md) in the Fox Red Labs repo. In short: the **probe** is the small XDS110 board that clips onto the LaunchPad and connects it to USB; **UniFlash** (`dslite.sh`) is TI's command-line flashing tool; **CCFG** is the chip's boot configuration area at 0x4E020000; **ZHA** is Home Assistant's built-in Zigbee integration; **RCP** is a radio-only co-processor image. 1 kB = 1,024 bytes and 1 GB = 1,024 MB. Checked 2026-10-05 to 2026-10-08 on an Intel Mac, macOS 15.7.9, with SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1, SysConfig 1.26.3, UniFlash 9.6.0, Zephyr 4.4.2, Home Assistant 2026.9.2 with zigpy 2.2.0, on one LP-EM-CC2340R5.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `cc2340r5/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | CC2340R52 (part CC2340R52E0RKPR, read from the factory part ID): Arm Cortex-M0+ at 48 MHz, Bluetooth LE and 802.15.4 radio (`cc2340r5/FAQ.md`) |
| Memory | 512 kB flash, 36 kB RAM. The 64 kB CC2340R53 is on a different board, LP-EM-CC2340R53 (`cc2340r5/FAQ.md`) |
| Debug probe | None on the board and no USB port. An LP-XDS110ET or LP-XDS110 probe clips on and carries power, flashing and serial (`cc2340r5/README.md`) |
| LEDs | Red LED on DIO14 toggles in TI's `drivers/empty` blink; green is `led0` in Zephyr's blinky (`cc2340r5/README.md`, `cc2340r5/FAQ.md`) |
| Serial | `/dev/cu.usbmodem<probe serial>1` at 115200 baud, 8 data bits, no parity; the probe makes two ports and the one ending in 1 is the board's (`cc2340r5/FAQ.md`) |
| Debug access | SWD only. A connection file with SWD off gives `Unable to access the DAP` (`cc2340r5/README.md`) |

## Start here

Tools it was built with: SimpleLink Low Power F3 SDK 9.21.00.36 LTS (`<SDK>`), TI Arm Clang 5.1.1 LTS (inside CCStudio 21.0.1), SysConfig 1.26.3, UniFlash 9.6.0 (`dslite.sh`). Install order and disk sizes: `mac-setup/README.md`.

Probe: an LP-XDS110ET or LP-XDS110 clipped onto the board, TGT VDD jumper on XDS so the probe powers the board. `xdsdfu -e` lists the probe serial; put it in `cc2340r5/CC2340R5.ccxml`, which also picks your probe when two are plugged in. Do not write the `.ccxml` by hand: let CCStudio write it (`cc2340r5/README.md`, gotcha 2).

Backup: no `backup.py` recipe for this board. The recorded commands are DSLite reads, whose length counts 32-bit words, so check the files come out 512 kB and 2 kB (`cc2340r5/oad/README.md`). Keep them outside the repository, they can hold a Zigbee key:

```text
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite; CCXML=cc2340r5/CC2340R5.ccxml; BK=<backup folder>
$DSLITE memory -c $CCXML -r 0x0,0x20000 -o $BK/main_flash.bin
$DSLITE memory -c $CCXML -r 0x4E020000,0x200 -o $BK/ccfg.bin
```

Build (22 s), in `cc2340r5/src/freertos/ticlang`. The SDK's `imports.mak` names folders a Mac lacks, so pass three paths:

```bash
make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

Flash (7 s), from `cc2340r5/`. Avoid `-v` (every image fails verify at the CCFG, though it runs), the `.bin` (1.22 GB, it spans to the CCFG), and `dslite.sh -p` (lists operations, does not talk to the chip). `-e` means verbose here, not erase.

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u src/freertos/ticlang/empty.hex
```

Check: the red LED (DIO14) toggles once a second. `uint32_t time = 1;` on line 57 of `src/empty.c` sets the seconds per toggle; edit, rebuild, flash again.

Restore: merge the backup into one hex, then flash it. The default load erases the chip first, then writes the backup (`cc2340r5/oad/README.md`):

```bash
python3 cc2340r5/oad/images.py merge $BK/restore.hex $BK/main_flash.bin@0x0 $BK/ccfg.bin@0x4E020000
```

```text
$DSLITE flash -c $CCXML -e -f -u $BK/restore.hex
```

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Unable to access the DAP` | Hand-written connection file with SWD off, or the wrong probe | Let CCStudio write the `.ccxml` (README gotcha 2) and add your probe serial | Flash connects |
| `Failed: Connect failed` from `cc23xx-unlock-debug-access` | Probe not fully seated; the unlock mode never recovered a chip here | Pull the probe off, press it on firmly, replug USB, then flash instead of unlocking | Flash connects |
| Zigbee example never joins, nothing printed | TI's example searches channel 11 only | In the `.syscfg` set `zigbee.primaryChannels = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];` and `zigbee.deviceType = ["zr"];` | Home Assistant lists the device |
| Zigbee serial output is unreadable symbols | TI's binary log format | Decode with `tilogger`; change `struct.unpack("L", ts_format)` to `struct.unpack("<I", ts_format)` on line 132 of `uart_to_log.py` | Readable lines |
| `Region Base (0xE6000) is outside the device's usable flash [0x0, 0x80000)` | TI's CC2755 example moved here keeps storage addresses past 512 kB | Set the storage addresses to 0x7C000 and 0x79000 | SysConfig passes |
| ``ld.bfd: region `RAM' overflowed by 346 bytes`` | Zephyr `samples/bluetooth/peripheral` with a 2048 heap on TI's fork | Use `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=1024` | Link passes, sample advertises |
| `section .nvs VMA [0000000000010000,0000000000013fff] overlaps section .text` | `ot-ti` puts settings at 0x10000 | Apply `cc2340r5/thread/ot-ti-local.patch` (settings at 0x7C000) | Link passes |
| `error #10264: NVRAM memory range overlaps existing memory range FLASH` | The DMM linker flash region runs into the Zigbee settings storage | Use `cc2340r5/matrix/dmm_r53_linker.cmd` | Link passes |
| `Failed to send request: device did not respond` in Home Assistant | A flash then a rejoin while the old device record remains | Remove the device in Home Assistant, wait until it is gone, rejoin | Entity appears |
| `R,embench,st,10314129,FAIL` | TI Arm Clang 5.1.1's software double add rounds down about 1 in 36 times | Treat `st` as a compiler result, not a board fault | Other Embench rows pass |
| A Bluetooth link drops (bleak: disconnected) the moment a debugger reads the board | CCStudio scripting and DSLite memory reads halt or reset the core | Release the probe during the link: get results over UART or Bluetooth, read the pin with `DSLite memory -r 0x40023200,1` after the link closes (CCStudio scripting resets the chip, so its read shows the boot state) | Link stays up; DOUT bit 14 follows the LED write (`ports/nordic-apps/host/nordic_nolink.py`, run 2026-11-02) |
| Zephyr pin interrupt counts nothing after a later `pinMode`, `Pin.init` or `gpio_pin_configure` on that pin | Zephyr 4.4.2 `gpio_cc23x0` clears the edge-detect bits on configure | Set the interrupt again after configuring (`portability/arduino/zephyr/README.md`, run 2026-10-25) | ISR count advances |
| Zephyr PWM on the red LED changes no register that reaches DIO14 | Board file's `pwm-leds` uses `<&pwm5 0 ...>`; DIO14 is LGPT1 channel 2 | Use channel 2 (`ports/micropython/zephyr-cc2340r5/README.md`, run 2026-10-25) | LGPT1 C2CC holds the duty |
| MicroPython `PWM(duty_u16=...)` duty far too small at 1 kHz | 32-bit overflow in MicroPython 1.29.0 `machine_pwm.c` | `duty_ns`, or `ports/micropython/zephyr-cc2340r5/pwm-duty-u16-overflow.patch` (run 2026-10-25) | C2CC = duty x 48000 / 65535 |
| An I2C scan on Zephyr finds nothing, but a register read at the same address works | The scan sends an empty write, which fails on Zephyr 4.4.2 CC23x0 I2C driver (cause not traced) | Probe with a one-byte register read | 0x44 answers reads on the Sensors BoosterPack (`portability/arduino/zephyr/README.md`, run 2026-10-25) |
| The Mac lists the Nordic image ("FRL Nordic") but a bleak connect times out, then `system_profiler SPBluetoothDataType` shows `State: Off` | macOS `bluetoothd` stopped by its XPC watchdog during the connect; cause not found (run 2026-10-25) | Turn Bluetooth off and on in Control Center, or `sudo pkill bluetoothd`; remove old pairings of this board ("CC2340R5 Gamepad") in System Settings, Bluetooth | `State: On`, then the connect returns |
| Serial text at the start of a sweep run is from the previous example | The capture starts before the new image loads | Judge a run by its banner or GPIO registers (`cc2340r5/FAQ.md`, sweep 2026-11-08) | Banner matches the example |
| ``make`` cannot find `sysconfig_1.26.3/sysconfig_cli.sh` | That SysConfig version is not installed | Pass `SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh`; the F3 9.21 examples build with it (`docs/experiments/2026-10-24-cc2340r5-cc35x1-driver-twins.md`) | Build passes |
| `undefined symbol RNG_init` after adding the RNG driver to a SysConfig file | The CC23x0 has no general-purpose TRNG and the driver library as shipped has no RNG; TI's RNG driver there draws on radio noise | Use a fixed-seed generator for timing work (keygen and encaps time does not depend on the bytes); say so in the report (`portability/pqc-join/boards/cc2340r5/PORT.md`, run 2026-11-09) | Link passes |
| `use of undeclared identifier 'strcasecmp'` building Arduino-style C++ sketches with TI Arm Clang, and `#include <strings.h>` fails with `cannot initialize return object of type 'char *'` | The compiler's C library header `strings.h` does not compile as C++ | Declare `int strcasecmp(const char*, const char*)` in a small header (`-include`) and define it in a C file (`ports/ulisp/boards/cc2340r5/compat.h`, `board.c`, run 2026-11-09) | uLisp builds and runs |

Traps shared with other boards: TRAPS.md.

## Radios and stacks

RAM left for your code, from each example's own settings (`cc2340r5/matrix/README.md`):

| Stack | What ran | RAM left |
|---|---|---|
| Bluetooth LE, TI SDK `basic_ble` | Advertise, connect, echo | 5.1 kB |
| Zigbee router, TI SDK | Joined Home Assistant; 20 of 20 commands landed | 799 B with logging on |
| Zigbee end device (`onoff_switch`) | Rejoined 15 s after flashing, measured once; device type not confirmed | 6.2 kB |
| Zigbee OTA client | Joined and listed as an end device | 10.7 kB |
| Thread RCP (`ot-ti`) | Flashed and run 2026-10-06: joined the CC1354P10 Thread leader (Mac host drives it) | 8.8 kB |
| Bluetooth plus 802.15.4 (DMM) | Compiles, fails to link on the R52 (2.1 kB short of total free RAM before any heap); links on the R53 | none |
| Full Thread device | Fails to link, 7.5 kB to 23.8 kB over | none |
| Bluetooth on TI's Zephyr fork | Ran with the C library heap on (below) | 1.7 kB for the peripheral sample |
| Matter | TI offers no build for this chip (its Matter fork has CC27xx examples only); not run | |

The two link failures are tagged Unknown, suspected chip limit: only TI's configurations were tried. The R53 builds linked, but nothing was run on an R53 (`cc2340r5/FAQ.md`). Upstream Zephyr 4.4.2 supports GPIO and UART on this board and no Bluetooth.

## What ran here

| Project | Result | Source |
|---|---|---|
| TI blink, FreeRTOS | Built 22 s, flashed 7 s; first run about 9 min, mostly a probe error | `cc2340r5/README.md`, `data/results/cc2340r5.csv` (`time_to_blink`) |
| Zephyr 4.4.2 blinky | 82 s from first command to running | `cc2340r5/zephyr/README.md` |
| CoreMark, bare metal | 111.09 (2.31 per MHz) with TI Arm Clang -O3; 84.07 under Zephyr with GCC | `cc2340r5/baremetal/README.md`, `cc2340r5/FAQ.md` |
| Interrupt latency | 28 to 29 cycles from pend to handler, bare metal | `data/results/cc2340r5.csv` (`irq_latency`) |
| Dhrystone and FFT | 0.602 DMIPS/MHz; 256-point q15 FFT in 1.685 ms | `data/results/cc2340r5.csv` (`dhrystone`, `fft256`) |
| Q15 PID loop | 97.4 kHz at -O2 and 113.5 kHz at -O3 with no missed period in 1,000 | `cc2340r5/FAQ.md` |
| Example sweep | 106 of 108 F3 SDK TI Arm Clang variants build; 34 no-wiring variants flashed and started with no fault, backup restored and read back equal. `build_all.py` names SysConfig 1.26.3, so pass 1.28.0 if that is what is installed (`cc2340r5/SWEEP.md`) |
| PWM check without watching | `DSLite memory -r 0x4006313C,1` (LGPT3 load, 47999) and `-r 0x40063148,1` (channel 2 compare) read back while `pwmled1` ran: compare stepped 0 to 43200 in 4800 steps (`cc2340r5/sweep/pwmled1-lgpt3-readback.txt`) |

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| Arduino sketches on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core: Blink and AnalogReadSerial | Works | Arduino's stock Blink and AnalogReadSerial run unchanged on the LP-EM-CC2340R5 and LP-MSPM0G3519 through the Zephyr-based core. | `portability/arduino/zephyr/README.md` |
| ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards | Works | The ESP32-BLE-Gamepad descriptor and reports match the library's output byte for byte on four boards, and 15 of 15 button changes from the LP-EM-CC2340R5 reached a Mac as gamepad reports. | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| ESPHome Zigbee device on the LP-EM-CC2340R5 | Works | The same YAML with cc23xx: runs a Zigbee end device on the LP-EM-CC2340R5. | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| MicroPython 1.29.0 on the LP-EM-CC2340R5 through upstream's Zephyr port | Works | MicroPython's own Zephyr port runs on the 36 kB LP-EM-CC2340R5: a >>> prompt, with Pin, ADC, PWM, SPI and I2C from Python. | `ports/micropython/zephyr-cc2340r5/README.md` |
| Nordic's nRF Blinky and UART services on the LP-EM-CC2340R5 | Works | TI's basic_ble with two 8-line service descriptions shows Nordic's LED Button and UART services to a Mac on the LP-EM-CC2340R5. | `ports/nordic-apps/README.md` |
| OpenOCD flash driver for the LP-EM-CC2340R5 and LP-EM-CC2755P10 | Works | OpenOCD erases, writes, verifies and restarts a blink on the LP-EM-CC2340R5 over the XDS110, with no CCStudio or UniFlash. | `docs/experiments/2026-10-18-openocd-cc23xx-driver.md` |
| Post-quantum key exchange and signatures on seven boards: LP-CC1354P10-1, LP-EM-CC2340R5, LP-AM243, LP-MSPM0G3519, LP-MSPM33C321A, LP-EM-CC2755P10 and LP-MSPM0G5187 | Works | ML-KEM-768 key exchange and ML-DSA-44 signatures pass NIST's known-answer vectors on seven boards, from a 36 kB Cortex-M0+ to an 800 MHz Cortex-R5F; the LP-CC1354P10-1 also agrees a key with a Mac. | `cc1354p10/pqc-join/README.md` |
| uLisp Lisp interpreter on the LP-MSPM0G5187, LP-MSPM0G3507, LP-MSPM0G3519, LP-EM-CC2340R5, LP-EM-CC2755P10, LP-MSPM33C321A, LP-AM243 and LAUNCHXL-F2800137 | Works | uLisp, an Arduino sketch, gives a Lisp prompt on the serial port of the LP-MSPM0G5187, LP-EM-CC2340R5 and LP-EM-CC2755P10: 19 of 19 REPL (type-and-run loop) checks pass on each. | `ports/ulisp/README.md` |
| Zigbee smoke and CO alarm on the LP-EM-CC2340R5 | Works | A Zigbee smoke and CO alarm joins Home Assistant as one device on the LP-EM-CC2340R5, with no custom Home Assistant code. | `cc2340r5/zigbee-alarm/README.md` |
| Bluetooth media remote (HID) on the LP-EM-CC2340R5 | Could work | A hand-written HID service on TI's basic_ble builds for the LP-EM-CC2340R5, and a Mac sees it as a keyboard-class device and starts pairing. | `ports/ble-hid-remote/README.md` |
| MSPM0 code on the CC2340R5 and CC1354P10 radio chips | Could work | MSPM0 driver examples build for the CC2340R5 with a 47-line shim, the first 5 ran with the same pin states as TI's own build, and 4 of 4 ran on the CC1354P10. | `portability/README.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `cc2340r5/README.md`, then `cc2340r5/FAQ.md` for each problem with its exact error and fix, and `cc2340r5/matrix/README.md` for the stack verdicts. Rows are in `data/results/cc2340r5.csv`. If this file is wrong about your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- The probe is not fully seated: The target failed to see a correctly formatted SWD header (`cc2340r5/FAQ.md`, `cc2340r5/AGENTS.md`)
- An image whose CCFG turns off debug access or chip erase (no image built here did) (`cc2340r5/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Pull the probe off the LaunchPad, press it on firmly, replug USB, then flash (the error stayed through a corrected ccxml and slower clocks until the reseat) | hands | proven | `cc2340r5/FAQ.md`, date not recorded |
| UniFlash cc23xx-unlock-debug-access (Failed: Connect failed, twice, while the probe was badly seated; whether it recovers a locked chip is not known) | remote | failed | `cc2340r5/FAQ.md`, `cc2340r5/AGENTS.md`, date not recorded |

<!-- recovery:end -->
