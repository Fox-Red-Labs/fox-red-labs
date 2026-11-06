# Radio family porting FAQ: errors seen moving code to and from the CC2340R5, CC2755P10 and CC1354P10

Errors hit while moving code onto and between the radio boards, with the fix and the file behind each. Board-only errors are in `cc2340r5/FAQ.md`, `cc2755p10/FAQ.md` and `cc1354p10/FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### An MSPM0 TI Drivers app stops at a `CONFIG_*_IOMUX` name or `DisplayUart.h`

Force-include the shim and define each pin-mux name as 0. The MSPM0 app ORs the pin's mux index into `GPIO_setConfig`; the radio SDKs take it from the board table, and call the display header `DisplayUart2.h`. 10 of 10 MSPM0 examples built this way with a 47-line shim and no app edits. `portability/mspm0-to-wireless.md`, `portability/shim/README.md`, matrix X1.

### A carried MSPM0 app calls `UART_open` or `SPI_MOTO4_POL0_PHA0`

The shim maps them. The radio SDKs call the UART layer `UART2` and drop the `MOTO4` part of the SPI mode names. The same SPI names carry different values (MSPM0 4, F3 0), so source moves but compiled libraries do not. `portability/mspm0-to-wireless.md`.

### `make` cannot find `sysconfig_1.26.3/sysconfig_cli.sh`

Pass `SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh`. The F3 9.21 examples build with 1.28.0. `cc2340r5/AGENTS.md`, matrix X27.

### `Region Base (0xE6000) is outside the device's usable flash [0x0, 0x80000)`

Set the storage addresses to 0x7C000 and 0x79000. TI's CC2755 example keeps storage past 512 kB, which the CC2340R5 does not have. `cc2340r5/AGENTS.md`.

### ``ld.bfd: region `RAM' overflowed by 346 bytes``

Use `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=1024`. Zephyr's `samples/bluetooth/peripheral` with a 2048 heap on TI's fork does not fit 36 kB. `cc2340r5/AGENTS.md`.

### `error #10264: NVRAM memory range overlaps existing memory range FLASH`

Use `cc2340r5/matrix/dmm_r53_linker.cmd`. The DMM linker flash region runs into the Zigbee settings storage. `cc2340r5/AGENTS.md`.

### `section .nvs VMA [0000000000010000,0000000000013fff] overlaps section .text`

Apply `cc2340r5/thread/ot-ti-local.patch`. `ot-ti` places settings at 0x10000. `cc2340r5/AGENTS.md`.

### `invalid option: --wrap=...` on TI Arm Clang

Drop the flag, or define the `__real_` names as stubs. TI's linker has no `--wrap`. `cc2755p10/AGENTS.md`.

### `undefined node label 'boosterpack_spi'` from `west build --shield ti_bp_cc140xp`

Build with no `--shield` and pass `-DEXTRA_DTC_OVERLAY_FILE=ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay`. The board file defines `boosterpack_header` but not `boosterpack_spi`; four CC1407P samples then build. `cc2755p10/AGENTS.md`, matrix X46.

### `rfPacketRx` fails to link: `drivers_cc13x4.a`, `driverlib.lib`, `nortos_cc13x4.a` missing

Run `python3 cc1354p10/radio-rx/build_f2_libs.py <SDK>`. TI's GitHub copy of the F2 SDK carries those libraries as source only. `cc1354p10/AGENTS.md`.

### A SysConfig UUID arrives byte-swapped

Type the two bytes swapped: for BTHome's 0xFCD2, `ble.advSet1.advData1.UUID016SD = 0xD2FC;`. SysConfig 1.26.3 writes a 16-bit service-data UUID high byte first. `portability/driver-layering.md`.

## Running

### `Debug is not allowed based on` after flashing the CC2755P10

Recover over SACI with `cc2755p10/recovery/saci_program.js` (`check` first), then flash a hex that carries a valid CCFG and SCFG. A chip erase, or an `ot-ti` GCC image with checksums left at 0, closes the debug port. Do not erase again. `cc2755p10/AGENTS.md`.

### `Error -242` on the CC1354P10

Unplug USB for 3 s and flash again, or switch the probe's hub port off for 6 s and load at once. The debugger connects only in the first seconds after power-on; a sleeping image, a flash read or a Bluetooth demo uses up the one connection. `cc1354p10/AGENTS.md`.

### `Unable to access the DAP` on the CC2340R5

Let CCStudio write the `.ccxml` and add your probe serial. A hand-written connection file had SWD off. `cc2340r5/AGENTS.md`.

### The Thread console prints garbage on the CC2755P10

Open the backchannel UART at 921600. `version` then prints `OPENTHREAD/1.4.0.0`. `cc2755p10/AGENTS.md`.

### A Zephyr pin interrupt counts nothing after a later `pinMode`, `Pin.init` or `gpio_pin_configure`

Set the interrupt again after configuring the pin. Zephyr 4.4.2 `gpio_cc23x0` clears the edge-detect bits on configure. `portability/arduino/zephyr/README.md`.

### MicroPython `PWM(duty_u16=...)` gives a duty far too small at 1 kHz

Use `duty_ns`, or apply `ports/micropython/zephyr-cc2340r5/pwm-duty-u16-overflow.patch`. A 32-bit overflow in MicroPython 1.29.0 `machine_pwm.c`; the patched image is 16 bytes larger. `ports/micropython/zephyr-cc2340r5/README.md`.

### An I2C scan finds nothing, but a register read at the same address works

Probe with a one-byte register read. The scan sends an empty write, which fails on the Zephyr 4.4.2 CC23x0 I2C driver; cause not traced. `portability/arduino/zephyr/README.md`, `ports/micropython/zephyr-cc2340r5/README.md`.

### MicroPython's stock Zephyr settings do not link on the CC2340R5

Use `ports/micropython/zephyr-cc2340r5/prj_cc2340r5.conf` (networking, sensors, shell and file systems off, 20 kB heap). The stock settings overflow the 36 kB of RAM by 46.8 kB. `ports/micropython/zephyr-cc2340r5/README.md`.

### The Mac lists the Nordic image but a connect times out and Bluetooth turns off

Turn Bluetooth off and on in Control Center, and remove old pairings of this board. macOS `bluetoothd` was stopped by its XPC watchdog during the connect; cause not found. `ports/nordic-apps/README.md`.

## Mistakes to avoid

- Never chip-erase a CC2755P10 to reset it, and never edit its CCFG or SCFG.
- Do not write the CCFG at 0x50000000 on the CC1354P10.
- Do not flash the `.bin` or use `-v` with DSLite on the F3 boards: the `.bin` is 1.22 GB and spans to the CCFG, and every image fails verify at the CCFG although it runs.
