# What goes wrong with the TI LP-EM-CC2340R5 LaunchPad development kit on a Mac, and how do you fix it?

Every problem met with the TI LP-EM-CC2340R5 LaunchPad (CC2340R52 chip, XDS110 probe) on an Intel Mac, macOS 15.7.9. Each answer starts with the fix and quotes the exact error text so a search finds it. Versions: SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1, SysConfig 1.26.3, UniFlash 9.6.0, Zephyr 4.4.2, Home Assistant 2026.9.2 with zigpy 2.2.0.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | A step in this project's own runs was wrong. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| TI example default | A TI example works on TI's bench but its default does not fit a real home |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The silicon cannot do it, usually RAM |
| Third-party behavior | Home Assistant, zigpy, macOS or upstream Zephyr, not TI |
| Radio environment | The signal between the board and the receiver |
| Unknown | Not found |

Contents: [Connecting and flashing](#connecting-and-flashing), [Memory and CPU](#memory-and-cpu), [Zephyr](#zephyr), [Zigbee and Home Assistant](#zigbee-and-home-assistant), [Over-the-air updates](#over-the-air-updates), [Thread, Matter and two radios at once](#thread-matter-and-two-radios-at-once), [Bluetooth from a Mac](#bluetooth-from-a-mac). 1 kB = 1,024 bytes.

Words used below (more in [docs/vocab.md](../docs/vocab.md)): the **probe** is the small XDS110 board that clips onto the LaunchPad and connects it to USB. **UniFlash** (`dslite.sh`) is TI's command-line flashing tool. **CCFG** is the chip's boot configuration area at 0x4E020000. **ZHA** is Home Assistant's built-in Zigbee integration.

## Connecting and flashing

### Why does UniFlash say "Unable to access the DAP"?

Check two things: that the command reaches the right probe, and that the connection file has SWD on. Let CCStudio write the file. UniFlash's command line needing a hand-made file is a Tool or docs, listed under the next question. The first attempt had three problems stacked. Two probes were plugged in, and it is not known whether the first attempts reached the other board's probe; the same error appeared later with one probe connected. A hand-written `.ccxml` connection file then left SWD off, which this chip needs; that file gave `Unable to access the DAP`, the chip's debug access port (and `invalid processor ID`). UniFlash's command line needs a `.ccxml` file and does not make one, and the errors do not mention SWD. Fixing the file still left the next error, below. The command that makes a correct file in 4 seconds is in `README.md`, gotcha 2.

### Why does flashing fail with "The target failed to see a correctly formatted SWD header" (error -615)?

Pull the probe off the LaunchPad and press it back on firmly. The error stayed through a corrected connection file, a CCStudio-written file, slower clock speeds and two tries of UniFlash's unlock mode (`Failed: Connect failed`). The board still powered up and printed on the serial port, so it looked connected. After the probe was pulled off and pressed back on, and the USB cable replugged at the same time, the first flash worked. Most likely the probe was not fully seated on the debug pins; not proven. `README.md`, gotcha 5. TI's OAD example README gives a second cause, an OAD app flashed without its bootloader: see [the OAD entry](#flashing-the-oad-app-alone-fails-with-error--615-why) under Over-the-air updates.

### `make` says "sysconfig_1.26.3/sysconfig_cli.sh: No such file or directory" building an F3 example

Point `SYSCONFIG_TOOL` at the SysConfig you have installed. With SysConfig 1.28.0 all 22 builds of the 11 Wi-Fi SDK twins in the F3 9.21.00.36 projects passed (2026-10-24, `docs/experiments/2026-10-24-cc2340r5-cc35x1-driver-twins.md`).

### `dslite.sh -p` succeeds but flashing fails. Is the connection fine?

No. `-p` lists flash operations without talking to the chip. Use a real flash or a memory read (`--mode memory`) to test the connection.

### Why does verify fail: "Verification failed: Values at address 0x4E020750 do not match (expected: 0x00, actual: 0xFF)"?

The program still runs; flash without `-v` until the cause is known. It happened on every image flashed with verify on (`-v`): TI SDK builds and upstream Zephyr builds alike. The address is the first byte of the CCFG user record, a small area for application data (layout in the SDK's `hw_ccfg.h`). Most later flashes skipped verify, so it is not known whether every image shows it.

### DSLite `memory -r 0x0,0x20000` wrote a 512 kB file. Is the length in bytes?

No: DSLite `memory -r` counts the length in 32-bit words (0x20000 gives 512 kB, the whole 512 kB flash). The length was read as bytes twice, and the first backup read ran past the end of flash. Try a small read first (`0x0,0x10` is 16 words, so the file should be 64 bytes) and check the file size. The CCFG read `0x4E020000,0x200` gives 2 kB. UniFlash 9.6.0. `oad/README.md`, step 1.

### Can you flash two boards from one Mac?

Yes. Put each probe's serial number in its connection file (`xdsdfu -e` lists them). The command line does not say which probe it used.

### Does the LP-EM-CC2340R5 have a USB port, or do you need a separate probe?

You need a separate probe. The board has no USB port of its own; the LP-XDS110ET or LP-XDS110 probe clips on and provides power, flashing and the serial port. Order it with the board.

### Which chip is on your LP-EM-CC2340R5: CC2340R5, CC2340R52 or CC2340R53?

The CC2340R52 (36 kB of RAM). The board name says R5 and the chip is an R52; the 64 kB CC2340R53 is on a different board, the LP-EM-CC2340R53. To check yours, read the 4-byte part ID at 0x4E00007C with `dslite.sh --mode memory`. This board read 0x803B2DDA; drop the top byte (a valid flag and revision bits) and look up 0x3B2DDA in the SDK's `source/ti/devices/cc23x0r5/driverlib/chipinfo.h`: CC2340R52E0RKPR.

### Can flashing lock or brick a CC2340R5?

It can, if an image turns off debug access or chip erase in its boot configuration (CCFG). The lock is a chip safety feature set by the image you flash. The permission fields of the images built here (the Zephyr builds, the Zigbee and other TI example builds, the bare-metal benchmark, TI's Zephyr fork, and the DMM and Thread builds) were read; all allowed both. The lock fields of TI's prebuilt dual-image MCUboot hex were read too: debug port and chip erase both allow, debug authorization open, no sectors protected. TI's prebuilt `basic_ble_app.hex`, the other MCUboot builds, and the Zigbee OTA client images were not checked. Upstream Zephyr prints a "LOCK YOUR DEVICE" warning when its settings would lock the chip; TI's fork and SDK builds have no such check, so read the fields yourself. Check `ti_devices_config.c` (TI SDK) or the built devicetree (Zephyr) for `allowDebugPort` and `allowChipErase` before flashing anything new.

### Why is your `.bin` file 1.22 GB?

The boot configuration area sits at 0x4E020000, so a raw binary from address 0 spans 1.22 GB (1.2 GB). It is mostly empty (a sparse file). Flash the `.hex`, never the `.bin`.

### What do you need to buy and install to start?

The LP-EM-CC2340R5 board, an LP-XDS110ET (or LP-XDS110) probe, and a USB cable for the probe. Software: TI's SimpleLink Low Power F3 SDK (0.55 GB download, 2.0 GB installed plus 2.2 GB of extras, 4.8 GB free needed while installing; <https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK>), UniFlash, and TI Arm Clang, which comes with CCStudio. A free myTI account and a short export form are needed for TI downloads. The probe is sold separately. Sizes and links: `mac-setup/README.md`. Prices change; check ti.com.

### Does this work on an Apple Silicon Mac, Windows or Linux?

Not tested. Everything here ran on an Intel Mac with macOS 15.7.9. Several fixes are Mac or Intel specific (the `tilogger` byte-size bug also affects Linux; the `lief` install failure was on Intel macOS).

### Which serial port and speed show the board's output?

`/dev/cu.usbmodem<probe serial>1` at 115200 baud, 8 data bits, no parity. The probe creates two ports; the one ending in 1 carries the board's serial output. Some TI examples send binary logs, not text (see `tilogger` below).

### The SDK's `make` cannot find the compiler or SysConfig

Pass the three paths on the `make` command line: `SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR`, `SYSCONFIG_TOOL` and `TICLANG_ARMCOMPILER`. The SDK's `imports.mak` points at a CCStudio 20.4 folder and a placeholder user name. The full command: `README.md`, "Build".

### The serial port shows lines from the program you flashed before

Read and discard what is waiting before you trust the output, or reset the board after opening the port. The previous program's lines appeared right after flashing a new one; where they were held was not checked.

### How do you recover a CC2340R5 that will not connect after a bad flash?

First reseat the probe (see "Why does flashing fail with 'The target failed to see a correctly formatted SWD header'" above). UniFlash also has a `cc23xx-unlock-debug-access` mode for chips whose debug access is turned off. It was run twice while the probe was badly seated and returned `Failed: Connect failed`; after the reseat it was not needed, so it is not known whether it recovers a locked chip.

### How long does box to blinking LED take when nothing goes wrong?

About 30 seconds of machine time once the tools are installed: building TI's blink example took 22 seconds and flashing 7 seconds; Zephyr's blink took 82 seconds from the first command, build included. The first run took about 9 minutes of waiting because of the connection problems above. Installing the tools is separate: `mac-setup/README.md`.

### My new board is already blinking or running something. Is that expected?

This board arrived running a previous owner's program, which printed on the serial port. Flashing your program replaces it; nothing needed erasing first.

### Can you do all this inside CCStudio instead of the terminal?

Probably, but it was not run on this board. CCStudio was used only to write the connection file. On the MSPM0 board in this project, CCStudio's AI tools flashed a blink in 2 minutes 29 seconds (`mspm0g3507/`).

### Can you debug the CC2340R5 with GDB or OpenOCD from a Mac?

Not with the tools used here. The prebuilt OpenOCD used for the MSPM0 board has no CC23xx support, and upstream Zephyr's flash setup for this board knows only J-Link. CCStudio's debugger should work (it wrote the connection file); it was not used to debug.

### Which LED should blink?

Red for TI's SDK blink example; green for Zephyr's blinky, whose board file calls the green LED `led0`. A different color is a quick way to tell which program is running.

### How do you put TI's own Bluetooth demo back on the board?

Flash TI's prebuilt image from the SDK: `examples/rtos/LP_EM_CC2340R5/ble/prebuilt_hexfiles/basic_ble_app.hex`, with the UniFlash command in `README.md`. TI's source names it "Basic BLE project" (`ble.deviceName` in the example's `basic_ble.syscfg`); not checked on the prebuilt file. Which program a new board ships with is not known; this one arrived running a previous owner's.

## Memory and CPU

### How much RAM does the LP-EM-CC2340R5 have? The product page says 64 kB

36 kB. The board carries the CC2340R52 (part CC2340R52E0RKPR, read from the chip's factory part ID). TI's data sheet SWRS272F (May 2026) lists 36 kB of SRAM for the R52 and 64 kB for the CC2340R53, which is sold on its own board, the LP-EM-CC2340R53. The product page says "up to 64KB" because it covers both.

### Is the CC2340R5 a Cortex-M33?

No, a Cortex-M0+ at 48 MHz, confirmed from the chip's CPU ID register (0x410CC601). TI's M33 wireless parts are the CC27xx (CC2755P10: Cortex-M33 at 96 MHz, 162 kB RAM). CoreMark on this chip: 111.09 with TI Arm Clang at -O3 (2.31 per MHz), 84.07 under Zephyr with GCC.

### How much RAM is left for your code after the radio stack?

With each example's own settings: Bluetooth (`basic_ble`) 5.1 kB; Zigbee end device (on/off switch, logging on) 6.2 kB; Zigbee OTA client (logging off) 10.7 kB; Zigbee router with logging on 799 bytes; Thread radio co-processor (TI's `ot-ti`, after a settings-area fix that it needed to link) 8.8 kB. With TI's Zephyr fork, the Bluetooth peripheral sample leaves 1.7 kB. These are the linker's figures for each example's own settings; your own build will differ. Per-stack figures: `matrix/README.md`.

### Why are your Zephyr timings 48 times off on the CC2340R5?

Upstream Zephyr's config says the cycle counter runs at 48 MHz; the driver reads a 1 MHz register. This is upstream Zephyr 4.4.2, `drivers/timer/cc23x0_systim_timer.c`. Read `SYSTIM TIME250N` (4 MHz) instead; `mspm0g3507/mcu-bench/src/main.c` shows how. The same board file also lists 80 kB of RAM; the chip has 36 kB.

## Zephyr

### On Zephyr, a pin interrupt stops firing after the pin is configured again. Why?

If you call `gpio_pin_configure` (Arduino `pinMode`, MicroPython `Pin.init`) on a pin after setting its interrupt, set the interrupt again. Zephyr 4.4.2's `gpio_cc23x0` driver rewrites the pin's IOC register without the edge-detect bits, so the edge setting is lost (IOC10 read 0x60034000 before and 0x60004000 after). Measured on S1 (DIO10) 2026-10-25: 0 of 10 edges counted after the reconfigure, 10 of 10 when only the pull field was changed. `portability/arduino/zephyr/README.md`, `ports/micropython/zephyr-cc2340r5/README.md`.

### On Zephyr, PWM on the red LED (`pwm5`) does nothing. Why?

Use timer channel 2: `<&pwm5 2 ...>` in devicetree, `machine.PWM(("pwm5", 2), ...)` in MicroPython. The red LED pin is muxed to LGPT1 channel 2 (`DIO14_T1_C2N`), and the cell after `&pwm5` is the timer channel. Zephyr 4.4.2's board file gives channel 0 for the red PWM LED; channel 0 wrote LGPT1's C0 compare register and left C2 alone (register read-back, 2026-10-25). The green LED (`pwm6`, channel 0) matches its pin. The LED itself was not watched. `ports/micropython/zephyr-cc2340r5/README.md`.

### In MicroPython, `PWM(..., duty_u16=16384)` at 1 kHz gives a much smaller duty than asked. Why?

Use `duty_ns`, or apply `ports/micropython/zephyr-cc2340r5/pwm-duty-u16-overflow.patch` to the MicroPython tree before building. MicroPython 1.29.0's `machine_pwm.c` multiplies `duty_u16` by the period in nanoseconds in 32 bits, which overflows for any frequency under about 15.3 kHz at full duty. Measured 2026-10-25: compare register 2,562 instead of 12,000 for 16,384 at 1 kHz; 12,000, 36,000 and 48,000 for 16,384, 49,152 and 65,535 with the patch. The LED was not watched. `ports/micropython/zephyr-cc2340r5/README.md`.

### On Zephyr, an I2C scan finds nothing, but a register read at the same address works. Why?

Probe an address with a one-byte register read (Arduino `Wire.requestFrom`, MicroPython `readfrom_mem(addr, reg, 1)` inside `try`) instead of an empty write. The scan sends an empty write to each address, and Zephyr 4.4.2's CC23x0 I2C driver returns an error for it even when a device is there. Seen 2026-10-25 with the Sensors BoosterPack seated: the scan found 0 devices (Arduino `Wire`) and `[]` (MicroPython), while a read at 0x44 returned the OPT3001 ID. The board has no I2C pull-ups, so the BoosterPack must supply them. `portability/arduino/zephyr/README.md`, `ports/micropython/zephyr-cc2340r5/README.md`.

### On Arduino-on-Zephyr, `Serial.println` output has most newlines missing at 9600 baud. Why?

Apply `portability/arduino/zephyr/serial-blocking-write.patch` to the ArduinoCore-zephyr checkout. The core's `ZephyrSerial::write()` copies bytes into a 64-byte ring buffer and drops what does not fit; AnalogReadSerial writes about 5 bytes every millisecond and 9600 baud carries about 1. With the patch (a loop that waits for room) 612 clean lines arrived in 3 s. The first line of output is the 115200 boot banner read at 9600; ignore it. `portability/arduino/zephyr/README.md`.

### On Arduino-on-Zephyr, `SPI.transfer(buf, n)` returns zeros or leaves stale bytes in the buffer. Why?

Call `SPI.transfer(byte)` in a loop. ArduinoCore-zephyr `af5b8610f9be` runs `spi_write` and then a separate `spi_read`, so the buffer receives the second transfer's input instead of the bytes clocked in during the first. Measured on the LP-MSPM0G3519 (`transfer(buf, 16)` returned 0 of 16 correct); the cause is in the core, so it applies here too, but a buffer transfer was not run on this board. `portability/arduino/zephyr/README.md`.

### Why does upstream Zephyr's `west flash` not work for `lp_em_cc2340r5`?

Upstream Zephyr's flash setup for this board knows only the J-Link probe. Build with `west build`, then flash `build/zephyr/zephyr_crc32.hex` (not `zephyr.hex`) with UniFlash. `zephyr/README.md`.

### Does upstream Zephyr support Bluetooth on the CC2340R5?

No. Zephyr 4.4.2 supports GPIO and UART on this board. TI's own fork (`TexasInstruments/simplelink-zephyr`) has a Bluetooth driver.

### Images from TI's Zephyr fork flash fine but never run. Why?

For a Bluetooth build, turn on the C library heap first: see the next question, "On the LP-EM-CC2340R5, a Bluetooth build from TI's Zephyr fork builds and flashes but shows no serial output and no advertising. Why?"; with the heap on, the project's beacon and TI's peripheral sample ran with the fork's own boot configuration. On 2026-10-05, before the heap was found, `hello_world` printed its boot banner only after one boot-configuration byte was changed (CCFG byte 0x4E02001C, 0x0F in the fork, 0x07 in upstream Zephyr 4.4.2) and recomputed all four checksums with upstream Zephyr's `soc/ti/simplelink/cc23x0/ti_ccfg_crc32_calculate.py` (serial capture not saved). On 2026-10-08 the Bluetooth images ran with 0x0F, so that change is not needed for them; why hello_world needed it was not rechecked. Tests ran only with the probe attached. `docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md`.

### On the LP-EM-CC2340R5, a Bluetooth build from TI's Zephyr fork builds and flashes but shows no serial output and no advertising. Why?

Turn on Zephyr's C library heap: add `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` to `prj.conf`, or 1024 for TI's `samples/bluetooth/peripheral`, which does not link at 2048. The heap is the memory a program asks for while it runs, through `malloc`. The fork's settings for this board leave it at 0 bytes, so every request fails, and the debugger's evidence reads as TI's Bluetooth stack failing to get memory for its first timer.

To see the value your build used, run `grep MALLOC_ARENA build/zephyr/.config`. At 2048 the peripheral sample fails to link with this line:

```text
ld.bfd: region `RAM' overflowed by 346 bytes
```

How it was found, 2026-10-08: halted with the debugger, the program sat in a loop at line 146 of the fork's `drivers/bluetooth/hci/hci_ti.c`. It was called from `halAssertHandlerExt`, TI's Bluetooth assert handler (the function the stack calls when one of its own checks fails; it then waits for a debugger). Below it on the stack were `ICall_abort`, `ICall_setTimer` and the timer callback `osal_msec_timer_cback`, and `ICall_setTimer` gets its memory from `malloc`. That is a reading of the call chain; the allocation was not stepped through.

With the heap on and nothing else changed, the BTHome beacon and TI's peripheral sample advertised and were seen from a Mac, with the fork's own boot-configuration byte (CCFG byte 0x4E02001C at 0x0F). TI simplelink-zephyr `v3.7.0-ti-1.00.01_ga`, GCC 12.2 (Zephyr SDK), UniFlash 9.6.0. Page: [the experiment](../docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md).

### TI's Zephyr fork beacon sample prints "Beacon started" and then `***** HARD FAULT *****`. Why is it not seen on air when the peripheral sample is?

The cause is not known. For a Bluetooth starting point on this fork, use `samples/bluetooth/peripheral` or this repo's BTHome beacon (`ports/bthome/zephyr/`), which both ran. With the heap at 2 kB, `samples/bluetooth/beacon` printed `Bluetooth initialized` and `Beacon started, advertising as 40:F3:B0:xx:xx:xx (public)`, then `>>> ZEPHYR FATAL ERROR 0: CPU exception on CPU 0` with `Faulting instruction address (r15/pc): 0x0000bf3c`, 0.122 s after boot; one scan from the Mac found 0 devices named "Test beacon". Not chased: the address was not mapped to a function. 2026-10-08, TI simplelink-zephyr `v3.7.0-ti-1.00.01_ga`.

### TI's Zephyr fork peripheral sample prints `Unable to send 0x2025 to driver (err -2)`. Can you ignore it?

For advertising, yes: the sample went on to print `Bluetooth initialized` and `Advertising successfully started`, and the Mac saw it. The lines at start are `<err> bt_ctlr_hci_driver: Failed to send buffer to device`, then `<err> bt_hci_core: send_cmd: Unable to send 0x2025 to driver (err -2)`. 0x2025 is the Bluetooth command that reads the chip's public key, used for secure pairing; pairing was not run, so whether it works is not known. 2026-10-08, heap 1 kB, TI simplelink-zephyr `v3.7.0-ti-1.00.01_ga`.

### Which file do you flash: `zephyr.hex` or `zephyr_crc32.hex`?

Upstream Zephyr 4.4.2: `zephyr_crc32.hex`, which has the boot checksums filled in. TI's Zephyr fork: `zephyr.hex`, because the fork writes the checksums into it. TI's fork changed the file name from upstream's (upstream's own naming is Third-party behavior), and neither project's page says so.

### TI's Zephyr fork build stops with `crc_tool: command not found` or fails installing `lief`

Install TI's `ti-simplelink-crc-tool` without its pinned dependencies and use a current `lief` wheel: `pip install --only-binary=:all: lief`, then `pip install --no-deps <the tool's wheel>`. The tool pins `lief` 0.12.3, which has no ready-made package for Python 3.12 on an Intel Mac and failed to build from source (the exact pip error was not saved). `crc_tool: command not found` appears after a good link when the tool is missing. Details: `zephyr-ti/README.md`, step 5.

### TI's Zephyr fork build fails calling objcopy at a fixed path

The fork's CC23xx post-build step calls objcopy inside a Zephyr SDK folder that may not exist. Change it to use the toolchain's objcopy (`${CMAKE_OBJCOPY}`). The exact error was not saved. Setting `ZEPHYR_SDK_INSTALL_DIR` to work around it does not help: it starts a Zephyr SDK lookup that fails (**Our mistake**, the first try). Details: `zephyr-ti/README.md`, step 6.

### TI's Zephyr fork says `west flash` supports only J-Link

Flash `zephyr.hex` with UniFlash instead (command in `README.md`). The fork's README for this board says "only JLink is available", and TI sells this board with an XDS110 probe.

### Building TI's Zephyr fork stops in Kconfig after you trimmed the modules

Keep `mbedtls` and `tinycrypt` in the module list (`west config manifest.project-filter -- '-.*,+simplelink-hal_ti,+cmsis,+mbedtls,+tinycrypt'`). The first filter left them out to save disk space, and Bluetooth needs both.

### Does the Zephyr form of the BTHome beacon run on the CC2340R5?

Yes, with the C library heap on: built on TI's Zephyr fork with `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` (110.5 kB of flash, 30.5 kB of RAM), it ran on 2026-10-08 and the Mac decoded its BTHome v2 packets as "FRL Temp" at -35 dBm. The tag is for the failure with the heap at 0, the fork's setting: it printed nothing and sent nothing (2026-10-07), like the fork's own Bluetooth samples; the answer above gives the cause as far as it was traced. Experiment: [docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md](../docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md).

## Zigbee and Home Assistant

### TI's Zigbee example never joins your Home Assistant network, and there is no error

The example searches channel 11 only. In the example's `.syscfg`, set `zigbee.primaryChannels = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];` and, to join an existing network, `zigbee.deviceType = ["zr"];` (router) instead of the default coordinator. `zigbee/README.md`.

### The Zigbee example's serial output is unreadable symbols

It is TI's binary log format, not text. Decode it with TI's `tilogger` from `tools/log/tiutils` in the SDK, after the fix below.

### `tilogger` prints "unpack requires a buffer of 8 bytes" and "'NoneType' object has no attribute 'parse_native'"

Change `struct.unpack("L", ts_format)` to `struct.unpack("<I", ts_format)` on line 132 of `tilogger_uart_transport/uart_to_log.py`. `"L"` is 4 bytes on Windows and 8 on macOS and Linux. Run it as `PYTHONUNBUFFERED=1 tilogger --elf <your .out file> uart /dev/cu.usbmodem<probe serial>1 115200 stdout`; without `PYTHONUNBUFFERED=1` the output is lost when you stop it. (The first diagnosis blamed an empty log section in the build. That was wrong. **Our mistake.**)

### My TI device shows up in Home Assistant as "unk_manufacturer unk_model"

The example leaves the manufacturer and model names empty. Harmless. The IEEE address starting `00:12:4b` is TI's.

### After reflashing, Home Assistant says "Failed to send request: device did not respond"

Remove the device in Home Assistant, wait until it is gone from the device list, then let it join again. The device was rejoined too fast, and Home Assistant kept a stale record of the old join. Why the first rejoin failed to answer at all is **Unknown (suspected: the chip erase resets the board's network security counters while the coordinator remembers the old ones).**

### Why does your Zigbee board answer in 5 seconds instead of under 100 ms?

Not explained. The same image on the same desk answered in 95 ms median after its first pairing, and 3.5 to 7 seconds after re-pairing about two hours later, while the coordinator reported a weaker link: RSSI -85 dBm at the first pairing, -89 to -91 dBm in the readings right after re-pairing (the full range that evening was -88 to -91; observed 2026-10-05, not saved). Measure more than once.

### How do you make a TI Zigbee example forget your network?

Probably by reflashing, but this was not checked. What was done: remove the device in Home Assistant, wait until it is gone, then reflash. UniFlash's settings for this chip erase the whole chip during every program load by default, which should clear the stored network; that was not checked directly, and the example's reset button was not tested. Note: in UniFlash's `dslite.sh`, `-e` means `--verbose`, not erase (in TI's `xdsdfu` tool, `-e` lists probes).

### Does the CC2340R5 work as a Zigbee end device in Home Assistant?

It rejoined the test network 15 seconds after flashing. Home Assistant kept the earlier router record, so the device type and sleep behavior were not confirmed. The OTA client example, also an end device, joined and showed up as one.

### Will TI's Zigbee examples work with Zigbee2MQTT?

Not tested. Only Home Assistant's ZHA with a Nabu Casa ZBT-2 was used.

### How do you find which channel your Zigbee network uses?

Home Assistant shows it in the Zigbee integration's network settings. It can be read through Home Assistant's API (`zha/network/settings`); the test network is on channel 20. TI's examples need it in `zigbee.primaryChannels`, or all 16 channels.

### How do you make a TI Zigbee end device keep its receiver on?

Change it in C: in the example's main file, `zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);` becomes `zb_set_rx_on_when_idle(ZB_TRUE);`, and skip the polling setup that follows. Adding `zigbee.powerModeOperation = "alwaysOn";` to the `.syscfg` built without complaint and changed nothing (TI's metadata marks the option read-only by default), and a `-DED_RX_ALWAYS_ON=ZB_TRUE` compiler flag did not change the built file either. The OTA tests used this.

### TI's CC2755 example moved to a CC2340R5 fails with a flash address out of range

Move the storage addresses inside the chip's 512 kB. SysConfig reported `Region Base (0xE6000) is outside the device's usable flash [0x0, 0x80000)`: the example's Zigbee and Bluetooth settings storage sat at CC27xx addresses (0xE6000 and 0xE3000). The port used 0x7C000 and 0x79000 (0x7E000 for the CC2340R53 build).

### Which Zigbee coordinators work with TI's examples?

Only a Nabu Casa ZBT-2 with Home Assistant's ZHA was tested. Other coordinators and Zigbee2MQTT were not run.

### The same `onoff_light` image that joined on 2026-10-05 never joined on a rerun. Why?

Not solved. On 2026-10-16 the image rebuilt from the same source was flashed or reset three times, each with ZHA joining open for about 4 minutes and the device removed from ZHA first once. It never appeared in ZHA's device list. As a control, the Zigbee alarm image (an end device) joined the same network in 31 s and then 7 s, with the coordinator and three routers up. The light's own log differed between tries: in one, "Device (1) STARTED OK" and "Successful steering" 6.2 s after reset; in another, steering was cancelled at 6.3 s (ZBOSS signal 55) and the stack reported no active links at 16 s (signal 24). Not checked: what cancels steering, and the coordinator's own log. Until then, treat the join as seen once. `zigbee/README.md`.

### A Zigbee smoke or CO alarm joins Home Assistant but its sensors never change

Remove the device in ZHA, wait until it is gone, open joining and flash again. ZHA did not enroll the zone (zone state 0). One of three scripted re-pairings never showed up within two minutes after a removal that was only seconds old; the suspected cause is pairing before ZHA finished the removal. Entities with no device class have the same cause: the zone type was not read, so reconfigure the device in ZHA. `zigbee-alarm/README.md`.

### The Zigbee alarm stays on "smoke" after `SMOKE 0`

The image sends its own zone status change notification. ZBOSS's `zb_zcl_ias_zone_set_status()` notifies only when an alarm bit is set; clearing it returns false and sends nothing. The project's `alarm.c` sets the attribute and sends the notification itself, to the address ZHA wrote. `zigbee-alarm/README.md`.

### The board stops answering when ZHA writes the alarm's CIE address

Declare the IAS Zone attributes with the extended list, `ZB_ZCL_DECLARE_IAS_ZONE_ATTRIB_LIST_EXT`. With the short list the board stopped responding at that write; with the extended one it did not. The cause is inside TI's closed library and was not checked; the suspect is that the short list has no internal context entry. `zigbee-alarm/README.md`.

### The Zigbee alarm goes silent after an idle minute

Keep the receiver on: build without `zb_sleep_now()` and without the sleepy power mode. With `zigbee.powerModeOperation = "sleepy"` and `zb_sleep_now()` the board joined and 10 of 10 changes landed right after the join, then it stopped answering ZHA's reads and stopped hearing serial lines. The suspect is that the UART receiver is off in standby; a product would wake the chip with a GPIO line first. Current was not measured. `zigbee-alarm/README.md`.

## Over-the-air updates

### Home Assistant never offers an update for TI's OTA client example

The example declares the OTA cluster in its code but leaves it out of the endpoint descriptor Home Assistant reads. zigpy then logs "Ignoring message on unknown cluster: 0x0019". Replace the endpoint declaration with one that lists OTA Upgrade (0x0019): `matrix/ota_endpoint.patch.c`.

### Where is `zOTAfileGen` in the F3 SDK?

It is not there; the example README points at the CC13xx/CC26xx SDK. `matrix/make_zigbee_ota.py` writes the standard Zigbee OTA file header (manufacturer 0xBEBE, image type 0x2340 for this example). zigpy's parser accepted it.

### You put a new image in Home Assistant's OTA folder and nothing is offered

zigpy caches the folder for 24 hours. Reload the ZHA integration, for example through Home Assistant's API (`config/config_entries/entry/<id>/reload`), which took 6 seconds. The folder is set with the `advanced` provider under `zha: zigpy_config: ota: extra_providers:` in `configuration.yaml`.

### The Zigbee update starts and stalls at about 1%

Unsolved. Four transfers stopped at 0.32% to 1.34% of the 200.1 kB image. As TI's example ships it is a sleeping end device (checks for messages every 3 seconds), and block requests sometimes came 17 seconds apart; with the receiver forced on in C it reached about 150 bytes a second, then hit `ZIGBEE_DELIVERY_FAILED: 3074`. `docs/experiments/2026-10-05-cc2340r5-zigbee-ota-home-assistant.md`.

### Can you update the LP-EM-CC2340R5 over Bluetooth from a Mac?

Yes, with this project's `tools/oad/oad_client.py` (Python and the bleak library) and TI's dual-image OAD example: one run on 2026-10-07 sent the 236 kB image in 31.4 s (61.5 kbit/s, ATT MTU 251), the board advertised its new name 7.8 s after the enable command, and version 2.0.0 read back. TI's own senders are a phone app (SimpleLink Connect) and a Windows program; the SDK has no Mac or Linux sender. One run only, and the LED was not watched. Steps: `oad/README.md`.

### TI's OAD guide and the SDK source disagree. Which one is right?

The source: a client written from the guide does not work, one written from `oad_profile.c`, `oad_service.c` and `sw_update.c` does. Eight differences turned up; the table "Where TI's guide and the SDK source disagree" in `oad/README.md` lists them all. The block size is always 240, not chosen from the packet size; block requests arrive on 0xFFC5, not 0xFFC2; the Enable Image command (`04`) that every update needs is missing from the guide's command table; "download complete" carries the block count, not 0xFFFFFFFF; block size, version and status commands are answered before image identify, not refused; the software-version reply is 7 bytes, not 5; the header's image size leaves out the header and signature, so a client must send the real file length; and the guide's header table names two fields wrongly. Guide: BLE5-Stack User's Guide in F3 SDK 9.21.00.36, OAD chapter.

### The OAD client stops with "ATT MTU ... carries at most ... bytes a write, the target wants 240"

The Mac and the board agreed a packet size too small for one 240-byte block, so the client stopped before sending any of the image; nothing on the board changed. TI's OAD source fixes the block at 240 bytes and ignores the offset of a long write, so the link needs an ATT MTU of 243 or more; the guide says the block size follows the packet size. The full message, from `tools/oad/oad_client.py`, is `ATT MTU <mtu> carries at most <mtu - 3> bytes a write, the target wants 240. The target ignores the offset of a long write, so this would corrupt the image. Stopping.` What to try: check that the board's image allows a large packet (TI's dual-image example sets `maxPDUSize = 255` in its `.syscfg`, and the Mac then agreed 251), then reconnect; on a Mac, macOS picks its side of the packet size, so another Mac may agree a larger one (not tested). The stop appeared only with the software stand-in `tools/oad/sim_target.py` set to 185; the board run agreed 251. `oad/README.md`, "The protocol".

### Which OAD flavour fits the LP-EM-CC2340R5?

Dual image (`basic_ble_oad_dual_image`): your app keeps running during the download and only two files go on the chip. The board has no external SPI flash fitted, so off-chip OAD does not fit it as shipped; on-chip OAD fits but erases your app during the download and needs a second "persistent" app. Dual image halves the room for the app: each slot is 236 kB, and TI's Bluetooth app uses 192 kB. Multi-step OAD needs a bootloader built with a different option and was not run. `oad/README.md`.

### The OAD linker file lets the app run 2 kB into the download slot. Is that a problem?

Not for a build that `imgtool` accepts, because `imgtool` refuses an image too large for its slot. The dual-image example's linker gives the app 0x3B700 bytes from 0x6100, which ends at 0x41800, 2 kB past the 0x41000 start of the download slot. The app used 192 kB of it (from the linker map), far from the edge. Why TI sized it that way was not found. `oad/README.md`.

### TI's prebuilt MCUboot hex has all its CCFG checksums set to zero. Will it boot?

Yes, if you flash it with UniFlash's default settings, which rewrite the checksums while flashing; do not turn "Overwrite CRC fields in CCFG" off. All four checksums in `mcuboot_dual_image.hex` read 0x00000000. On 2026-10-07 UniFlash logged `Updating CRC32 field ccfg.crc32 @ address 0x4E02074C, based on data in the range [0x4E020010, 0x4E02074B]. Value changes from 0x00000000 to 0xB714DFB7` and the board booted. UniFlash 9.6.0's settings file skips the user record checksum by default, which may explain the verify mismatch at 0x4E020750 above; not tested.

### Flashing the OAD app alone fails with error -615. Why?

Flash TI's MCUboot and the app in one command, as in `oad/README.md`, step 2. The dual-image app starts at 0x6000 and needs MCUboot at 0x0 to start it; TI's example README says the probe reports error -615 when the app is flashed without it. This was not run. The other cause of -615, a probe not fully seated, is under Connecting and flashing.

## Thread, Matter and two radios at once

### Can the CC2340R5 be a Thread end device or router?

Not with TI's configurations, built with GCC 12.2 rather than TI's tested toolchain, on the CC2340R52. TI's `ot-ti` lists it as a radio co-processor (RCP) only. With the board check removed, every node build compiles and then fails to link with messages like `region 'SRAM' overflowed by 10216 bytes` (the linker writes the quotes as `SRAM'` with a backtick in front): 7.5 kB to 23.8 kB more RAM than the chip has, depending on build options. A trimmed build might fit (not run). All link on the 64 kB CC2340R53; not run. `docs/experiments/2026-10-05-cc2340r5-thread-full-device.md`.

### `ot-ti` builds just the RCP for LP_EM_CC2340R5 and skips the rest without a message

Two board-name checks in `examples/apps/cli/CMakeLists.txt` and `examples/apps/CMakeLists.txt` skip them. TI does not state why; the RAM shortfalls in the Thread answer above line up with TI's RCP-only listing.

### `ot-ti` link fails with "section .nvs VMA [0000000000010000,0000000000013fff] overlaps section .text"

The settings storage is placed inside the code for every CC23xx part. The build used GCC 12.2, SysConfig 1.26.3 and F3 SDK 9.20.00.86 rather than TI's exact setup, which may contribute. Move it to the end of flash (0x7C000 here): `thread/ot-ti-local.patch`.

### Can you run Bluetooth and an 802.15.4 radio stack (Zigbee, Thread, TI 15.4) at the same time on a CC2340R5?

Not with TI's example on the CC2340R52. The test used Bluetooth plus TI's 15.4 MAC stack, expected to be the lightest pairing; Zigbee or Thread would likely need more RAM (not measured). TI's DMM release notes say "The CC23XX products do not support DMM." TI's CC2755 DMM example, retargeted, compiles but fails to link with `error #10099-D: program will not fit into available memory` and `run placement with alignment fails for section ".bss" size 0xca0e` (full message in the experiment page). It is 2.1 kB short of the chip's free RAM before any heap. It links for the CC2340R53 with 1.9 kB unused; not run. `docs/experiments/2026-10-05-cc2340r5-dmm.md`.

### Does the CC2340R5 support Matter?

TI offers no Matter build for it: TI's Matter fork has examples for the CC27xx only. No build was tried; RAM is the likely reason. TI forum figures for Matter on older TI parts are about 700 kB of flash and 148 to 153 kB of RAM.

### Should you buy the LP-EM-CC2340R5 or the LP-EM-CC2340R53 for a first project?

For Bluetooth or Zigbee alone, the LP-EM-CC2340R5 (CC2340R52 chip) worked. For Thread as a full device, Bluetooth plus Zigbee or Thread at once, or room for your own code beside a radio stack, the builds here point to the 64 kB CC2340R53 (LP-EM-CC2340R53 board). Builds for it linked; nothing has been run on one.

### TI's Thread build script will not run on your Mac

`ot-ti`'s `script/bootstrap` is written for Linux (apt-get, sudo). Run its CMake step by hand with existing tools; the full setup is in `thread/README.md`. Its release notes name F3 SDK 9.20.00.81 and the repo pins 9.20.00.86, while 9.21.00.36 is the current download.

### What is "error #10264: NVRAM memory range overlaps existing memory range FLASH"?

The linker file gives flash a range that runs into the area reserved for stored settings (NVRAM). It appeared when moving TI's CC2755 DMM example to a CC23xx chip: the flash region had to end where the Zigbee settings storage starts. Fixed linker file: `matrix/dmm_r53_linker.cmd`.

### Should you use upstream Zephyr or TI's Zephyr fork for Bluetooth on this board?

TI's fork, with the C library heap turned on (`CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048`); upstream Zephyr 4.4.2 has no Bluetooth for this board. On the fork, the BTHome beacon and TI's peripheral sample ran on 2026-10-08; TI's beacon sample hard-faults (see above). TI's F3 SDK, without Zephyr, worked first time and is still the better-tested path.

### The Thread radio CLI image prints unreadable bytes at 115200 baud

Not solved. The `ot-cli-radio` test image built for this board (after its checksums were filled) answered with bytes that did not decode, then nothing, at every common speed. Observed 2026-10-06; not saved.

## Bluetooth from a Mac

### A Bluetooth scan over SSH on another Mac fails: "Bluetooth is not authorized for an unknown reason"

macOS grants Bluetooth only to apps a person has approved, not to SSH sessions. Scan from the Mac in front of you, or approve a helper app on the other Mac's screen.

### Home Assistant in Docker on a Mac cannot see Bluetooth devices

Docker on macOS has no access to the Mac's Bluetooth radio. Use an ESPHome Bluetooth proxy (an ESP32 board) or a Mac-side relay.

### My Mac sees fewer advertisements than the board sends

The test Mac reported about 2.5 a second at a 100 ms interval, where the board should send about 10. Measured once, with one receiver. Use gaps between packets, not counts, to check the interval.

### Can you change the Bluetooth name or advertising interval without the SysConfig GUI?

Yes: edit the example's `.syscfg` file and run `make` (31 seconds). TI's READMEs describe only the GUI. Example lines: `ble.deviceName = "My sensor";` and `ble.advSet1.advParam1.primIntMin = 20;` (milliseconds). Diff: `matrix/basic_ble_text.syscfg.diff`.

### My BTHome or custom-service UUID arrives byte-swapped from SysConfig

Type the UUID with its two bytes swapped in the `.syscfg`: for BTHome's 0xFCD2, write `ble.advSet1.advData1.UUID016SD = 0xD2FC;`. SysConfig 1.26.3 with the F3 SDK writes the 16-bit service-data UUID high byte first (`0xfc, 0xd2` in `ti_ble_config.c`); Bluetooth sends UUIDs low byte first, so scanners read 0xD2FC and nothing that looks for BTHome matches. The "More 16-bit UUIDs" field in the same tool gets the order right. Check the bytes in the generated `ti_ble_config.c` once. The worked line is in `ports/bthome/basic_ble.patch`.

### How can you see the board's Bluetooth from a phone?

Any Bluetooth scanner app should list TI's `basic_ble` example as "Basic BLE project". Scans ran only from a Mac (with the Python library `bleak`); phone apps were not run.

### How long will it run on a battery, and how do you measure its current from a Mac?

Not measured. TI's EnergyTrace software runs on Windows and Linux, not macOS. A separate meter such as Nordic's Power Profiler Kit II works from a Mac; none was bought for this project. TI's data sheet quotes under 0.71 microamps in standby for the CC2340R52.

### You set an option in the `.syscfg` text and nothing changed, with no error

Check the generated `ti_*.h` or `ti_*.c` file for the value. A Zigbee "always on" line built without complaint and was ignored; TI's metadata marks that option read-only by default. Some options can only be changed in C (see "How do you make a TI Zigbee end device keep its receiver on?").

### Why is Bluetooth throughput only 4 to 7 kbit/s from a Mac?

The test was a request-and-reply echo, not a stream. Each 20 to 128-byte write waited for its echo, and macOS's choice of connection interval is expected to have paced it; the interval was not read. A streaming test needs a characteristic that accepts writes without a reply, which TI's `data_stream` example does not have.

### Why does the Mac hang about 30 seconds when you connect to "CC2340R5 Gamepad", then drop it?

Remove the device in System Settings, Bluetooth, connect again and accept the pairing request. The gamepad's HID service needs an encrypted link, so macOS asks for pairing; if the request is not accepted it waits out a 30 s timeout and passes no input. 12 of 12 unaccepted connects returned after 31.2 to 31.4 s. Details: `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md`.

### Why does the Mac see "FRL Nordic" advertise, then fail to connect and turn its Bluetooth off?

Turn Bluetooth off and on in Control Center (or `sudo pkill bluetoothd`), check `system_profiler SPBluetoothDataType` shows `State: On`, and remove any earlier pairing of this board, such as "CC2340R5 Gamepad", in System Settings, Bluetooth. On 2026-10-25 the Nordic image advertised normally, a bleak connect timed out after 10 s, the next hung, and macOS's `bluetoothd` was stopped by its watchdog and came back with the controller off. The same image had connected 3 of 3 times on 2026-10-12. Details: `ports/nordic-apps/README.md`.

### Why does the gamepad reader page say "No gamepad"?

Press a button once with the page in front. Chrome and Edge hand a gamepad to a page only after the first input while it is open. Safari was not run. Page: `site/bench/gamepad.html`.

## Benchmarks

### Why does Embench `st` fail its own check on the CC2340R5 with TI Arm Clang?

It prints `R,embench,st,10314129,FAIL` because the compiler's software double add is sometimes one step low. On this core (no FPU) TI Arm Clang 5.1.1 links its own `__aeabi_dadd`, which rounds down in about 1 in 36 random additions where IEEE 754 rounds up; `st` checks its sums to 1e-13 and they come out 9.09e-13 and 1.82e-12 off. The same code passes on a Mac and with GCC.

### Can the CC2340R5 run a Q15 digital-power PID loop at 100 kHz?

Not quite: with no missed period in 1,000 it holds 97.4 kHz at -O2 and 113.5 kHz at -O3 (48 MHz, code in flash). At exactly 100 kHz the -O2 build's loop runs past some of its periods. The float version holds 21.8 kHz. Only builds from flash were tried. The limit lies between 480 and 493 timer counts; the timing checks were expected to lower it, and they did not. Why is not known; one suspect is that the checks run while the ADC is still sampling the next trigger.

### How fast can the CC2340R5 run the FOC motor-control loop?

Up to 2.42 kHz with no missed period in 1,000, and each interrupt takes 3,380 percent of a 100 kHz period of CPU time, about 16k cycles. The Cortex-M0+ has no FPU, so every float operation and every sine and cosine is library code; only -O2, -O3 and -Os builds from flash were tried. Two of the nine load runs were not clean: the main loop saw only 995 and 986 of the 1,000 interrupts. Why is not known; the rows use the clean runs.

### How much CPU does the CC2340R5's PID loop take?

At -O2 each Q15 interrupt takes 99.6 percent of a 100 kHz period (478 cycles at 48 MHz, measured at 48.7 kHz) and each float one 430.5 percent (measured at 10.9 kHz); at -O3, 90.8 and 427.9 percent. 478 cycles of CPU alone would allow about 100 kHz, and the loop's highest rate is 97.4 kHz, so at that rate almost no CPU is left. How the 478 comes about: per interrupt the main loop lost 601 cycles; minus one 22-cycle pass of the main loop's timer-reading loop, 82 cycles of timing checks and 19 of bookkeeping leaves 478. Measured at half the highest rate. The first try, at the highest rate itself, left the main loop no time and gave no number; that was a mistake in the method. The worst interrupt, with the checks left in, took 121.9 percent. Only builds from flash were tried; not run: code in RAM, other compilers.

### Are the CC2340R5's loop rates exact, given that its timer check reads 1.8 percent fast?

They may be up to 1.8 percent high. The rates are worked out from LGPT0 timer counts at its nominal 48 MHz; the check read 1,018 timer counts per 1k CPU cycles, so if the check is right each rate is up to 1.8 percent lower, for example 95.6 rather than 97.4 kHz for the Q15 PID loop.

### How much faster is the CC2340R5's AES hardware for one 16-byte block?

5.4 times faster than tiny-AES-c for one block (ECB hardware 1.2k cycles against 6.4k for software with its key expanded beforehand; CBC hardware takes 2k), against 128 times at 1 kB. For one block most of the hardware time is the driver call, which also loads the key every time; with the software's key expansion timed too the ratio is 6.8. TI Arm Clang 5.1.1 -O2, F3 SDK 9.21.00.36.

### Why did the FreeRTOS `yield_rt` test fail one pass in 1,008?

The harness started timing 50 ms after boot, and something interrupts once at about 53 ms. The bad pass ran the partner task 0 or 2 times instead of once; waiting 500 ms before timing fixed it in six of six runs. What that early interrupt is was not found.

### Why does the `rtos-heap` bench report `I,heap,total 8192` when the heap is bigger?

The line prints TI's default heap size, not the one the image links. That file is compiled without this project's heap setting; the linker map shows the real heap, 25.7 kB (`.bss.ucHeap`). The 41-task result is unaffected.

### Why does an ESPHome or other Zigbee router image fail to link with "run placement with alignment fails for section ".data""?

The image needs more than the chip's 36 kB of RAM. The ESPHome router build needed 37.4 kB, 1.3 kB over. Build it as a Zigbee end device instead (it fit with 4.0 kB left), or shrink ZBOSS's tables in SysConfig (not run). Details: `docs/experiments/2026-10-13-cc2340r5-esphome-fit.md`.

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
