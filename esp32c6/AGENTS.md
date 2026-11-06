# AGENTS.md for the Espressif ESP32-C6-DevKitC-1-N8: what an AI coding assistant needs to know

Know this first: ESP-IDF v6.1 on an Intel Mac needs newlib set before the first program prints a decimal number, and ESP-IDF and ESPHome must be used from separate terminals. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [ESP32-C6 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **ESP-IDF** is Espressif's C SDK; **ESPHome** builds device firmware from a YAML file; **newlib** is a C library, and picolibc is ESP-IDF v6.1's default; **CoreMark** is a standard CPU speed test; **Thread** and **Zigbee** are 802.15.4 mesh networks. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `esp32c6/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | ESP32-C6: one 32-bit RISC-V core at up to 160 MHz plus a 20 MHz low-power core; from Espressif's data sheet, not a run (`esp32c6/README.md`) |
| Memory | 512 kB of RAM; 8 MB of flash from the ESP32-C6-WROOM-1 module (`esp32c6/README.md`) |
| Not on the chip | No floating-point unit and no DAC (`esp32c6/README.md`) |
| Radio | Wi-Fi 6, Bluetooth LE 5, 802.15.4 (Zigbee 3.0, Thread 1.3) on one antenna (`esp32c6/README.md`) |
| Debugger | None needed: the port marked USB is the chip's own USB, seen as "USB JTAG/serial debug unit" (303a:1001, `/dev/cu.usbmodem...`). The port marked UART is a CP2102N (10c4:ea60) (`esp32c6/FAQ.md`) |
| LED and buttons | One addressable color LED on GPIO8; BOOT and RESET (`esp32c6/README.md`) |
| Cable | None in the bag; it must carry data (`esp32c6/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | ESP-IDF v6.1, installed with `./install.sh esp32c6` (57 s, 3.2 GB installed); ESPHome 2026.9.1 in a Python 3.12 venv, which builds on its own ESP-IDF 5.5.5 (`esp32c6/README.md`) |
| Activate | `. ~/.espressif/v6.1/esp-idf/export.sh`, in a terminal where ESPHome is not used (`esp32c6/README.md`) |
| Build blink | `cp -R "$IDF_PATH/examples/get-started/blink" blink && cd blink`, then `echo CONFIG_LIBC_NEWLIB=y >> sdkconfig.defaults`, `idf.py set-target esp32c6`, `idf.py build` (67 s clean build) (`esp32c6/README.md`, `data/results/esp32c6.csv` blink_build) |
| Flash | `idf.py -p /dev/cu.usbmodemXXXX flash monitor`, port from `ls /dev/cu.usbmodem*` (about 2.4 s for the write) (`esp32c6/README.md`, `data/results/esp32c6.csv` blink_flash) |
| Check | The color LED blinks and the log prints `Turning the LED ON!` and `OFF!` once a second (`esp32c6/README.md`) |
| Everything prebuilt | `esp32c6/prebuild.sh` (refuses under 6 GB free) (`esp32c6/README.md`) |

Time from plugged-in board to a changed blink: 184 s with the tools installed and blink already built (`data/results/esp32c6.csv` time_to_blink).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Guru Meditation Error: Core  0 panic'ed (Illegal instruction)` with `MTVAL : 0x0000239c` | ESP-IDF v6.1's default picolibc links a library with double-precision loads the C6 cannot run | Add `CONFIG_LIBC_NEWLIB=y` to `sdkconfig.defaults`, delete `build`, rebuild (`esp32c6/FAQ.md`) | Program prints decimals |
| `ninja failed with exit code 1` on a rebuild | The build folder's CMake files had been deleted | Delete `build` and build again; flashing in between writes the old image (`esp32c6/FAQ.md`) | Rebuild passes |
| `No serial data received`, or the USB port is gone | Some examples take the USB pins (GPIO12 and 13) | Flash blink through the port marked UART (`esp32c6/FAQ.md`) | The USB port returns |
| `The component 'esp_vfs_console' could not be found` | ESP-IDF is exported in the shell ESPHome runs in | Compile ESPHome from a shell without `IDF_PATH` (`esp32c6/FAQ.md`) | Compile passes |

## Rules for the assistant

1. Use the port marked USB for flashing, not UART (`esp32c6/FAQ.md`).
2. Keep ESP-IDF and ESPHome in separate shells (`esp32c6/FAQ.md`).
3. Zigbee and Thread in one image failed to build (`data/results/esp32c6.csv` stack_support zigbee-thread, `esp32c6/FAQ.md`).
4. Numbers measured here are on macOS Intel 15.7, 2026-10-11 (`esp32c6/README.md`).

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran or built here

Code written for other chips and tools, brought to this board. The full card for each, with what was changed and where it stopped, is in `docs/ports.md`; the method and a prompt to port your own are there too.

| Port | Status | Result | Evidence |
|---|---|---|---|
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | On the bench | MaJerle's STM32 UART DMA example (1,838 stars) answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306 (636 B of its 4 kB RAM), C1104 (632 B of 1 kB) and MSPM33C321A on the same host check as the NUCLEO-U083RC original, 2,246 to 2,294 packets a second against 2,259; the LP-AM13E230 passes five of six checks. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
<!-- ports:end -->

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- After an ESP-IDF light-sleep or power-management example, esptool says No serial data received on the USB port even with BOOT and RESET (`esp32c6/FAQ.md`)
- An OTA update stalls at Progress: 0.1% while the board still answers ping (`esp32c6/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Plug a second cable into the UART port and run esptool there (erase-flash, write-flash); its bridge resets the chip into the ROM bootloader | hands | proven | `esp32c6/FAQ.md`, date not recorded |
| Reset (OTA stall) | hands | proven | `esp32c6/FAQ.md`, date not recorded |
| uhubctl cycle of the probe's hub port (board was connected but never enabled) | remote | failed | 2026-10-12 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-12. Board connected but never enabled; a cycle did not help.

<!-- recovery:end -->
