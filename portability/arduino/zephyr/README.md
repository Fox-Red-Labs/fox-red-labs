# Arduino Blink and AnalogReadSerial on Zephyr: TI LP-EM-CC2340R5 and LP-MSPM0G3519

**Short answer.** Yes. Arduino's two stock examples, Blink and AnalogReadSerial, built unchanged and ran on both boards through the Zephyr-based Arduino core (`zephyrproject-rtos/ArduinoCore-zephyr`, the GSoC 2022 module). Each board needs one devicetree overlay of about 40 lines for the pin map and no edit to the sketch. **Status: Works** on both boards (one board each). Two things need care: the core's `Serial` drops bytes on the CC2340R5 unless a six-line patch is applied, and on the MSPM0G3519 the image only starts after an nRST pulse (nRST is the chip's reset pin). Both are in [FAQ.md](FAQ.md).

Terms: [vocab](../../../docs/vocab.md); problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. A **devicetree overlay** is a small text file that tells Zephyr which chip pin is which Arduino pin. The **static module** is how the core joins the build: Zephyr compiles its source into the image (`-DZEPHYR_EXTRA_MODULES`), so neither Zephyr tree is edited. **LLEXT** (loadable extensions, which load a sketch at run time) is not used. The **backchannel** is the probe's USB serial port.

## Results

Flash and RAM come from the Zephyr link summary (`Memory region` table) and, for this repo's layer, `tiarmsize`. The Zephyr figure includes the kernel, drivers and C library. The ADC values are floating pins: they show the call works, not that it is accurate.

**LP-EM-CC2340R5** (36 kB RAM), Zephyr 4.4.2.

| Sketch | Flash | RAM | Ran | Source |
|---|---|---|---|---|
| Blink | 27.7 kB | 5.2 kB | yes: GPIO output register bit for DIO15 clear, then set on the next three reads | Zephyr link summary |
| AnalogReadSerial | 27.8 kB (the Serial patch adds 16 bytes) | 5.2 kB | yes: 10-bit values 295 to 400 on a floating A0 at 9600 baud (with the Serial patch) | serial capture |
| InterruptSelfTest, FadeCheck, SpiLoop, WireScan | 32.9, 32.9, 32.0, 37.4 kB | 6.1 kB | `attachInterrupt`, `analogWrite`, `SPI.transfer`, `Wire`: Works, limits under "Limits" below. `attachInterrupt` counted 10 of 10 edges driven by flipping the pad's pull resistor; `analogWrite` compare registers read back as value x 48,000 / 255; WireScan read the OPT3001 on the Sensors BoosterPack (ID 0x5449 and 0x3001, 1,594 lux) | serial captures |

**LP-MSPM0G3519** (Zephyr sees 64 kB of its 128 kB), Zephyr main 4.5.0-dev.

| Sketch | Flash | RAM | Ran | Source |
|---|---|---|---|---|
| Blink | 22.8 kB | 4.8 kB | yes: PB27 toggled, `blink.py` PASS | LED pin read over the probe |
| AnalogReadSerial | 22.8 kB | 4.8 kB | yes: 10-bit values 72 to 400 on a floating A0 (PA25) at 9600 baud | serial capture |
| WireFT6336U, ButtonInterrupt, FadePWM, SpiLoopback | 31.1, 31.7, 32.1, 31.5 kB | 6.3, 6.3, 6.4, 6.3 kB | `Wire`, `attachInterrupt`, `analogWrite`, `SPI.transfer`: Works, limits under "Limits" below. `Wire` found the touch controller at 0x38 (chip ID 0x64, 100 reads, 0 wrong); `attachInterrupt` RISING counted 10 of 10; PWM duty measured inverted (FAQ); SPI loopback returned 256 of 256 bytes | serial captures |

**LP-MSPM0G3507**, this repo's own Arduino layer (`../build.sh`), for comparison.

| Sketch | Flash | RAM | Ran | Source |
|---|---|---|---|---|
| Blink | 2.6 kB text, 8.8 kB total with the layer's 6 kB stack and heap reserve | 6.2 kB | built, not flashed | build output of `../build.sh` |
| AnalogReadSerial | does not compile: `use of undeclared identifier 'A0'` | | no | build log |

## Overlay pins

- **LP-EM-CC2340R5:** `LED_BUILTIN` is D0 = DIO15 (green LED). A0 is DIO5 = ADC channel 2 (A1 DIO2, A2 DIO1, A3 DIO25, A4 DIO23). `Serial` is `uart0` on the XDS110 backchannel. BoosterPack header: SPI SCLK pin 7 (DIO18), MOSI pin 15 (DIO13), MISO pin 14 (DIO12), CS pin 18 (DIO11); I2C SCL pin 9 (DIO24), SDA pin 10 (DIO0). The board has no I2C pull-ups.
- **LP-MSPM0G3519:** `LED_BUILTIN` is D0 = PB27 (green LED). A0 is PA25 = ADC0 channel 2 (header J1 pin 6). `Serial` is `uart0` on PA10 and PA11 with jumpers J12 and J13 on XDS. With the `zw` overlay: I2C2 on PC2 and PC3, SPI0 on PB18, PB17 and PB19, PWM on TIMA1 PB27, S2 as D4. The overlay also switches the ADC on (the board file does not), with `ti,clk-range = <6>`, `ti,clk-divider = <8>`, `max-result-reg = <12>` and the `analog_pa25` pin state; the MSPM0 ADC driver is in Zephyr main only, so AnalogReadSerial cannot run on an MSPM0 board with Zephyr 4.4.2.

## Limits

- `tone` was not run, and ADC accuracy was not tested through the Arduino call (the pins float).
- CC2340R5: SPI read data and the HDC2080 read need a loopback wire and a power pin tied low; the interrupt was driven from software, not by pressing S1; the LED fade and blink were not seen, only registers and timer counts were read.
- LP-MSPM0G3519: SPI was checked through the block's loopback bit, not a wire; the interrupt was driven from software, not by pressing S2; the PWM was read at the pin from inside the chip; the LED blink was read through the GPIO output register only.
- LP-MSPM0G3507: Zephyr was not flashed on it; only this repo's Blink build exists.

## Take it with you

Copy these files into a folder of your own, keeping the folder names. The build needs nothing else from this repo.

| File | What it is |
|---|---|
| `app/CMakeLists.txt` | Includes `sketches/<Name>/<Name>.ino` as it is after `#include <Arduino.h>`; sketch name and folder come in as `-DSKETCH` and `-DSKETCH_DIR` |
| `app/prj.conf` | `CONFIG_GPIO`, `CONFIG_ADC`, `CONFIG_ARDUINO_API` |
| `app/boards/lp_em_cc2340r5.conf` | `CONFIG_PWM`, `CONFIG_SPI`, `CONFIG_I2C`; Zephyr reads it because the file name is the board name |
| `variants/lp_em_cc2340r5/lp_em_cc2340r5.overlay` | The Arduino pin map, the ADC channels and the PWM, UART, I2C and SPI choices for this board (passed as `-DDTC_OVERLAY_FILE`) |
| `sketches/<Name>/<Name>.ino` | Your sketch, or Arduino's unchanged |
| `serial-blocking-write.patch` | CC2340R5 only, optional: makes `Serial` wait for room instead of dropping bytes; applied to the core checkout, not to your project |

For the LP-MSPM0G3519 also take `variants/lp_mspm0g3519/lp_mspm0g3519.overlay`, `lp_mspm0g3519-zw.overlay` and `zw.conf` (pass the first two as `-DDTC_OVERLAY_FILE="a;b"` and `zw.conf` as `-DEXTRA_CONF_FILE`; `build-g3519.sh` shows the exact line). The G3519 Wire, SPI, PWM and interrupt sketches need a Zephyr main workspace with the MSPM0 I2C pull request applied (recipe in [`../../zephyr-mspm0/README.md`](../../zephyr-mspm0/README.md#set-up-once)).

**What it needs.**

- Zephyr 4.4.2 west workspace for the CC2340R5, with the picolibc module; Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler.
- ArduinoCore-zephyr checked out at `af5b8610f9be6d7b962e2ba6dd8e6859697c44ff`, with ArduinoCore-API 1.5.2 in `zephyr/blobs/ArduinoCore-API/api`: `git clone --depth 1 --branch 1.5.2 https://github.com/arduino/ArduinoCore-API.git` and copy its `api` folder and `LICENSE` there.
- No force-include flag and no include order to set: the core comes in as `-DZEPHYR_EXTRA_MODULES`, the overlay as `-DDTC_OVERLAY_FILE`.

**A different board in the same family** (another LaunchPad that Zephyr already has a board file for): copy the overlay to `variants/<board>/<board>.overlay` and edit the `zephyr,user` node. Change `digital-pin-gpios`, `builtin-led-gpios`, `adc-pin-gpios`, `io-channels`, `pwms`, and the `serials`, `i2cs` and `spis` lines, then the `&adc0` channel nodes (one per `io-channels` entry). The PWM cell after the controller is the timer channel the pin is muxed to, not a pin number. Copy `app/boards/lp_em_cc2340r5.conf` to `app/boards/<board>.conf`. The pin numbers come from the board's BoosterPack connector file in Zephyr; the overlays in `variants/` list the ones used here. On an MSPM0 part, check every pin group against SysConfig's device file first ([FAQ](FAQ.md)).

**Build from a fresh folder** (`<port>` is the folder holding `app/`, `sketches/` and `variants/`; `<AM>` is the core checkout):

```bash
. <zephyr workspace>/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd <zephyr workspace>/zephyr
west build -p always -b lp_em_cc2340r5 <port>/app -d <build> -- -DSKETCH=Blink -DSKETCH_DIR=<port>/sketches \
  -DZEPHYR_EXTRA_MODULES=<AM> -DDTC_OVERLAY_FILE=<port>/variants/lp_em_cc2340r5/lp_em_cc2340r5.overlay
```

Built from a copy of only these files on 2026-10-25 (Blink, LP-EM-CC2340R5, copy outside the repo, zero errors): 32.0 kB of flash and 6.1 kB of RAM. The Blink row in the results table (27.7 kB, 5.2 kB) was linked before the overlay gained its PWM, SPI and I2C nodes and the `app/boards` file; the same copy without that file links at 28.3 kB and 5.7 kB. The LP-MSPM0G3519 copy was not built from a copy.

## Set up once

The build scripts install nothing. Each reads the variables below and stops with one line naming a missing path. Host: an Intel Mac (the only one tried), Python 3.12 or newer, `ninja`, `cmake`, UniFlash 9.6.0 and an XDS110 probe.

| Variable | What it is | Size |
|---|---|---|
| `AM` | ArduinoCore-zephyr checkout at `af5b8610f9be` (recipe below) | about 5 MB |
| `ZP`, `TC`, `VENV` | Zephyr 4.4.2 west workspace, Zephyr SDK 0.17.4 GCC 12.2.0, Python 3.12 venv with `west`: `cc2340r5/zephyr/setup-intel-mac.sh` installs all three | 3.6 GB, 1.1 GB, 0.3 GB |
| `PICOLIBC` | picolibc module, comes with `ZP` (`west update picolibc`) | 30 MB |
| `ZW` | Zephyr main workspace with the MSPM0 I2C pull request, for G3519 Wire, SPI, PWM and interrupts: [`../../zephyr-mspm0/README.md`](../../zephyr-mspm0/README.md#set-up-once) | 0.8 GB |
| `ZM`, `HALTI`, `CMSIS6` | Zephyr main at `1405adc9a` and its `hal_ti` and `cmsis_6` modules, for the two stock sketches on the G3519 when `ZW` is not set | 0.8 GB |
| `OUT` | build folder, one per sketch, created by the script | 5 to 60 MB |

```bash
git init $AM && cd $AM && git remote add origin https://github.com/zephyrproject-rtos/ArduinoCore-zephyr.git
git fetch --depth 1 origin af5b8610f9be6d7b962e2ba6dd8e6859697c44ff && git checkout FETCH_HEAD
mkdir -p zephyr/blobs && cp -R <repo>/third-party/arduino/ArduinoCore-API/api zephyr/blobs/ArduinoCore-API   # replaces `west blobs fetch`
git apply <repo>/portability/arduino/zephyr/serial-blocking-write.patch   # CC2340R5 only, see FAQ
```

## Build

```bash
portability/arduino/zephyr/build-cc2340r5.sh Blink     # or AnalogReadSerial, InterruptSelfTest, FadeCheck, SpiLoop, WireScan; about 80 s
portability/arduino/zephyr/build-g3519.sh Blink        # stock sketches on Zephyr main; about 80 s
V=portability/arduino/zephyr/variants/lp_mspm0g3519    # Wire, SPI, PWM, interrupts: the patched workspace, about 4 min
ZW=<zephyr workspace> OVERLAY="$V/lp_mspm0g3519.overlay;$V/lp_mspm0g3519-zw.overlay" CONF=$V/zw.conf \
  portability/arduino/zephyr/build-g3519.sh WireFT6336U         # or ButtonInterrupt, FadePWM, SpiLoopback
```

The result is `$OUT/zephyr/zephyr.hex` (the CC2340R5 build also writes `zephyr_crc32.hex`). `app/CMakeLists.txt` includes the `.ino` as it is after `#include <Arduino.h>`, so the sketch file stays byte-identical to Arduino's.

## Flash and run

Back up first and restore at the end: read the CC2340R5 main flash and CCFG (the chip's boot configuration area), or the G3519 main flash and NONMAIN, with DSLite before the first write, and compare after the restore. `ports/micropython/zephyr-cc2340r5/README.md` has the exact CC2340R5 commands.

- CC2340R5: `DSLite flash -c cc2340r5/CC2340R5.ccxml -e -f -u $OUT/zephyr/zephyr_crc32.hex`, then `xds110reset -s <probe serial>`. Serial at 9600 baud on `/dev/cu.usbmodem<probe serial>1`; ignore the first line (FAQ).
- G3519: `portability/zephyr-mspm0/flash.sh <image.hex>` pulses nRST, flashes from a `.bin` padded to a multiple of 8 bytes, and pulses nRST again. Check by reading the LED pin over the probe.

## Links

- [FAQ.md](FAQ.md): every problem met, with the exact text and the fix, and when to use this core and when this repo's own Arduino layer.
- Checked 2026-10-24 (Blink, AnalogReadSerial) and 2026-10-25 (interrupts, PWM, SPI, Wire) on an Intel Mac, macOS 15.7. ArduinoCore-zephyr `af5b8610f9be` (branch `next`, 2026-08-21), ArduinoCore-API 1.5.2 (the copy in `third-party/arduino/`, all 46 files match the module's SHA-256 list), sketches from `arduino/arduino-examples` (`50b66a3`, `examples/01.Basics`; SHA-256 of `Blink.ino` `ed145f60...`, `AnalogReadSerial.ino` `8357b5a1...`), Zephyr 4.4.2 for the CC2340R5, Zephyr main 4.5.0-dev (`1405adc9a`, 2026-10-20) for the MSPM0G3519, GCC 12.2.0 (Zephyr SDK 0.17.4), picolibc.
- License: ArduinoCore-zephyr (Apache-2.0) is cloned, not copied; ArduinoCore-API 1.5.2 (LGPL-2.1) is the copy in `third-party/arduino/`; the sketches are Arduino's, unedited. The patch and overlays are this repo's.
