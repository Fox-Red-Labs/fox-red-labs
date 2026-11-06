# Type Python at a prompt on a TI chip

MicroPython's own Zephyr port builds with one config file and runs on the LP-EM-CC2340R5 and the LP-MSPM33C321A: a prompt on the probe's serial port and Pin from Python.

The page: [site/project-micropython.html](../../site/project-micropython.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## MicroPython on the LP-EM-CC2340R5

MicroPython's own Zephyr port gives a prompt on the 36 kB Bluetooth chip; Pin, ADC, interrupts, PWM, SPI and I2C run from Python.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/micropython/zephyr-cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add ports/micropython/zephyr-cc2340r5
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| A Mac to build and flash from | 1 | builds and flashes |

### Before you start

- A Mac. Only an Intel Mac, macOS 15.7, was tried. Python 3.12 or newer, `ninja`, `cmake`, UniFlash 9.6.0 and `pyserial` (`python3 -m pip install pyserial`) for the REPL helper.
- About 5 GB for the Zephyr 4.4.2 workspace, the Zephyr SDK 0.17.4 GCC 12.2.0 cross compiler and a Python 3.12 environment with `west`, plus 62 MB for MicroPython.
- Close any CCStudio debug connection before step 5; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Install the Zephyr 4.4.2 workspace, the compiler and the `west` environment; `NO_FLASH=1` stops the script after its build so it does not touch a board. Then clone MicroPython v1.29.0 (commit `0fd6c573e`); nothing in it is edited.

   ```text
   NO_FLASH=1 <repository folder>/cc2340r5/zephyr/setup-intel-mac.sh
   git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git <MP>
   ```

   You should see: The script ends with `NO_FLASH set. Built, not flashed.` and `<MP>` holds MicroPython.

   If a download fails, rerun the script; it keeps what it already fetched.

2. **Mac, terminal.** Copy `prj_cc2340r5.conf` and `lp_em_cc2340r5.overlay` from `ports/micropython/zephyr-cc2340r5` into a folder `<proj>`, then build. The conf replaces MicroPython's own `prj.conf`, which overflows the chip's RAM by 46.8 kB; the overlay adds the ADC channels.

   ```text
   . <zephyr workspace>/.venv/bin/activate
   export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-
   cd <zephyr workspace>/zephyr
   west build -p always -b lp_em_cc2340r5 <MP>/ports/zephyr -d <build> -- -DCONF_FILE=<proj>/prj_cc2340r5.conf -DEXTRA_DTC_OVERLAY_FILE=<proj>/lp_em_cc2340r5.overlay
   ```

   You should see: About 3 minutes later the build ends with zero errors: 174.9 kB of flash (34.15 %) and 29.9 kB of RAM (82.99 %). The image is `<build>/zephyr/zephyr_crc32.hex`.

   If the link stops with ``region `RAM' overflowed by 47888 bytes``, the build used MicroPython's own `prj.conf`: pass `-DCONF_FILE` as shown. If it stops with `relocation truncated to fit: R_ARM_THM_JUMP11 ... nlr_push_tail`, keep `CONFIG_MICROPY_CONFIGFILE="mpconfigport.h"` and do not use the minimal conf.

3. **Bench.** Clip the LP-XDS110ET onto the board and plug it into the Mac with its USB-C cable; the board has no USB port of its own. Read the probe serial with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   You should see: The probe shows up as an XDS110 and `xdsdfu -e` prints its serial number.

   If `xdsdfu -e` lists nothing, check the clip-on connection and use a data cable.

4. **Mac, terminal.** Back up the main flash and the CCFG before the first write, then flash the `_crc32` image, which carries the boot checksums. Restore later with the same `flash` command on `restore.hex`.

   ```text
   DSLITE=<UniFlash folder>/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite; CCXML=cc2340r5/CC2340R5.ccxml; BK=<backup folder outside the repo>
   $DSLITE memory -c $CCXML -r 0x0,0x20000 -o $BK/main_flash.bin
   $DSLITE memory -c $CCXML -r 0x4E020000,0x200 -o $BK/ccfg.bin
   python3 cc2340r5/oad/images.py merge $BK/restore.hex $BK/main_flash.bin@0x0 $BK/ccfg.bin@0x4E020000
   $DSLITE flash -c $CCXML -e -f -u <build>/zephyr/zephyr_crc32.hex
   ```

   You should see: DSLite writes the image and reports success; the restore hex reads back byte for byte against the backup.

   If the flash fails, close CCStudio or any other program that holds the probe, and rerun it.

5. **Mac, terminal.** Open the REPL with the helper that holds the serial port open and sends lines (115200 baud, `/dev/cu.usbmodem<your-probe-serial>1`).

   ```text
   python3 ports/micropython/zephyr-cc2340r5/repl.py /dev/cu.usbmodem<your-probe-serial>1 out.log --reset -- 'print(1+1)'
   ```

   You should see: `out.log` holds the banner `MicroPython v1.29.0 on 2026-10-24; zephyr-lp_em_cc2340r5 with cc2340r5`, then the line `2`.

   If nothing appears, check that the port is the probe's second serial port and that no other program holds it.

6. **Mac, terminal.** Drive DIO15, the green LED pin, and read an ADC pin from Python. Pins are named by Zephyr devicetree label and number; the ADC number is the channel from the overlay, not the DIO number.

   ```text
   python3 ports/micropython/zephyr-cc2340r5/repl.py /dev/cu.usbmodem<your-probe-serial>1 out.log -- 'import machine' 'led = machine.Pin(("gpio0", 15), machine.Pin.OUT)' 'led.on(); print(led.value())' 'led.off(); print(led.value())' 'print(machine.ADC(("adc0", 2)).read_u16())'
   ```

   You should see: `out.log` shows `1` after led.on(), `0` after led.off(), then an ADC number (about 42,000 to 48,000 on a floating pin). The LED itself was not watched; the state is known from the read-back.

   If `AttributeError: 'Pin' object has no attribute 'toggle'`, use `led.value(1 - led.value())`. After a Ctrl-D soft reboot a pad you made an output stays one: use `machine.reset()`.

### Expected result

MicroPython 1.29.0 runs on the LP-EM-CC2340R5: a prompt at 115200 baud, DIO15 driven from Python, and the ADC read.

### What this shows

- MicroPython's own Zephyr port runs unmodified on a chip with 36 kB of RAM, once its `prj.conf` is replaced by one file that drops networking, sensors and file systems.
- The image is 174.8 kB of flash and 29.9 kB of RAM with a 20 kB Python heap (19.1 kB free after boot).
- Pin.irq (10 of 10 edges), PWM with duty_ns, SPI and I2C calls ran from the REPL.

### Limits

- That the LED blinked to the eye: the pin read-back changed and the LED was not watched.
- That SPI data is right: the loopback wire was not fitted; the HDC2080 read and I2C.scan() did not work.
- That the `duty_u16` form of PWM is right in MicroPython 1.29.0: it overflows at 1 kHz; use `duty_ns` or apply the one-cast patch in the folder.
- That it works on Windows or Linux: only an Intel Mac was used.
- There is no file system, so scripts are typed or pasted at the prompt and re-sent after each reset.
- Pin.irq loses its edge when Pin.init() is called on the same pin; call irq() again after init().
- An empty Python loop takes 0.63 microseconds a pass at 48 MHz.

### Numbers and sources

| | Value | Source |
|---|---|---|
| MicroPython | v1.29.0, commit 0fd6c573e, unmodified | `ports/micropython/zephyr-cc2340r5/README.md` |
| Zephyr | 4.4.2, Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler | `ports/micropython/zephyr-cc2340r5/README.md` |
| Image | 174.8 kB of flash (34.2 % of 512 kB), 29.9 kB of RAM (83.0 % of 36 kB), 20 kB heap |  |
| Free heap after boot | 19.1 kB of the 20 kB heap |  |
| Python speed | 10,000 empty loop passes in 6,323 microseconds; ADC.read_u16() about 7.2 microseconds |  |

## MicroPython on the LP-MSPM33C321A

MicroPython's own Zephyr port runs unmodified on the 160 MHz Cortex-M33 with its stock config, on Zephyr main plus TI's open pull requests.

Status: Could work. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/micropython/zephyr-mspm33c321a/AGENTS.md`):

```bash
git sparse-checkout add ports/micropython/zephyr-mspm33c321a
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | the board |
| A Mac to build and flash from | 1 | builds and flashes |

### Before you start

- A Mac or Linux machine; only an Intel Mac, macOS 15.7, was tried. git, `cmake` and `ninja` on the path, a Python 3.12 environment with `west`, the Zephyr SDK 0.17.4 GCC 12.2.0 `arm-zephyr-eabi` (`cc2340r5/zephyr/setup-intel-mac.sh` installs both), UniFlash 9.6.0 (DSLite). About 1.5 GB for the Zephyr tree and 0.3 GB for the build.
- Zephyr main at `1405adc9a604` plus TI's open pull requests for the UART, clock and GPIO drivers: Zephyr's released board has no serial port, GPIO, I2C or SPI driver. `zephyr-tree.sh` pins each pull request to a commit, and pull requests move.
- Close any CCStudio debug connection before step 4; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Clone MicroPython v1.29.0 (commit `0fd6c573e`) from a fresh folder outside this repository; nothing in it is edited.

   ```text
   git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git mp
   ```

   You should see: The `mp` folder holds MicroPython.

   If the clone fails, check the network; the tag is v1.29.0.

2. **Mac, terminal.** Build the Zephyr tree this port needs: Zephyr main plus the pinned pull requests, with `picolibc` and `cmsis_6`. Run it from `ports/micropython/zephyr-mspm33c321a`. The kinds are `uart`, `i2c` and `spi`; this version uses `uart`.

   ```text
   ./zephyr-tree.sh "$PWD/zw" uart
   ```

   You should see: About 10 minutes later the `zw` folder holds the merged Zephyr tree.

   If a later build stops with ``fatal error: alloca.h: No such file or directory``, the picolibc module is missing: `west update picolibc` (the script does it) and pass the folder as `PICO`.

3. **Mac, terminal.** Build MicroPython's `ports/zephyr` for `lp_mspm33c321a` with MicroPython's own `prj.conf` plus `mspm33c321a.conf` (watchdog off, GPIO, I2C, SPI).

   ```text
   MP="$PWD/mp" ZW="$PWD/zw" OUT="$PWD/out" KIND=uart ./build.sh
   ```

   You should see: About 4 minutes later `out/zephyr/zephyr.hex` exists: 292.3 kB of flash (28.5 % of 1 MB) and 83.6 kB of RAM (32.7 % of 256 kB) with the GPIO driver.

   If the build stops naming a variable, set it. If `machine_wdt.c` stops the build, `CONFIG_WATCHDOG=n` is missing; use `mspm33c321a.conf`.

4. **Mac, terminal.** Back up the board, pad the image to a multiple of 16 bytes (DSLite writes 128-bit words), convert it to hex at 0x10000000 and flash it. Restore later with the same `flash` command on a hex made from the saved `main-flash.bin`.

   ```text
   python3 tools/benchmark/backup.py save mspm33c321a --probe <your-probe-serial>
   python3 -c "b=open('out/zephyr/zephyr.bin','rb').read(); open('p.bin','wb').write(b+b'\xff'*(-len(b)%16))"
   arm-zephyr-eabi-objcopy -I binary -O ihex --change-addresses 0x10000000 p.bin p.hex
   DSLite flash -c mspm33c321a/MSPM33C321A_<your-probe-serial>.ccxml -e -f -v p.hex
   ```

   You should see: DSLite writes and verifies the 1 MB image in about 20 s.

   If DSLite says `Error in image size ... should be divisible by 16`, the padding step was skipped. If the next call says `Error initializing emulator (Error -261)`, run `xds110reset -s <your-probe-serial> -a toggle -d 200` and flash again.

5. **Mac, terminal.** Open the REPL on the probe's serial port (115200 baud) and read two pins with their pull resistors on: PA12 pulled up, then pulled down.

   ```text
   python3 ports/micropython/zephyr-mspm33c321a/repl.py /dev/cu.usbmodem<your-probe-serial>1 out.log --reset -- 'print(1+1)' 'import machine' 'print(machine.Pin(("gpioa", 12), machine.Pin.IN, machine.Pin.PULL_UP).value())' 'print(machine.Pin(("gpioa", 12), machine.Pin.IN, machine.Pin.PULL_DOWN).value())'
   ```

   You should see: `out.log` holds the banner `MicroPython v1.29.0 on 2026-10-25; zephyr-lp_mspm33c321a with mspm33c321a`, then `2`, then `1` for the pull-up and `0` for the pull-down.

   If `ValueError: device gpioc not found` appears, `CONFIG_GPIO` is off: build with `mspm33c321a.conf`. Send one-line statements; a multi-line `def` typed line by line is indented wrongly. If a read of `machine.I2C` hangs the console, press the reset button; use writes only.

### Expected result

MicroPython 1.29.0 prints its banner and a >>> prompt at 115200 baud on the LP-MSPM33C321A, pulled-up and pulled-down inputs read 1 and 0, and float math works.

### What this shows

- MicroPython's own Zephyr port runs unmodified, with its own `prj.conf` as shipped, on the Cortex-M33 chip.
- The image is 292.3 kB of flash and 83.6 kB of RAM with a 48 kB Python heap (47.1 kB free after boot).
- machine.Pin and float math run from the REPL on the `KIND=uart` image; machine.SPI calls ran from the `KIND=spi` image.

### Limits

- That machine.I2C works: it opens and writes return, but every read hangs until a hardware reset.
- That machine.ADC or machine.PWM work: Zephyr has no driver for this chip's ADC or PWM.
- That the LED blinked to the eye: the pin state is known from the output register and the LED was not watched.
- That the image from the fresh-folder build ran: it produced an image that was not flashed.
- Zephyr's released `lp_mspm33c321a` board has no serial port, GPIO, I2C or SPI driver; this port needs Zephyr main plus TI's open pull requests, and pull requests move.
- I2C and SPI cannot be in one image: pull requests 110526 and 119347 carry different copies of the UNICOMM driver, so each gets its own build (`KIND=i2c`, `KIND=spi`).
- `Pin.value()` reads 0 right after `led.on()`; read the output register instead.
- An empty Python loop costs 803 CPU cycles a pass at 160 MHz (5.0 microseconds); the cause is not found.

### Numbers and sources

| | Value | Source |
|---|---|---|
| MicroPython | v1.29.0, commit 0fd6c573e, unmodified | `ports/micropython/zephyr-mspm33c321a/README.md` |
| Zephyr | main 1405adc9a604 plus open pull requests 116768 and 118511 (UART console and GPIO; 110526 for I2C and 119347 for SPI, each with 118511), GCC 12.2.0 from Zephyr SDK 0.17.4 | `ports/micropython/zephyr-mspm33c321a/README.md` |
| Image | 292.3 kB of flash (28.5 % of 1 MB), 83.6 kB of RAM (32.7 % of 256 kB), 48 kB heap |  |
| Free heap after boot | 47.1 of 48 kB after gc.collect() |  |
| Python speed | 10,000 empty loop passes in 50,182 microseconds |  |
