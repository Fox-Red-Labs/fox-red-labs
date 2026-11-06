# Espressif ESP32-C6-DevKitC-1-N8 FAQ: errors on a Mac and their fixes

Problems with the ESP32-C6-DevKitC-1-N8 (ESP32-C6 chip: 32-bit RISC-V, 160 MHz, 512 kB RAM, 8 MB flash in the module) from a Mac terminal. Checked 2026-10-07 to 2026-10-12. 1 kB = 1,024 bytes. Terms: [docs/vocab.md](../docs/vocab.md).

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | The run did something wrong. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| Third-party behavior | Espressif, ESPHome, macOS or open-source code |
| Chip limit | The silicon cannot do it |
| Unknown | Not found |

## Connecting and flashing

### The board is plugged in and the Mac shows nothing. Is it dead?

Probably the cable. Use a USB-C cable that carries data, in the port marked **USB**. The board comes with no cable: Espressif's user guide (ESP32-C6-DevKitC-1 v1.2, read 2026-10-07) lists "USB-A to USB-C cable" under required hardware and warns "Some cables are for charging only and do not provide the needed data lines". The port marked USB is the chip's own USB and shows up as "USB JTAG/serial debug unit" (VID:PID 303a:1001, `/dev/cu.usbmodem...`). The port marked UART is a CP2102N USB-to-UART chip (10c4:ea60). Both power the board; the 3.3 V power LED lights on either.

## Installing the tools

### `pip install esphome` stops with `error: can't find Rust compiler` while building cbor2. Do you need Rust?

No. Tell pip to take a prebuilt cbor2: `pip install --only-binary cbor2 esphome==2026.9.1`. On macOS Intel with Python 3.12, pip 26.1.2 chose a cbor2 version (not recorded) that builds a Rust extension from source; with the flag it picked cbor2 5.9.0's macOS x86_64 wheel and finished in 37 s (checked 2026-10-11).

### `eim install` (Espressif's ESP-IDF Installation Manager, EIM) says "The following prerequisites are not satisfied ["dfu-util"]" and exits. Does the ESP32-C6 need dfu-util?

No. DFU is for the ESP32-S2 and ESP32-S3. Run `eim install ... --skip-prerequisites-check true` with cmake and ninja on your PATH, or skip EIM and run `./install.sh esp32c6` from a clone of ESP-IDF. EIM 0.20.0 listed dfu-util as missing on this ESP32-C6 install; the C6 does not use DFU: ESP-IDF's DFU guide is written for the ESP32-S3 and has no ESP32-C6 page (docs.espressif.com, read 2026-10-11).

### EIM filled the disk (6.3 GB and rising) on a one-chip install. How big should ESP-IDF be?

About 3.2 GB for the ESP32-C6 with GCC: the tools folder 2.3 GB (GCC, gdb, OpenOCD, ROM ELFs), the ESP-IDF tree 0.73 GB, the Python environment 0.12 GB. In non-interactive mode EIM 0.20.0 logs `Non-interactive mode: selecting all tools by default (QEMU excluded)`, which adds a 2.8 GB clang you do not need for `idf.py build`, and keeps the downloaded archives in `~/.espressif/dist` (0.9 GB). What worked: skip EIM's defaults and run `./install.sh esp32c6` from a clone of ESP-IDF v6.1 (57 s after the downloads, 3.2 GB in all); after EIM, delete `~/.espressif/tools/esp-clang*` and `~/.espressif/dist`. EIM's help also lists `--idf-tools` for choosing tools; it was not run here, so check the tool names with `eim install --help` first. Have about 4 GB free for the GCC-only path (3.7 GB measured); the install-everything path was stopped at 6.3 GB and still growing, so its full size is not known (checked 2026-10-11; sizes in `mac-setup/README.md`).

## Building with ESP-IDF

### An ESP-IDF v6.1 program resets in a loop with `Guru Meditation Error: Core  0 panic'ed (Illegal instruction)` and `MTVAL : 0x0000239c` the first time it prints a number with a decimal point

Add `CONFIG_LIBC_NEWLIB=y` to `sdkconfig.defaults` (or pick newlib under Component config, LibC in `idf.py menuconfig`), delete the `build` folder and build again. `0x239c` is `fld`, a double-precision load the ESP32-C6 cannot execute. ESP-IDF v6.1 defaults to the picolibc C library, and the `esp-15.2.0_20251204` toolchain on this Intel Mac has no picolibc folder for the C6's architecture (`rv32imac_zicsr_zifencei_zaamo_zalrsc`), so the linker falls back to the top-level picolibc `libc.a`: the build's link map names that file, and it holds 2,105 double-precision load and store instructions (counted 2026-10-11). Newlib has the right folder; with it the stock temperature example printed `Temperature value 27.40 ℃` (checked 2026-10-11; not checked on Linux or Apple-silicon toolchains).

### The rebuild fails after a few seconds with `ninja failed with exit code 1`, and flashing again writes the old program

Delete the `build` folder and build again; the full build took 97 s here. The build folder's CMake files had been deleted to save disk, and `idf.py` then could not rebuild in place; the flash step still wrote the image left from the last good build, so the change did not show (checked 2026-10-11).

### A Mac's Bluetooth scanner sees nothing from the board

Check that the Mac's Bluetooth is really on: `system_profiler SPBluetoothDataType` should say `State: On` and show an address. The behavior is macOS's. Here it said `State: Off` with `Address: NULL` for 15 minutes after Bluetooth was switched on in the menu bar; a Mac restart fixed it, and the scanner then saw the board's first advertisement 0.45 s after flashing (checked 2026-10-11).

### An ESP-IDF networking example (HTTP, HTTPS, sockets, SMTP, DNS over HTTPS) resets in a loop and the log ends with `ESP_ERROR_CHECK failed` and `abort() was called`

The example calls `example_connect()` and no Wi-Fi name or password is set, so the connection fails and `ESP_ERROR_CHECK` (the macro that stops the program on any error) aborts it. The same abort follows if the example is set to Ethernet with none fitted. Run `idf.py menuconfig`, open Example Connection Configuration, set the WiFi SSID and password, then build and flash again. To tell it from a real fault, look for `example_connect: Start example_connect.` a few lines before the abort. 24 of the 232 examples flashed for the C6 stopped this way with their default settings (23 networking examples and one parallel-IO example that also connects; checked 2026-10-12). They were not rerun with credentials.

### A flashed example makes the chip's own USB port disappear, and esptool says `No serial data received` or the port is gone

Some examples take the pins the USB port uses (GPIO12 and 13) or silence the port; seen with ESP-IDF's `mcpwm_bldc_hall_control` and `spi_slave_hd_append_mode_master`. Plug a second cable into the port marked **UART** and flash any other image through it, for example the blink example: the CP2102N chip there resets the chip into its ROM by itself. The steps are under the light-sleep entry below.

### An example resets with `W5500 version mismatched`, an assert in `esp_flash_spi_init`, or `ESP_ERR_INVALID_STATE` in a PSRAM example

The example expects a part the DevKitC-1 does not have: a W5500 Ethernet chip (`sockets_tcp_client_multi_net`), a second SPI flash chip (`custom_flash_driver`), PSRAM (`xip_from_psram`; the ESP32-C6-WROOM-1 module has none) or a trace encoder component (`sysview_tracing` stops with `Encoder 'ext' not found`). Nothing is wrong with the board.

### `system_console_basic` or `system_gdbstub` stops at start-up. `system_console_basic` ends with `Guru Meditation Error: Core 0 panic'ed (Load access fault)` right after `Command history enabled`

Seen on the default build (program counter 0x42020390); the cause was not found. `system_gdbstub` stops at start-up with an `assert failed` in the task-creation code, also not investigated. The `console_advanced` example ran clean.

### After running an ESP-IDF light-sleep or power-management example, esptool says `No serial data received` on the port marked USB, even after BOOT and RESET. How do you get the board back?

Use the other port. Plug a second USB-C data cable into the port marked **UART** and run esptool on that port (`/dev/cu.SLAB_USBtoUART` or `/dev/cu.usbserial-*`; check it reads your board's MAC with `esptool --port <port> read-mac`), then `erase-flash` and `write-flash` your image. The UART port's USB-to-serial chip resets the ESP32-C6 into its ROM bootloader by itself, so the sleeping program never gets a chance to run. On this bench, after ESP-IDF's `lowpower_power_management` example (automatic light sleep), the chip's own USB port stayed enumerated but silent across BOOT plus RESET, a BOOT-held replug and three esptool reset modes, and OpenOCD failed with `Unsupported DTM version: -1`; the UART port worked on the first try (erase 1.8 s, a 1.46 MB write 22.3 s at 460,800 baud; checked 2026-10-12). If both `/dev/cu.SLAB_USBtoUART` and `/dev/cu.usbserial-*` appear, one may say `the port is busy`: use the other.

## Home Assistant

### The ESPHome integration says `connection_error` when given `frl-ble-proxy.local`. The board is on Wi-Fi

Give it the board's IP address instead (it is in the `esphome logs` output as `IP Address:`), port 6053, then the API key. The name did not resolve from Home Assistant in Docker on another machine here; the cause was not isolated (suspected: the mDNS name does not cross the container network). The IP flow completed on the first try (REST flow; no log saved; checked 2026-10-11).

### The log shows `Disconnected ssid='...' bssid=... reason='Authentication Failed'` right after boot. Is the password wrong?

Wait for the next attempt before changing anything. In the one boot saved here the line appeared once, about 8 s after the bootloader's first log line (host stamps), and the second attempt joined 1 s later with the same password; `IP Address:` followed. Seen once, cause not found (checked 2026-10-11). If every attempt fails the same way, the password or network name in `secrets.yaml` is the first thing to check.

### Do you need ESP-IDF installed to build the ESPHome proxy?

No. ESPHome 2026.9.1 downloads its own ESP-IDF 5.5.5 and tools into `~/Library/Caches/esphome` (2.7 GB) on the first compile. If an ESP-IDF v6.1 environment is exported in the same shell (`IDF_PATH` set), ESPHome builds against that instead and fails with `The component 'esp_vfs_console' could not be found`; compile from a shell without it (checked 2026-10-11).

### Zigbee: the log says `Failed to steering network with status(0x03)` and the light does not appear in Home Assistant

Wait for the automatic retry: on this bench the second search, about 5 s later, joined Home Assistant's Zigbee network, and the light appeared 32.5 s after flashing. One run; the cause of the first failure was not found (checked 2026-10-11). Walkthrough: [zigbee/README.md](zigbee/README.md).

## ESPHome, Zigbee and Thread

### The log says `No networks found` forever, but the proxy image on the same board joined

Add a Bluetooth component to the YAML (even an idle `esp32_ble_tracker:`) and flash again. A Wi-Fi-only ESPHome 2026.9.1 image (`frl-esphome-minimal.yaml`) found no network in four boots on this board; the same YAML plus `esp32_ble_tracker:` joined at once. The two sdkconfigs differ in the coexistence options only. The cause was not isolated.

### `esphome upload` over Wi-Fi reaches `Preparing for upload`, then `Operation timed out` or `Connection reset by peer`

Upload over USB instead: `esphome upload file.yaml --device /dev/cu.usbmodemXXXX`. On two images the board logged `Stopping scan for OTA` and `Progress: 0.1%`, then printed nothing while still answering ping. A reset recovers it.

### The Zigbee log says `Failed to join network with status(0x03)` again and again

Check that ZHA's permit is really open: `zha.permit` takes at most 254 seconds, and Home Assistant rejects 300 with `value must be at most 254 at 'duration'`. If the permit is open and the device also runs Wi-Fi or a Bluetooth proxy, it may never join: the same ESPHome Zigbee end device joined in 6 s with Wi-Fi and Bluetooth left out.

### The board resets itself every 12 to 55 seconds with `rst:0xc (SW_CPU)` and no panic text

Seen with the ESPHome Zigbee end device plus Wi-Fi; the cause was not found. The Zigbee-only image and the Bluetooth-proxy-only image did not do it.

### Home Assistant on the Mac mini cannot reach a Thread device, though the name resolves

The Home Assistant host has no route to the Thread network's prefix. On macOS the route the Apple TV advertises is not installed by itself; add it with `sudo route add -inet6 <Thread prefix>/64 <Apple TV link-local address>%en0` (read your prefix from `netstat -rn -f inet6` on a Mac that has the route).

### Zigbee and Thread in one image: `multiple definition of 'esp_ieee802154_transmit_done'`

It does not link, and no setting changes that. OpenThread's radio port and the Zigbee library each define the seven `esp_ieee802154_*` driver callbacks. Use two chips, as Espressif suggests.

## Mistakes to avoid

- Compile ESPHome in a shell that does not have ESP-IDF v6.1 exported; otherwise it fails as above. Keep the two in separate shells.
- Keep the build folder of anything you will rebuild. Deleting its CMake files to save disk makes the next rebuild fail while the flash step writes the old image.
- Give each ESPHome YAML its own device name. ESPHome builds into a folder named after the device, so a second YAML with the same name overwrites the first one's image. Copy a build's `firmware.factory.bin` out before compiling another (rebuilding it took 4 minutes).
- Ask ZHA to permit joining for 254 seconds or less. It refuses 300, and the failed joins that follow look like the device's fault. Read the permit call's reply.

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
