# Can `esphome compile` and `esphome upload` build and flash a TI CC2755P10 or CC2340R5?

**Answer.** Two ESPHome platforms, `cc27xx` and `cc23xx`, in `components/`. Add them to a YAML with `external_components`, run `esphome compile`, then `esphome upload`, and a TI LaunchPad shows up in Home Assistant's Zigbee integration (ZHA) with a switch and a temperature sensor. On the LP-EM-CC2755P10 as a Zigbee router, `esphome compile` takes 35 s cold and 27 s warm (median of 3, Mac busy), the image is 262.7 kB of flash and 37.6 kB of RAM (124.4 kB free), `esphome upload` takes 15 s, the device joined ZHA 5 to 6 s after the flash and switched the LED 32 of 32 times (median 85 to 92.5 ms). The LP-EM-CC2340R5 builds in 31 s cold and was flashed the same way: as an end device (219.9 kB of flash, 32.0 kB of RAM, 4.0 kB free) it joined 7 to 19 s after the flash and answered 93 of 100 commands (20 of 20 in the last run); as a router it needs `heap_size: 0x23BC` and then runs with no heap left over. Results and cause tags: [ESPHome cc27xx platform experiment](../../docs/experiments/2026-10-13-esphome-cc27xx-platform.md). The earlier link tests that this grew out of: [CC2755P10](../../docs/experiments/2026-10-13-cc2755p10-esphome-fit.md) and [CC2340R5](../../docs/experiments/2026-10-13-cc2340r5-esphome-fit.md).

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. ZHA is Home Assistant's Zigbee integration; DSLite is the command-line flasher in UniFlash.

Needs, exactly (these are the versions built and run here; others are untested):

| What | Version | Where this Mac has it |
|---|---|---|
| ESPHome | 2026.9.1, in a Python 3.12 venv | `pip install esphome==2026.9.1` |
| TI SimpleLink Low Power F3 SDK | 9.21.00.36 LTS | `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS` (2.1 GB) |
| TI Arm Clang (`ti-cgt-armllvm`) | 5.1.1.LTS, comes with CCStudio 21.0.1 | `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS` (1.9 GB) |
| SysConfig | 1.26.3 | `/Applications/ti/sysconfig_1.26.3` (1.0 GB) |
| UniFlash (its `dslite.sh` flashes) | 9.6.0 | `/Applications/ti/uniflash_9.6.0` (2.0 GB) |
| Hardware | LP-EM-CC2755P10 or LP-EM-CC2340R5 on an XDS110 debug probe | `xdsdfu -e` lists the probe serial |

The platform looks for each tool under `/Applications/ti`, `~/ti` and `/opt/ti` (override with `ESPHOME_TI_ROOT`), and by name with `ESPHOME_TI_F3_SDK`, `ESPHOME_TI_CGT`, `ESPHOME_TI_SYSCONFIG`, `ESPHOME_TI_DSLITE`, or with `sdk_path:`, `compiler_path:`, `sysconfig_path:`, `dslite_path:` in the YAML block. A missing tool stops the build with its name and version. Only macOS has been tried.

## Ten-minute path

1. `python3.12 -m venv venv && ./venv/bin/pip install "cbor2<6" esphome==2026.9.1` (26 s, 222 MB; the `cbor2<6` is needed on an Intel Mac, where cbor2 6 has no wheel and wants a Rust compiler).
2. Install the four TI tools above (once, about 8 GB; the downloads need a free myTI login).
3. Copy this folder (`components/` and `frl-cc27xx.yaml`) next to each other.
4. In the YAML, set `probe_serial:` to your probe's serial (or leave it out and pass `--device <serial>` to `upload`).
5. `./venv/bin/esphome compile frl-cc27xx.yaml` (35 s cold).
6. In Home Assistant: Settings, Devices, Zigbee Home Automation, Add device (the join window stays open for 4 minutes).
7. `./venv/bin/esphome upload frl-cc27xx.yaml --device <probe serial>` (15 s). Without `--device` and `probe_serial:`, ESPHome lists the serial ports; pick the XDS110 port whose name holds your probe's serial (for example `/dev/cu.usbmodemLB07021W1`). The device appears in ZHA 5 to 6 s later.

Once the TI tools are installed, the path is the ESPHome install, one compile and one flash: about ten minutes. The CC2340R5 uses `frl-cc23xx.yaml` (an end device) or `frl-cc23xx-router.yaml`.

## What a YAML looks like

```yaml
esphome:
  name: frl-cc27xx
external_components:
  - source: {type: local, path: components}
    components: [cc27xx, die_temperature]
cc27xx:
  board: LP_EM_CC2755P10
  probe_serial: LB07021W
zigbee:
  router: true
switch:
  - {platform: gpio, name: "LED", pin: DIO16}
sensor:
  - {platform: die_temperature, name: "Chip temperature"}
```

`cc27xx` and `cc23xx` are target platforms like `esp32:` and `nrf52:`. Zigbee is the only radio stack. `die_temperature` is this repo's sensor (ESPHome's `internal_temperature` accepts only its own platform list). Pins are written as the DIO number (`DIO16`, or `16`). Every DIO of both chips works, with the header pin, the board's own use and the signals each pin can carry in the tables below (from `components/cc27xx/pindata.json`; `gen_pins.py` rebuilds it from TI's board and device data). On the CC2755P10 the red LED is DIO16 and the green is DIO12; on the CC2340R5 the red is DIO14 and the green is DIO15.

## If you see ... do

| You see | Do |
|---|---|
| `TI F3 SDK not found` (or Arm Clang, SysConfig, DSLite) | Install it, or point to it with the variable or YAML key above |
| `Cannot upload Over the Air as the ota configuration is not present` | ESPHome did not find the platform's upload step (it falls back to wireless upload). Use ESPHome 2026.9.1, and run `upload` from the folder that holds `components/` |
| The device joined once, then did not rejoin after a second `upload` | Remove the device from ZHA before flashing again; ZHA keeps the old entry and the new image did not rejoin it in 200 s (cause not found) |
| The CC2340R5 link stops with `program will not fit into SRAM` (a router) | Use `router: false`, or `heap_size: 0x23BC` in the `cc23xx:` block (see `frl-cc23xx-router.yaml`; the heap then runs out) |
| A Home Assistant command to the end device takes 2 to 3 s or times out | The end device is sleepy (polls every 3 s). The platform keeps its receiver on unless the YAML has `zigbee: sleepy: true`; rebuild with ESPHome 2026.9.1 and this folder |
| DSLite `Error -2001` or `Debug is not allowed` | The chip's CCFG and SCFG are blank or bad; see the [CC2755P10 FAQ](../../cc2755p10/FAQ.md) before touching a chip erase |

## The Wi-Fi board: LP-EM-CC35X1 (`cc35xx`)

**Answer.** `esphome compile` and `esphome upload` build and flash ESPHome for the LP-EM-CC35X1 (CC3551E, Wi-Fi 6). It joins Wi-Fi about 5 s after a reset, logs through ESPHome's logger on the board's serial port, announces itself over mDNS and serves ESPHome's native API. Home Assistant adopting it has not been run (the test network isolates its clients); an API client on the board itself got 20 of 20 switch commands. Details: [ESPHome cc35xx platform experiment](../../docs/experiments/2026-10-13-esphome-cc35xx-platform.md).

Needs, besides ESPHome 2026.9.1:

| What | Version | Where this Mac has it |
|---|---|---|
| TI SimpleLink Wi-Fi SDK | 10.20.00.39 | `/Applications/ti/simplelink_wifi_sdk_10_20_00_39` |
| TI Arm Clang | 5.1.1.LTS (CCStudio 21.0.1) | as above |
| SysConfig | 1.28.0 | `/Applications/ti/sysconfig_1.28.0` |
| SimpleLink Wi-Fi Toolbox (signs and programs) | 4.3.23 | `/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23` |
| Hardware | LP-EM-CC35X1 on an LP-XDS110ET probe, activated once with the Toolbox | `xdsdfu -e` lists the probe serial |

Overrides: `ESPHOME_TI_WIFI_SDK`, `ESPHOME_TI_CGT`, `ESPHOME_TI_SYSCONFIG_WIFI`, `ESPHOME_TI_TOOLBOX`, or the YAML keys `sdk_path`, `compiler_path`, `sysconfig_path`, `toolbox_path`.

1. Copy `components/` and `frl-cc35xx.yaml` into one folder, and write `secrets.yaml` beside them with `wifi_ssid:` and `wifi_password:`.
2. `./venv/bin/esphome compile frl-cc35xx.yaml` (56 s cold, 23 s warm).
3. `./venv/bin/esphome upload frl-cc35xx.yaml --device <probe serial>` (60 s: a full flash erase, then the signed image).
4. Watch it start: the board's application serial port at 115200 shows ESPHome's log and `[I][wifi]: Connected`.
5. In Home Assistant: Settings, Devices, Add integration, ESPHome, and the address from the log (not run).

| You see | Do |
|---|---|
| `KeyError: 'power_save_mode'` (or `max_connections`, `max_send_queue`) during compile | Keep those three keys in the YAML; they have no platform default on `cc35xx` |
| The log stops after `Starting scan` | Use ESPHome 2026.9.1 with this platform's current Wi-Fi backend (scanning was added after the first version) |
| Nothing on the serial port after programming | Read the port at 115200; programs from the Toolbox start by themselves after a full erase. An image programmed over TI's Wi-Fi CSI image without the erase stays silent (see the CC35X1 FAQ) |

Limits today: the API runs without encryption (the Noise library, noise-c, compiles to 25.5 kB of flash for a Cortex-M33 with TI Arm Clang at -Oz, so size is not the blocker; the random number source and the wiring into the build are not done), settings do not survive a reset, the network name and password are built into the image, Home Assistant adoption was not run, and there are no over-the-air updates. The board was not on USB for the 2026-11-10 round, so the Wi-Fi items stay open (see docs/next-session.md).

## Since 2026-11-10: logger, pins, buses, saved settings

- `logger:` works on `cc27xx` and `cc23xx`: the log goes to the probe's first serial port at 115200 baud and `esphome logs <yaml> --device /dev/cu.usbmodem<probe serial>1` reads it ([E319](../../docs/experiments/2026-11-10-esphome-ti-logger.md)). Use `logger: tx_buffer_size: 256` on the CC2340R5.
- Pins: all DIOs, checked against the table below; SWD and the 32 kHz crystal pads are refused ([E320](../../docs/experiments/2026-11-10-esphome-ti-pins.md)). `python3 ports/esphome-ti/pins_markdown.py` prints the tables.
- Buses ([E321](../../docs/experiments/2026-11-10-esphome-ti-buses.md)): `i2c:` on TI's I2C driver (ESPHome's own `hdc2080` and `opt3001` load on it), `ti_adc` sensor, `ti_pwm` output for `light: monochromatic`. PWM is verified in the timer registers; the I2C and ADC readings wait for the sensors BoosterPack and two jumpers. No `spi:` or `uart:` bus.
- Saved settings: `preferences:`, `restore_mode` and `globals: restore_value` survive a reset through TI's NVS driver, 8 kB under the Zigbee NVRAM ([E322](../../docs/experiments/2026-11-10-esphome-ti-saved-settings.md)).
- Examples: `frl-cc23xx-sensors.yaml` (I2C sensors and a PWM light), `frl-cc27xx-adc.yaml` (ADC and PWM).
- SysConfig 1.27.0 builds these as well as 1.26.3.

#### LP-EM-CC2340R5 (cc23xx)

| DIO | Header pin | Already used on the board for | Analog in | PWM (timer, channel) | Other signals |
|---|---|---|---|---|---|
| 0 | 10 | free | ADC5 | LGPT3 ch2 | I2C0SDA SPI0CSN |
| 1 | 25 | free | ADC4 | LGPT3 ch1 | UART0RTS |
| 2 | 26 | free | ADC3 |  | UART0CTS |
| 3 | not on the header | 32 kHz crystal pad |  | LGPT1 ch2, LGPT3 ch1 |  |
| 4 | not on the header | 32 kHz crystal pad |  | LGPT0 ch2 | SPI0MOSI UART0TXD |
| 5 | 27 | free | ADC2 | LGPT2 ch2 |  |
| 6 | 38 | SPI flash chip select | ADC1 | LGPT1 ch2 | I2C0SCL SPI0CSN UART0TXD |
| 7 | 28 | free | ADC0 | LGPT3 ch1 |  |
| 8 | 36 | free |  |  | I2C0SDA SPI0SCLK UART0RTS |
| 9 | 12 | button BTN-2 |  | LGPT3 ch0 |  |
| 10 | 13 | button BTN-1 |  |  |  |
| 11 | 18 | BoosterPack SPI chip select |  | LGPT0 ch0 | SPI0CSN SPI0MISO |
| 12 | 14 | SPI flash and BoosterPack SPI data in |  | LGPT1 ch1 | I2C0SDA SPI0MISO SPI0MOSI UART0RXD |
| 13 | 15 | SPI flash and BoosterPack SPI data out |  |  | SPI0MISO SPI0MOSI UART0TXD |
| 14 | 39 | red LED |  | LGPT3 ch2 |  |
| 15 | 40 | green LED |  |  | UART0RXD |
| 16 | not on the header | SWD (debug) |  | LGPT1 ch2 | I2C0SDA SPI0MOSI UART0RXD |
| 17 | not on the header | SWD (debug) |  | LGPT0 ch2 | I2C0SCL SPI0SCLK UART0TXD |
| 18 | 7 | SPI flash and BoosterPack SPI clock |  | LGPT3 ch0 | SPI0SCLK UART0TXD |
| 19 | 17 | free |  | LGPT3 ch1 | SPI0MOSI |
| 20 | 4 | backchannel UART: chip TX | ADC11 | LGPT1 ch0 | SPI0MISO UART0RXD UART0TXD |
| 21 | 37 | free | ADC10 | LGPT0 ch1 | SPI0MISO UART0CTS |
| 22 | 3 | backchannel UART: chip RX | ADC9 | LGPT2 ch0 | UART0RXD |
| 23 | 23 | free | ADC8 | LGPT2 ch1 |  |
| 24 | 9 | free | ADC7 | LGPT1 ch0, LGPT3 ch0 | I2C0SCL SPI0SCLK |
| 25 | 24 | free | ADC6 |  | I2C0SCL SPI0MISO |

#### LP-EM-CC2755P10 (cc27xx)

| DIO | Header pin | Already used on the board for | Analog in | PWM (timer, channel) | Other signals |
|---|---|---|---|---|---|
| 0 | 40 | button BTN-2 |  | LGPT0 ch0, LGPT1 ch0 |  |
| 1 | 4 | backchannel UART: chip TX |  | LGPT1 ch0, LGPT1 ch1, LGPT2 ch0 | CAN0TX UART0TXD |
| 2 | 3 | backchannel UART: chip RX |  | LGPT1 ch1, LGPT1 ch2 | CAN0RX UART0RXD |
| 3 | 7 | SPI flash and BoosterPack SPI clock |  | LGPT2 ch0 | I2S0SCLK SPI0SCLK UART1TXD |
| 4 | 14 | SPI flash and BoosterPack SPI data in |  | LGPT1 ch2, LGPT2 ch1 | SPI0MISO SPI0MOSI UART1RXD |
| 5 | 15 | SPI flash and BoosterPack SPI data out |  | LGPT2 ch1, LGPT2 ch2 | SPI0MISO SPI0MOSI |
| 7 | 18 | BoosterPack SPI chip select |  | LGPT2 ch2 | I2S0WS SPI0CSN |
| 9 | 34 | SWD (debug) |  | LGPT0 ch1 | I2C0SCL I2S0SD0 |
| 10 | 33 | SWD (debug) |  | LGPT0 ch2 | I2C0SDA I2S0SD1 |
| 11 | 39 | free |  | LGPT3 ch0 | SPI1MISO SPI1MOSI |
| 12 | 19 | green LED |  | LGPT3 ch1, LGPT3 ch2 | SPI1MISO SPI1MOSI |
| 15 | 35 | free |  | LGPT3 ch1, LGPT3 ch2 | SPI1SCLK |
| 16 | 36 | red LED |  | LGPT3 ch0 | I2S0MCLK SPI1CSN |
| 17 | 32 | free | ADC8 | LGPT0 ch0 | CAN0TX I2S0SCLK UART0RTS |
| 18 | 31 | free | ADC7 | LGPT0 ch1 | CAN0RX I2S0WS UART0CTS |
| 19 | 38 | SPI flash chip select | ADC6 |  | I2S0SD0 SPI0CSN UART0RXD UART0TXD |
| 20 | 30 | button BTN-1 | ADC5 |  | I2S0SD1 SPI0SCLK UART0RXD UART0TXD |
| 21 | 2 | free | ADC4 |  | I2C0SCL SPI0MOSI UART1TXD |
| 22 | 6 | free | ADC3 |  | I2C0SDA SPI0MISO UART1RXD |
| 23 | not on the header | 32 kHz crystal pad |  | LGPT0 ch2, LGPT1 ch0 | SPI1CSN UART1RTS |
| 24 | not on the header | 32 kHz crystal pad |  | LGPT0 ch0 | SPI1SCLK UART1CTS |
| 27 | 9 | free | ADC1 |  | I2C0SCL SPI1MOSI |
| 28 | 10 | free | ADC0 |  | I2C0SDA SPI1MISO |



## What is here

- `components/cc27xx/`: the platform. `platform_factory.py` (schema, pin, `run_compile`, `upload_program`, modelled on ESPHome's nrf52 platform), `ti_build.py` (SysConfig, compile, link, CRC patch, hex, DSLite), `shim/` (ESPHome's Zephyr and Nordic surface answered on TI's FreeRTOS and drivers: 388 lines), `ti/*.syscfg` (TI's `onoff_light` settings, logging off), `ccxml/` (debug connection templates).
- `frl-cc23xx.yaml`, `frl-cc23xx-router.yaml`: the CC2340R5 end device and router (router needs the smaller heap).
- `components/cc23xx/`: the same code under the platform name `cc23xx`; needs `cc27xx` beside it.
- `components/cc35xx/`: the Wi-Fi platform for the LP-EM-CC35X1. `__init__.py` (schema, pins, logger hook, `run_compile`, `upload_program`), `ti35_build.py` (TI's mqtt_client example as board support, SysConfig, compile, link, Toolbox signing and programming), `shim/` (Wi-Fi backend, HAL, logger, mDNS, the on-board API test), `ti/frl_wifi_append.c` (entry points appended to the build copy of TI's `wifi_if.c`).
- `frl-cc35xx.yaml`: the Wi-Fi example (LED switch, uptime, Wi-Fi signal).
- `components/die_temperature/`: the chip temperature sensor. On the CC2340R5 it uses TI's Temperature driver and reads about 10 C below the room (cause not found).
- `build.sh`: the earlier script version of the same steps, kept working (it builds from the nrf52 template YAMLs `frl-esphome-fit*.yaml`).
- `toggle_bench.py`: the Home Assistant toggle timer.

## How it fits into ESPHome

ESPHome 2026.9.1 lets an external component be a target platform (`IS_TARGET_PLATFORM`) with its own `run_compile` and `upload_program`. The platform runs ESPHome's code generation as usual, then builds ESPHome's generated C++, this repo's shim, and TI's FreeRTOS and ZBOSS libraries with TI's compiler, in parallel with unchanged files skipped on the next run. Four workarounds, each in the code with its reason: the platform adds `nrf52` to the loaded integrations because ESPHome's Zigbee component switches its switch support on by that name; it removes the validated-config cache because `esphome upload` otherwise skips loading external components; it passes `-DUSE_ZEPHYR -DUSE_NRF52` because the Zigbee sources are guarded by them; and the shim stands in for Zephyr.

Known limits: two boards flashed (CC2755P10: two joins, 32 toggles; CC2340R5: four joins, 190 commands, a 7-minute router run), no long soak test; random bytes are a stub; ESPHome's four `--wrap` hooks are not applied because TI's linker has no `--wrap`; only the Zigbee components the CC2755P10 link test exercised (switch, sensor) were run.
