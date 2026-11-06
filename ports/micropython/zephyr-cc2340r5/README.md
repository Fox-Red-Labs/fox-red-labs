# MicroPython on Zephyr for the LP-EM-CC2340R5

**Short answer.** MicroPython 1.29.0 runs on the LP-EM-CC2340R5 (36 kB RAM, 512 kB flash) through upstream MicroPython's own Zephyr port and upstream Zephyr 4.4.2. It gives a prompt on the probe's serial port at 115200 baud, drives a pin from Python and reads an ADC pin. The image is 174.8 kB of flash and 29.9 kB of RAM with a 20 kB Python heap, so it fits in the chip. MicroPython's stock Zephyr settings do not fit: they overflow the RAM by 46.8 kB. The fix is one config file and one devicetree overlay, both in this folder. Status: **Works** (one board).

Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-24 on an Intel Mac, macOS 15.7, MicroPython v1.29.0 (commit `0fd6c573e`, shallow clone, unmodified), Zephyr 4.4.2, GCC 12.2.0 from Zephyr SDK 0.17.4 through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, UniFlash 9.6.0 (DSLite), one LP-EM-CC2340R5 on an XDS110 probe.

## What ran

| Check | Result |
|---|---|
| Banner | `MicroPython v1.29.0 on 2026-10-24; zephyr-lp_em_cc2340r5 with cc2340r5` |
| First boot | The REPL answered on the first flash of the first image (core features); `print(1+1)` printed 2 |
| LED from the REPL | `led = machine.Pin(("gpio0", 15), machine.Pin.OUT)`, then `led.on()` reads back 1 and `led.off()` reads back 0 (DIO15 is the green LED). The blink itself was not watched (see Not checked) |
| ADC read | `machine.ADC(("adc0", 2)).read_u16()` on DIO5. With the pin pulled up through `machine.Pin` it read 65,471 (`read_uv()` 3,295,000 µV); pulled down, 16 (0 µV); driven high 65,503; driven low 0. A floating pin read 42,000 to 48,000 and drifted |
| Float | `2**0.5*1000` printed 1414.2135 |
| Python speed | An empty `for` loop of 10,000 passes took 6,323 µs (0.63 µs a pass at 48 MHz). A `Pin.value()` write took about 1.24 µs. One `ADC.read_u16()` took about 7.2 µs |
| Free heap after boot | 19.1 kB of the 20 kB heap (basic features); 15.2 kB of 16 kB (extra features, with `json`, `re`, `struct`) |
| Hard reset | `machine.reset()` restarts into the banner |

## Interrupts, PWM, SPI and I2C from the REPL

Checked 2026-10-25 with the same basic image (175 kB flash, 29.9 kB RAM); the board file already enables `pwm5`, `pwm6`, `spi0` and `i2c0`, so no overlay change was needed.

| Check | Result |
|---|---|
| `Pin.irq()` | `machine.Pin(("gpio0", 10), machine.Pin.IN, machine.Pin.PULL_UP).irq(handler)` on S1. With no hand at the button, `machine.mem32` flips only the pull field of the pin's IOC register (0x40003128), down then up, 5 times: the handler ran 10 of 10 expected. The same flips through `Pin.init()` gave 0 of 10 (Things to know) |
| `machine.PWM` with `duty_ns` | `machine.PWM(("pwm5", 2), freq=1000, duty_ns=250000)` on the red LED: LGPT1 period register 48,000, compare 12,000, then 36,000 for 750,000 ns. The LED was not seen |
| `machine.PWM` with `duty_u16` | Wrong in MicroPython 1.29.0: `duty_u16=16384` at 1 kHz gave compare 2,562 instead of 12,000. With `pwm-duty-u16-overflow.patch` (one cast) it gave 12,000, 36,000 and 48,000 for 16,384, 49,152 and 65,535; the patched image is 16 bytes larger |
| `machine.SPI` | `machine.SPI("spi0", baudrate=1000000).write_readinto(...)` completed; it read FF FF FF FF because MISO (pin 14, DIO12) is not connected. Data not checked |
| `machine.I2C` | With the Sensors BoosterPack seated and pin 27 (DIO5) high: `readfrom_mem(0x44, 0x7E, 2)` gave `b'TI'`, device 0x3001, and one reading of 1,596 lux. The HDC2080 (0x40) did not answer (`OSError: 5`), before or after a wire from pack pin 6 to GND; a one-byte read at every address from 0x08 to 0x77 found only 0x44. `scan()` returned `[]` with and without the pack, although 0x44 answers reads |

Status: **Works** for `Pin.irq` (software-driven edges), `PWM` (with `duty_ns`, or `duty_u16` after the patch) and the SPI and I2C calls. **Should work**: SPI data with a loopback wire (pin 15 to pin 14; the wire is not fitted), the HDC2080 read (the sensor did not answer) and `I2C.scan()` (returns an empty list, see "What went wrong").

## Sizes against the chip

| Image (config in this folder) | Flash | RAM | Python heap | Ran |
|---|---|---|---|---|
| MicroPython's own `prj.conf` as shipped | not linked | over by 46.8 kB | 48 kB default | no, link error |
| Core features, heap 8 kB | 172.0 kB (33.6 % of 512 kB) | 17.9 kB (49.7 % of 36 kB) | 8 kB | built, no `time` module |
| Basic features, heap 20 kB (`prj_cc2340r5.conf`) | 174.8 kB (34.2 %) | 29.9 kB (83.0 %) | 20 kB | yes |
| Extra features, heap 16 kB | 213.6 kB (41.7 %) | 26.0 kB (72.3 %) | 16 kB | yes |

Source: the `Memory region` table in each Zephyr link. The static RAM that is not heap is 9.9 to 10.0 kB in every build (Zephyr kernel, a 4 kB main stack, the console buffers), so the largest heap that links is about 26 kB. MicroPython's own guidance is 256 kB of flash and 16 kB of RAM at least; the basic build uses 68 % of that flash figure and has a 20 kB heap.

What was trimmed from MicroPython's `prj.conf` to make it fit: networking (IPv4, IPv6, UDP, TCP, sockets, DHCP, DNS), the sensor subsystem, the thread analyzer, net buffer pool tracking, and the FAT and littlefs file systems (`CONFIG_MICROPY_VFS_*`). The heap is `CONFIG_MICROPY_HEAP_SIZE`, a static array, so raising it is the only knob that moves RAM. There is no file system, so scripts are typed or pasted at the prompt.

## Take it with you

Copy these files into a folder of your own. They are the whole port; MicroPython and Zephyr stay as upstream ships them.

| File | What it is |
|---|---|
| `prj_cc2340r5.conf` | Replaces MicroPython's `prj.conf` (passed as `-DCONF_FILE`): networking, sensors, shell and file systems off, basic features, 20 kB Python heap (`CONFIG_MICROPY_HEAP_SIZE=20480`) |
| `lp_em_cc2340r5.overlay` | Adds the ADC channels 2, 3, 4 and 6 (DIO5, DIO2, DIO1, DIO25) for `machine.ADC` (passed as `-DEXTRA_DTC_OVERLAY_FILE`) |
| `pwm-duty-u16-overflow.patch` | Optional, one cast: fixes `machine.PWM(duty_u16=...)` in `ports/zephyr/machine_pwm.c`; `git apply` inside the MicroPython checkout |
| `repl.py` | Optional host helper: holds the console port open and sends lines to the REPL; needs `pyserial` only |

**What it needs.**

- A MicroPython v1.29.0 checkout (`git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git <MP>`; commit `0fd6c573e`).
- A Zephyr 4.4.2 west workspace and Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler (`ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`). The `lp_em_cc2340r5` board file is already in Zephyr 4.4.2, so no board file is added.
- No force-include flag and no include order to set. The conf must keep `CONFIG_MICROPY_CONFIGFILE="mpconfigport.h"`: the minimal config file fails to link on this Cortex-M0+ (see the table below).
- Flashing the `zephyr_crc32.hex` needs UniFlash DSLite and the board's `.ccxml`; the recipe with a backup is under "Flash and run".

**A different board in the same family** (another Zephyr board with an ADC): pass its name to `-b`. Copy `lp_em_cc2340r5.overlay` to `<board>.overlay` and change `io-channels` and the `channel@N` nodes to the board's ADC channel numbers (the number in the node is the channel, not the pin). Keep `prj_cc2340r5.conf` and change `CONFIG_MICROPY_HEAP_SIZE`: static RAM outside the heap was 9.9 to 10.0 kB, so the heap is roughly the chip's RAM minus 10 kB minus headroom. In MicroPython, pins are `("gpio0", n)`, `("adc0", channel)` and `("pwm5", channel)`, the Zephyr device label and number.

**Build from a fresh folder** (`<proj>` holds the two files; `<MP>` is the checkout; nothing in `<MP>` or the workspace is edited):

```bash
. <zephyr workspace>/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd <zephyr workspace>/zephyr
west build -p always -b lp_em_cc2340r5 <MP>/ports/zephyr -d <build> -- \
  -DCONF_FILE=<proj>/prj_cc2340r5.conf -DEXTRA_DTC_OVERLAY_FILE=<proj>/lp_em_cc2340r5.overlay
```

Built from a copy of only the conf and the overlay on 2026-10-25 (copy outside the repo, about 3 minutes, zero errors): 174.9 kB of flash (34.15 % of 512 kB) and 29.9 kB of RAM (82.99 % of 36 kB), the same as the "Basic features, heap 20 kB" row above. The image was not flashed from that build.

## Set up once

`build.sh` reads the variables below, falls back to the default shown, and stops with one line naming the variable when a path is missing.

| Variable | What it is | How to get it | Default folder | Size |
|---|---|---|---|---|
| `MP` | MicroPython v1.29.0 checkout (commit `0fd6c573e`) | `git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git $MP` | `frl-ports/micropython` on an external drive | 62 MB |
| `ZP` | Zephyr 4.4.2 west workspace | `cc2340r5/zephyr/setup-intel-mac.sh` (`NO_FLASH=1` stops after its build); it also installs `TC` and `VENV` | `zephyrproject` in your home folder | 3.6 GB |
| `TC` | Zephyr SDK 0.17.4 GCC 12.2.0 (`arm-zephyr-eabi`), used as a cross compiler | same script | `zephyr-toolchain` in your home folder | 1.1 GB |
| `VENV` | Python 3.12 venv with `west` | same script | `$ZP/.venv` | 0.3 GB |
| `OUT` | build folder | created by the build | external drive | about 60 MB |

Also needed: `ninja` and `cmake` on the path, UniFlash 9.6.0 (DSLite) and an XDS110 probe for flashing, and `pyserial` (`python3 -m pip install pyserial`) for `repl.py`. The board has no USB port: clip the probe on (`cc2340r5/AGENTS.md`).

## Build

```bash
ports/micropython/zephyr-cc2340r5/build.sh        # about 2.5 minutes; image at $OUT/zephyr/zephyr_crc32.hex
```

`build.sh` passes two files to `west build`: `prj_cc2340r5.conf` (replaces MicroPython's `prj.conf`) and `lp_em_cc2340r5.overlay` (ADC channels 2, 3, 4 and 6 on DIO5, DIO2, DIO1 and DIO25; the board file already routes those pins to the ADC). Neither the MicroPython tree nor the Zephyr tree is edited. The Zephyr board file `lp_em_cc2340r5` already exists upstream, so no board file was added. For the `duty_u16` fix, run `git apply <repo>/ports/micropython/zephyr-cc2340r5/pwm-duty-u16-overflow.patch` inside `$MP` first.

## Flash and run

Back up first and restore at the end:

```text
DSLITE=<UniFlash folder>/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite; CCXML=cc2340r5/CC2340R5.ccxml; BK=<backup folder outside the repo>
$DSLITE memory -c $CCXML -r 0x0,0x20000 -o $BK/main_flash.bin
$DSLITE memory -c $CCXML -r 0x4E020000,0x200 -o $BK/ccfg.bin
python3 cc2340r5/oad/images.py merge $BK/restore.hex $BK/main_flash.bin@0x0 $BK/ccfg.bin@0x4E020000
$DSLITE flash -c $CCXML -e -f -u $OUT/zephyr/zephyr_crc32.hex       # the _crc32 file carries the boot checksums
```

Restore with the same `flash` command on `restore.hex`, then read both areas again; they match the backup byte for byte. Console: `/dev/cu.usbmodem<probe serial>1`, 115200 baud. `repl.py` holds the port open and sends lines:

```bash
python3 ports/micropython/zephyr-cc2340r5/repl.py /dev/cu.usbmodem<probe serial>1 out.log --reset -- 'import machine' 'print(machine.ADC(("adc0", 2)).read_u16())'
```

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| ``ld.bfd: region `RAM' overflowed by 47888 bytes`` | MicroPython's `prj.conf` turns on networking and a 48 kB heap | Use `prj_cc2340r5.conf` |
| ``relocation truncated to fit: R_ARM_THM_JUMP11 against symbol `nlr_push_tail` `` | Using `-DCONF_FILE=prj_minimal.conf` selects `mpconfigport_minimal.h`, which lacks `MICROPY_NLR_THUMB_USE_LONG_JUMP` on a Cortex-M0+ | Use the regular `mpconfigport.h` (`CONFIG_MICROPY_CONFIGFILE="mpconfigport.h"`, as in this folder's conf) |
| ``ImportError: no module named 'time'`` | The core-features level leaves out `time` | Use basic features or higher |
| ``AttributeError: 'Pin' object has no attribute 'toggle'`` | The Zephyr port's `Pin` has `on`, `off`, `value` only | `led.value(1 - led.value())` |
| `machine.ADC` reads 0 or 65,5xx after a soft reboot | The soft reboot (Ctrl-D) does not reset the hardware: a pad you made a GPIO output stays one | `machine.reset()` for a hard reset |
| `PWM(..., duty_u16=n)` gives a much smaller duty than asked at low frequencies (1 kHz, 16,384 gave 2,562 of 48,000 counts) | `machine_pwm.c` multiplies `duty_u16` by the period in nanoseconds in 32 bits, which overflows whenever the product passes 4,294,967,295 (any frequency under about 15.3 kHz at full duty) | Use `duty_ns`, or apply `pwm-duty-u16-overflow.patch` to the MicroPython tree before building |
| A pin's `irq()` stops firing after `Pin.init()` on the same pin | Zephyr 4.4.2's `gpio_cc23x0` driver rewrites the pin's IOC register without the edge-detect bits | Call `irq()` again after `init()` |
| `PWM(("pwm5", 0), ...)` changes nothing on the red LED | The red LED pin is wired to LGPT1 channel 2; the tuple's second item is the timer channel | `PWM(("pwm5", 2), ...)` for red, `PWM(("pwm6", 0), ...)` for green |
| `I2C.scan()` returns `[]` while `readfrom_mem` at a known address works | The scan's empty write fails on Zephyr 4.4.2's CC23x0 I2C driver (not traced) | Probe with `readfrom_mem(addr, reg, 1)` inside `try` |
| `machine.reset_cause()` prints `Warning: machine_reset_cause is not implemented` | Not in the Zephyr port | Ignore |

Pins are named by Zephyr devicetree label and number: `machine.Pin(("gpio0", 15), ...)`, `machine.ADC(("adc0", 2))` where 2 is the ADC channel number from the overlay, not the DIO number.

## Not checked

- The LED blink was not watched; LED state is known only from the pin read-back through `Pin.value()`. **Unknown.**
- `machine.UART` was not exercised. SPI read data waits on a loopback wire; the HDC2080 read waits on its power pin being tied low. The interrupt was driven from software, not by pressing S1, and the PWM LED was not watched.
- The extra-features image is 213.6 kB; the full-features level was not built.

## License and source

MicroPython is MIT-licensed and used unmodified from its v1.29.0 tag, apart from the optional `pwm-duty-u16-overflow.patch`. Zephyr 4.4.2 is Apache-2.0. `prj_cc2340r5.conf`, `lp_em_cc2340r5.overlay`, `repl.py` and `build.sh` are this repo's. What a submission to MicroPython's own tree would contain: `../../../portability/zephyr-mspm0/upstream/MICROPYTHON-CC2340R5.md`.
