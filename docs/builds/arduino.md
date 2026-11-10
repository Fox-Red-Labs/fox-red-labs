# Run Arduino sketches on a TI chip

Arduino's Blink and AnalogReadSerial build unchanged through the Zephyr-based Arduino core and run on the LP-EM-CC2340R5 and the LP-MSPM0G3519. PlatformIO was not run.

The page: [site/project-arduino.html](../../site/project-arduino.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Arduino Blink and AnalogReadSerial on the LP-EM-CC2340R5

Arduino's two stock sketches build unchanged through the Zephyr-based Arduino core and run on the 36 kB Bluetooth chip.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/arduino/AGENTS.md`):

```bash
git sparse-checkout add portability/arduino
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| A Mac to build and flash from | 1 | builds and flashes |

### Before you start

- A Mac. Only an Intel Mac, macOS 15.7, was tried. Python 3.12 or newer, `ninja`, `cmake` and UniFlash 9.6.0.
- About 5 GB for the Zephyr 4.4.2 workspace (3.6 GB), the Zephyr SDK 0.17.4 GCC 12.2.0 cross compiler (1.1 GB) and a Python 3.12 environment with `west` (0.3 GB).
- Close any CCStudio debug connection before step 5; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Install the Zephyr 4.4.2 workspace, the compiler and the `west` environment with the script that builds Zephyr's blinky; `NO_FLASH=1` stops it after the build so it does not touch a board.

   ```text
   NO_FLASH=1 <repository folder>/cc2340r5/zephyr/setup-intel-mac.sh
   ```

   You should see: The script ends with `NO_FLASH set. Built, not flashed.`

   If a download fails, rerun the script; it keeps what it already fetched.

2. **Mac, terminal.** Check out the Arduino core at the pinned commit and put Arduino's API library beside it. The last line is a six-line patch that makes `Serial` wait for room instead of dropping bytes at 9600 baud; skip it if you only run Blink.

   ```text
   git init <ArduinoCore-zephyr folder> && cd <ArduinoCore-zephyr folder> && git remote add origin https://github.com/zephyrproject-rtos/ArduinoCore-zephyr.git
   git fetch --depth 1 origin af5b8610f9be6d7b962e2ba6dd8e6859697c44ff && git checkout FETCH_HEAD
   mkdir -p zephyr/blobs && cp -R <repository folder>/third-party/arduino/ArduinoCore-API/api zephyr/blobs/ArduinoCore-API
   git apply <repository folder>/portability/arduino/zephyr/serial-blocking-write.patch   # CC2340R5 only
   ```

   You should see: The core folder holds `zephyr/blobs/ArduinoCore-API/api`, and `git status` shows the patched file.

   If the build later cannot find Arduino's API headers, `zephyr/blobs/ArduinoCore-API/api` is missing.

3. **Mac, terminal.** Build Blink. The script takes the variables the README lists (`AM` for the core, `ZP`, `TC` and `VENV` for the workspace) and stops with one line naming a missing path. Neither Zephyr tree nor the sketch is edited.

   ```text
   <repository folder>/portability/arduino/zephyr/build-cc2340r5.sh Blink
   ```

   You should see: About 80 s later the build folder holds `zephyr/zephyr.hex` and `zephyr/zephyr_crc32.hex`. Blink is about 32 kB of flash and 6.1 kB of RAM.

   If it stops naming a variable, set it: `AM` is the core folder of step 2.

4. **Bench.** Clip the LP-XDS110ET onto the board and plug it into the Mac with its USB-C cable; the board has no USB port of its own (`cc2340r5/AGENTS.md`). Read the probe serial with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   You should see: The probe shows up as an XDS110 and `xdsdfu -e` prints its serial number.

   If `xdsdfu -e` lists nothing, check the clip-on connection and use a data cable.

5. **Mac, terminal.** Back up first: read the main flash and the CCFG with DSLite before the first write; the exact backup and restore commands are in `ports/micropython/zephyr-cc2340r5/README.md`, Flash and run. Then flash the `_crc32` image, which carries the boot checksums, and reset the board.

   ```text
   $DSLITE flash -c cc2340r5/CC2340R5.ccxml -e -f -u <build folder>/zephyr/zephyr_crc32.hex
   xds110reset -s <your-probe-serial>
   ```

   You should see: DSLite writes and verifies the image, then the reset starts Blink. The green LED pin (DIO15) is the one Blink toggles; the check read its output register through the probe and saw bit 15 clear, then set. The LED itself was not watched.

   If the flash fails, close CCStudio or any other program that holds the probe, and rerun it.

6. **Mac, terminal.** To see serial output, build AnalogReadSerial the same way (`build-cc2340r5.sh AnalogReadSerial`), flash it as in step 5, and open the probe's serial port at 9600 baud in any serial terminal.

   ```text
   <repository folder>/portability/arduino/zephyr/build-cc2340r5.sh AnalogReadSerial
   # serial port: /dev/cu.usbmodem<your-probe-serial>1 at 9600 baud
   ```

   You should see: A line of a number per read: 10-bit values between about 295 and 400 on a floating A0. The first line may be garbled; ignore it.

   If bytes are dropped or the numbers run together, the Serial patch of step 2 is missing.

### Expected result

Arduino's own Blink sketch, unchanged, runs on the LP-EM-CC2340R5 through the Zephyr-based core, and AnalogReadSerial prints readings at 9600 baud on the probe's serial port.

### What this shows

- Arduino's two stock examples, Blink and AnalogReadSerial from `arduino/arduino-examples`, build byte-for-byte unchanged for this chip and run.
- The port is one devicetree overlay of about 40 lines for the pin map, a CMake file that includes the `.ino` as it is, and an optional six-line Serial patch.
- attachInterrupt, analogWrite, SPI and Wire also ran from short test sketches.

### Limits

- That the LED blinked to the eye: the check read the pin's output register and the LED was not watched.
- That ADC readings are accurate: the values come from a floating pin and show only that the call works.
- That PlatformIO works: it was not run, so this page gives no PlatformIO steps.
- That it works on Windows or Linux: only an Intel Mac was used.
- The core's `Serial` drops bytes at 9600 baud on this chip unless the six-line patch is applied.
- The CC2340R5 board file maps the red PWM LED to a timer channel that does not reach the pin; see the board page.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Core | ArduinoCore-zephyr af5b8610f9be (Apache-2.0), ArduinoCore-API 1.5.2 | `portability/arduino/zephyr/README.md` |
| Zephyr | 4.4.2, Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler | `portability/arduino/zephyr/README.md` |
| Image, Blink | 32.0 kB of flash and 6.1 kB of RAM, built from a copy of the take-it-with-you files | `portability/arduino/zephyr/README.md` |
| Image, AnalogReadSerial | 27.8 kB of flash, 5.2 kB of RAM (before the overlay gained PWM, SPI and I2C) |  |
| Sketches | Arduino's unchanged examples from arduino/arduino-examples 50b66a3 | `portability/arduino/zephyr/README.md` |

## Arduino Blink and AnalogReadSerial on the LP-MSPM0G3519

The same two stock sketches run on the 80 MHz MSPM0 through the Zephyr-based Arduino core, on Zephyr main.

Status: Works. Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/arduino/AGENTS.md`):

```bash
git sparse-checkout add portability/arduino portability/zephyr-mspm0
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | the board |
| A Mac to build and flash from | 1 | builds and flashes |

### Before you start

- A Mac. Only an Intel Mac, macOS 15.7, was tried. Python 3.12 or newer, `ninja`, `cmake`, UniFlash 9.6.0.
- A Zephyr main workspace at `1405adc9a` with its `hal_ti` and `cmsis_6` modules and the Zephyr SDK 0.17.4 GCC 12.2.0 cross compiler, set up as in `portability/zephyr-mspm0/README.md`, Set up once. Zephyr 4.4.2 has no MSPM0 ADC, so the stock sketches need Zephyr main.
- Close any CCStudio debug connection before step 3; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Set up the Zephyr main workspace, its modules and the compiler as `portability/zephyr-mspm0/README.md` describes under Set up once, and check out the Arduino core as in the CC2340R5 version, step 2 (skip the Serial patch).

   You should see: The workspace folder and the core folder exist.

   If a path is missing, the script stops with one line naming the variable (`AM`, `ZM`, `HALTI`, `CMSIS6`); set it.

2. **Mac, terminal.** Build Blink for the LP-MSPM0G3519 on Zephyr main. Neither the Zephyr tree nor the sketch is edited.

   ```text
   <repository folder>/portability/arduino/zephyr/build-g3519.sh Blink
   ```

   You should see: About 80 s later the build folder holds `zephyr/zephyr.hex`; Blink is 22.8 kB of flash and 4.8 kB of RAM.

   If it stops naming a variable, set it; `AM` is the core folder of step 1.

3. **Mac, terminal.** Flash the image. The script pulses nRST, flashes from a `.bin` padded to a multiple of 8 bytes, and pulses nRST again; without the second pulse the image stops in the NMI handler.

   ```text
   <repository folder>/portability/zephyr-mspm0/flash.sh <build folder>/zephyr/zephyr.hex
   ```

   You should see: The script reports the flash and the board restarts into Blink.

   If the board stops in the NMI handler, pulse nRST again (`flash.sh` does it after the flash).

4. **Mac, terminal.** Check that the Blink pin toggles.

   ```text
   python3 <repository folder>/tools/check/blink.py --board mspm0g3519 --probe <your-probe-serial>
   ```

   You should see: The checker prints PASS while PB27 toggles.

   If it prints FAIL while PB27 toggles, it read the output-enable bit right after its own reset, inside the 0.4 s start-up; read the bit over DSLite without a reset.

5. **Mac, terminal.** To see serial output, build AnalogReadSerial the same way, flash it as in step 3, and open the probe's serial port at 9600 baud.

   ```text
   <repository folder>/portability/arduino/zephyr/build-g3519.sh AnalogReadSerial
   # serial port: /dev/cu.usbmodem<your-probe-serial>1 at 9600 baud
   ```

   You should see: A number per read: 10-bit values between about 72 and 400 on a floating A0 (PA25).

   If the port stays silent, pulse nRST: the G3519 image only starts after one (`flash.sh` does it after the flash).

### Expected result

Arduino's own Blink sketch, unchanged, toggles PB27 on the LP-MSPM0G3519 through the Zephyr-based core, and AnalogReadSerial prints readings at 9600 baud.

### What this shows

- Arduino's two stock examples build unchanged for the MSPM0G3519 and run.
- With the MSPM0 I2C pull request applied to Zephyr main, Wire, SPI, analogWrite and attachInterrupt also ran from short test sketches.

### Limits

- That the LED blinked to the eye: the check read the pin and the LED was not watched.
- That ADC readings are accurate: the values come from a floating pin.
- That PlatformIO works: it was not run, so this page gives no PlatformIO steps.
- That it works on Zephyr 4.4.2: that release has no MSPM0 ADC or I2C driver.
- Zephyr sees 64 kB of the chip's 128 kB of RAM.
- The G3519 Wire, SPI, PWM and interrupt sketches need a Zephyr main workspace with the MSPM0 I2C pull request applied.
- The take-it-with-you copy of the files was built for the CC2340R5 only; the G3519 copy was not built from a copy.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Core | ArduinoCore-zephyr af5b8610f9be, ArduinoCore-API 1.5.2 | `portability/arduino/zephyr/README.md` |
| Zephyr | main 4.5.0-dev (1405adc9a, 2026-10-20), Zephyr SDK 0.17.4 GCC 12.2.0 | `portability/arduino/zephyr/README.md` |
| Image, Blink | 22.8 kB of flash, 4.8 kB of RAM |  |
| Image, AnalogReadSerial | 22.8 kB of flash, 4.8 kB of RAM |  |
