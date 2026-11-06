# AI assistant notes for the ST NUCLEO-U083RC

That the linker scripts say 32 kB of RAM but the chip has 40 kB, that OpenOCD needs the SWD transport chosen before the target file, and that hardware AES runs 19.6 to 74 times faster than tiny-AES-c depending on whether ST's driver calls or direct register writes are used. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [NUCLEO-U083RC FAQ](FAQ.md) in the Fox Red Labs repo. In short: **STLINK-V2EC** is ST's debug probe built into the board; **STM32CubeCLT** is ST's command-line tool set; **STM32CubeU0** is ST's example and driver package for this chip family; **AES** is the standard block cipher; **tiny-AES-c** is a small open-source AES library in plain C; **TRNG** is a true random-number generator; **CoreMark** is a standard CPU speed test. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `nucleo-u083rc/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | STM32U083RC: Arm Cortex-M0+ up to 56 MHz; ST's data sheet, not a run (`nucleo-u083rc/README.md`) |
| Memory | 256 kB flash, 40 kB RAM; the RAM is one block from 0x20000000 to 0x2000A000 and a program filled and read back 38 kB (`nucleo-u083rc/FAQ.md`) |
| On the chip | Hardware AES-128 and AES-256, a TRNG, a 12-bit ADC, one DAC channel (`nucleo-u083rc/README.md`) |
| Not on the chip | No hardware divide and no hardware sin or cos: those rows are not applicable (`data/results/nucleo-u083rc.csv` math_divide, math_sincos) |
| Debugger | STLINK-V2EC on USB-C, socket CN1 marked STLINK; the Mac sees USB 0483:3752 and a serial port (`nucleo-u083rc/README.md`) |
| LED and button | Green LED LD4 on PA5; blue button B1 on PC13 (`nucleo-u083rc/README.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | Zephyr 4.4.2 with GCC 12.2, xPack OpenOCD 0.12.0-7; or STM32CubeCLT 1.22.0 in `/opt/ST` (`nucleo-u083rc/README.md`, `nucleo-u083rc/FAQ.md`) |
| Fetch sources | `./prebuild.sh --fetch-sources` (45 s) (`nucleo-u083rc/README.md`) |
| Fastest route | `./blink.sh 250` builds ST's `GPIO_IOToggle` with a 250 ms delay, flashes and verifies it: 14 s, 6 s build and 5 s flash (`nucleo-u083rc/README.md`, `data/results/nucleo-u083rc.csv` time_to_blink cli) |
| Zephyr build | `export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-` then `west build -p always -b nucleo_u083rc /tmp/blinky -d /tmp/blinky-build` (43 s) (`nucleo-u083rc/README.md`, `nucleo-u083rc/FAQ.md`) |
| Zephyr flash | `OCD=~/zephyr-toolchain/xpack-openocd-0.12.0-7` then `$OCD/bin/openocd -s $OCD/openocd/scripts -f interface/stlink.cfg -c "transport select swd" -f target/stm32u0x.cfg -c "program /tmp/blinky-build/zephyr/zephyr.elf verify reset exit"` (1.2 s) (`nucleo-u083rc/README.md`) |
| Check | LD4 changes speed (on 250 ms, off 250 ms); 7 min 51 s from plug-in on the Zephyr path (`nucleo-u083rc/README.md`) |
| Back up ST's demo first | `-c "init; reset halt; dump_image demo.bin 0x08000000 0x40000; shutdown"`; restore with `-c "program demo.bin 0x08000000 verify reset exit"` (`nucleo-u083rc/FAQ.md`) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Could not find a package configuration file provided by Zephyr-sdk` | This Mac has GCC 12.2 but not the full Zephyr SDK | Set the `cross-compile` toolchain variables above (`nucleo-u083rc/FAQ.md`) | `west build` runs |
| `stm32u0x.cfg:29: Error: invalid command name "swd"` | The target file assumes SWD but does not select it | Put `-c "transport select swd"` before the target file (`nucleo-u083rc/FAQ.md`) | OpenOCD connects |
| `couldn't bind gdb to socket on port 3333` | Another OpenOCD holds the port | Add `-c "gdb port disabled; tcl port disabled; telnet port disabled"` before the interface file (`nucleo-u083rc/FAQ.md`) | Flash runs |
| `Examination failed` after ST's `FLASH_EraseProgram` example | Cause unknown | Wait a few minutes or replug USB (`nucleo-u083rc/FAQ.md`) | The next try connects |
| `memory region not defined: RAM` from ST's Clang | ST's linker scripts are written for the GNU linker | Use ST's hybrid mode (`nucleo-u083rc/FAQ.md`) | Examples link |
| `Apple could not verify ... is free of malware` on the STM32CubeCLT installer | The package is unsigned | System Settings, Privacy & Security, Open Anyway (`nucleo-u083rc/FAQ.md`) | Installer opens |
| `NOD_U083RC not found` on Arduino upload | The default upload copies to a USB drive that does not appear | Set the upload method to STM32CubeProgrammer (SWD) (`nucleo-u083rc/FAQ.md`) | Upload runs |

## Rules for the assistant

1. Do not read GPIO registers over SWD while the program runs: the values come back wrong. Halt, read, resume (`nucleo-u083rc/FAQ.md`).
2. The wired loop tests (DAC to ADC, UART) have no result: the internal-route versions failed (`data/results/nucleo-u083rc.csv` adc_dac_error, uart_max_clean).
3. 1 kB AES against tiny-AES-c: 19.6 times through ST's driver calls, 65.1 times with direct register writes (`data/results/nucleo-u083rc.csv` aes_speedup).
4. CoreMark at 48 MHz with GCC 12.2 -O3 unrolled: 97.3 per second, 2.03 per MHz (`data/results/nucleo-u083rc.csv` coremark).

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran or built here

Code written for other chips and tools, brought to this board. The full card for each, with what was changed and where it stopped, is in `docs/ports.md`; the method and a prompt to port your own are there too.

| Port | Status | Result | Evidence |
|---|---|---|---|
| ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards | On the bench | The descriptor and four test reports match the library's output on an ESP32-C6 byte for byte on the LP-MSPM0G3519 (10,112 B flash), the NUCLEO-U083RC (3,724 B), the LAUNCHXL-F2800137 (8,302 B of code) and the LP-EM-CC2340R5 (193,995 B flash, 34,575 B of 36,864 B RAM), where the HID gamepad also advertises and shows the same Device Information. | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | On the bench | MaJerle's STM32 UART DMA example (1,838 stars) answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306 (636 B of its 4 kB RAM), C1104 (632 B of 1 kB) and MSPM33C321A on the same host check as the NUCLEO-U083RC original, 2,246 to 2,294 packets a second against 2,259; the LP-AM13E230 passes five of six checks. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
<!-- ports:end -->

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Examination failed after ST's FLASH_EraseProgram example; the next 11 examples could not connect at first; low-power and watchdog examples can stop the chip answering (`nucleo-u083rc/FAQ.md`, `nucleo-u083rc/AGENTS.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Wait a few minutes, or unplug USB and replug (all 11 loaded and ran on a second try a few minutes later) | hands | proven | `nucleo-u083rc/FAQ.md`, date not recorded |

<!-- recovery:end -->
