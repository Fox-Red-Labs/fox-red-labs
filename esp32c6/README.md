# Espressif ESP32-C6-DevKitC-1-N8 on a Mac: 184 s to a changed blink, CoreMark, Thread, Zigbee and Bluetooth

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

A plugged-in ESP32-C6-DevKitC-1-N8 reached a changed blink in 184 s with ESP-IDF v6.1 installed and the blink project already built (the clock includes a failed rebuild and a 97 s full rebuild on a busy Mac; starting from the copy step adds the first clean build, 67 s here), flashed over the chip's own USB port with no probe and no button. Its RISC-V core gives 437 CoreMark per second at 160 MHz (GCC -O3, code in flash), it joined the house Thread network in 4.55 s and Home Assistant's Zigbee network as a router light, and it runs as the house Bluetooth proxy. Measured 2026-10-11 on macOS Intel 15.7; every number is a row, with its conditions, in `data/results/esp32c6.csv`.

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes. Problems with fixes: [FAQ](FAQ.md).

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `prebuild.sh` (builds every target that needs no board; it writes outside the repository); `proxy/` (Bluetooth proxy configs) and `zigbee/` (Zigbee demos). Results are rows in `data/results/esp32c6.csv` at the top of the repository, not in this folder.

**Not in this folder:** Espressif's ESP-IDF and ESPHome (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add esp32c6 tools data third-party/coremark mspm0g3507/mcu-bench
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact |
|---|---|
| Chip and core | ESP32-C6: one 32-bit RISC-V core at up to 160 MHz plus a 20 MHz low-power core (Espressif data sheet v1.5, 2026-03-31, read 2026-10-07) |
| Memory | 512 kB of RAM, plus 16 kB that stays powered in deep sleep; the ESP32-C6-WROOM-1 module adds 8 MB of flash (data sheet) |
| Radio | Wi-Fi 6, Bluetooth LE 5 and 802.15.4 (Zigbee 3.0, Thread 1.3) on one shared antenna (data sheet) |
| Analog and serial | 12-bit ADC (up to 100k samples a second); two UARTs (up to 5 Mbaud) (data sheet) |
| Security | AES, SHA, RSA and ECC hardware, a true random number generator (data sheet) |
| Not on the chip | No floating-point unit and no DAC (data sheet) |
| Debugger and USB | Two USB-C ports: the chip's own USB (USB JTAG/serial debug unit, no probe needed) and a CP2102N USB-to-UART chip (user guide v1.2, schematic v1.4) |
| LEDs and buttons | One addressable color LED on GPIO8; BOOT and RESET buttons (user guide) |
| Other | A jumper (J5) for measuring the module's current; 32 header pins; no USB cable in the bag; sold by DigiKey |
| Published speed | Espressif publishes 496.66 CoreMark at 160 MHz (3.10 per MHz) without naming the compiler (data sheet) |

## What this board is for here

Two jobs. All the numbers below were taken on the board that is also the Bluetooth proxy; it was reflashed for the tests and has the proxy image back (checked 2026-10-11). A second board has not been used for tests.

1. **The Bluetooth proxy.** Home Assistant runs in Docker on a Mac and cannot use the host's own Bluetooth. An ESP32-C6 running ESPHome's Bluetooth proxy listens for Bluetooth sensors and passes them to Home Assistant over Wi-Fi. Walkthrough: [proxy/README.md](proxy/README.md).
2. **The comparison board.** The same tests as the TI boards, run the same way.

## Ten-minute path

Install ESP-IDF v6.1 first: clone it and run `./install.sh esp32c6` in its folder (57 s after the downloads; 3.2 GB installed, 3.7 GB while installing; the [FAQ](FAQ.md) says how to keep it that small). In every new terminal, activate it before anything else; use a different terminal for ESPHome, because the two clash ([FAQ](FAQ.md)). The path below is where this Mac's copy lives; use your own ESP-IDF folder if it differs.

```bash
. ~/.espressif/v6.1/esp-idf/export.sh
```

Then copy the example:

```bash
cp -R "$IDF_PATH/examples/get-started/blink" blink && cd blink
```

Before setting the target, tell the build to use newlib (a different C library from ESP-IDF's default, picolibc). Blink works without it, but the first program that prints a decimal number stops with `Illegal instruction` on this Mac's toolchain ([FAQ](FAQ.md)).

```text
echo CONFIG_LIBC_NEWLIB=y >> sdkconfig.defaults
```

```bash
idf.py set-target esp32c6
```

```bash
idf.py build
```

```bash
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Plug the board into the USB-C port marked **USB**, not the one marked UART. Find the port name with `ls /dev/cu.usbmodem*`. The color LED blinks and the log prints `Turning the LED ON!` and `OFF!` once a second. To prove your own change landed, set the period in `idf.py menuconfig` (Example Configuration, Blink period) to 250, build and flash again: the log lines now come four times a second.

## One-command build

```bash
esp32c6/prebuild.sh
```

Builds blink, the temperature sensor example, CoreMark (three flag sets plus Espressif's own port), the four occupancy models, Thread's command-line example and both ESPHome images into `<scratch>/runs/arrivals/esp32c6/`, each with newlib. `esp32c6/prebuild.sh --sweep` also builds every ESP-IDF example that lists the C6. It installs nothing and refuses to start under 6 GB free.

## What was measured

All rows measured 2026-10-11 on macOS Intel 15.7 unless the Level column says otherwise. The conditions for each are in its row of `data/results/esp32c6.csv`.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Plugged board to a changed blink | 184 s | ESP-IDF v6.1 installed, blink already built; includes a failed rebuild and a 97 s full rebuild; expected under 5 minutes | measured | `time_to_blink` in `data/results/esp32c6.csv` |
| First clean blink build | 67 s | from the copy step | measured | `blink_build` |
| CoreMark | 436.9 per second (2.73 per MHz) | GCC -O3, 160 MHz, code in flash; expected 410 to 500 | measured | `coremark`, `coremark_per_mhz` (gcc-bare-o3-riscv) |
| CoreMark, Espressif's own port | 451.1 per second (2.82 per MHz) | same board | measured | `coremark` (gcc-vendor-o3) |
| CoreMark, Espressif's published figure | 496.66 (3.10 per MHz) | compiler not named | claim | Espressif data sheet v1.5 |
| Room-occupancy models | 1.12, 2.17, 7.14 and 27.1 ms, all 20 of 20 right | -O3; the 8k model was expected at 8 to 17 ms | measured | `ai_occ_latency`, `ai_occ_fit` |
| ESP-IDF examples that build | 333 of 356 listed for the C6 | ESP-IDF v6.1 | computed | `examples_built` (esp-idf-v6.1) |
| ESP-IDF examples that run clean | 202 of 232 | no crash marker in 20 s of console | measured | `examples_ran_clean` (esp-idf-v6.1) |
| Sonnet 5.5 agent, blink | passed 3 of 3 with this repo's docs and 3 of 3 without; median 3.9 and 3.6 minutes | 18 runs in all | measured | `agent_pass`, `agent_minutes` (`blink-sonnet5.5-ours`, `blink-sonnet5.5-none`) |
| Sonnet 5.5 agent, CoreMark | passed 3 of 3 and 3 of 3; median 4.6 and 7.8 minutes | with and without the docs | measured | `agent_pass`, `agent_minutes` (`coremark-sonnet5.5-ours`, `coremark-sonnet5.5-none`) |
| Sonnet 5.5 agent, Zigbee-and-Thread question | passed 3 of 3 and 2 of 3; median 0.3 and 1.4 minutes | with and without the docs | measured | `agent_pass`, `agent_minutes` (`question-sonnet5.5-ours`, `question-sonnet5.5-none`) |
| Bluetooth: flash to first advertisement on the Mac | 0.45 s | expected 1 to 3 s | measured | `ble_on_air` (nimble-legacy-30ms) |
| Thread: join the house network through an Apple border router | 4.55 s; one hour of pings, 3,600 of 3,600 answered | expected 3 to 10 s | measured | `radio_join` (thread-child), `packet_loss_1h` |
| Zigbee: join Home Assistant's ZHA as a router light | 32.5 s (one run; the first search failed and the automatic retry joined); 20 of 20 toggles, 98.5 ms median | expected 10 to 60 s and 70 to 150 ms | measured | `radio_join` (zigbee-router), `radio_cmd_latency` |
| Bluetooth proxy in Home Assistant | on Wi-Fi 8.8 s after reset, 207 kB free heap | expected about 150 kB or more | measured | `esp32c6/proxy/README.md`, `radio_ram_left` (ble-esphome-proxy) |

Also measured: GPIO toggle, interrupt latency, task switching and AES (`data/results/esp32c6.csv`). Not measured: the UART and ADC loops (they need two jumper wires) and power (needs a meter).

## If you see this

- If you see `Guru Meditation Error: Core  0 panic'ed (Illegal instruction)` with `MTVAL : 0x0000239c` the first time a program prints a number with a decimal point, add `CONFIG_LIBC_NEWLIB=y` to `sdkconfig.defaults`, delete the `build` folder and build again.
- If you see `ninja failed with exit code 1` a few seconds into a rebuild, delete the `build` folder and build again (97 s here); flashing before that writes the old program.
- If you see `No serial data received` on the port marked USB after flashing an example, plug a second cable into the port marked UART and flash the blink example through it.

The rest, with causes: [FAQ](FAQ.md).

## Limits

- UART and ADC loops: not run; they need two jumper wires.
- Power: not measured; it needs a meter.
- Light-sleep example sweep rows (`examples_ran_clean`, esp-idf-v6.1-sleep): not run.
- Zigbee and Thread in one image: failed to build (`stack_support`, zigbee-thread); see the FAQ.

## Where these facts come from

Checked 2026-10-11 on macOS Intel 15.7 (Mac, one board, the same one that is the Bluetooth proxy). Tools: ESP-IDF v6.1 (toolchain `esp-15.2.0_20251204`) for the benchmarks and examples; ESPHome 2026.9.1, which builds on its own ESP-IDF 5.5.5, for the proxy and the ESPHome images; Python 3.12. Chip facts: Espressif ESP32-C6 data sheet v1.5 (2026-03-31), ESP32-C6-DevKitC-1 user guide v1.2 and schematic v1.4, read 2026-10-07. Sources: `data/results/esp32c6.csv`, `AGENTS.md`, `FAQ.md`.
