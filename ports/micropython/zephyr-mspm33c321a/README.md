# MicroPython on Zephyr for the LP-MSPM33C321A

**Short answer.** MicroPython 1.29.0 runs on the LP-MSPM33C321A (256 kB RAM, 1 MB flash) through upstream MicroPython's own Zephyr port, unmodified, with MicroPython's own `prj.conf` as it ships. It prints its banner and a `>>>` prompt on the probe's serial port at 115200 baud, drives a pin from Python, reads a pulled-up and a pulled-down input, and does float math. The image is 292.3 kB of flash (28.5 % of 1 MB) and 83.6 kB of RAM (32.7 %), with a 48 kB Python heap. The catch: Zephyr's released `lp_mspm33c321a` board has no serial port, GPIO, I2C or SPI driver. This port builds against Zephyr `main` plus TI's open pull requests for those drivers, and `machine.ADC` and `machine.PWM` have no driver at all. Status: **Works** for the REPL, `Pin`, float and SPI calls. **Should work** once the blocker is cleared: SPI data (needs a wire from PB12 to PB14). `machine.I2C` opens and writes return, but reads hang because of a driver bug (see "What went wrong"). **Not for this chip** on the Zephyr path: `machine.ADC`, `machine.PWM`.

Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-25 on an Intel Mac, macOS 15.7, MicroPython v1.29.0 (commit `0fd6c573e`, unmodified), Zephyr `main` at `1405adc9a604` plus the pull requests below, GCC 12.2.0 from Zephyr SDK 0.17.4 through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, picolibc, UniFlash 9.6.0 (DSLite 21.0.0.4106), one LP-MSPM33C321A on its XDS110 probe.

## What ran

| Check | Result |
|---|---|
| Banner | `MicroPython v1.29.0 on 2026-10-25; zephyr-lp_mspm33c321a with mspm33c321a`, then `print(1+1)` printed 2 |
| Pin write | `led = machine.Pin(("gpioc", 26), machine.Pin.OUT)` (red LED, PC26); `led.on()` sets bit 26 of GPIOC DOUT31_0 (0x4000000) and `led.off()` clears it. `led.value()` reads 0 both times (What went wrong). The LED itself was not watched |
| Pull-up and pull-down | `machine.Pin(("gpioa", 12), machine.Pin.IN, machine.Pin.PULL_UP)` reads 1; with `PULL_DOWN` it reads 0 (pad unconnected) |
| ADC | No driver for this chip's HSADC on the Zephyr path, so no read | What went wrong |
| Float | `2**0.5*1000` printed 1414.2135 |
| Python speed | An empty `for` loop of 10,000 passes took 50,182 µs: 5.0 µs a pass, 803 CPU cycles at 160 MHz. The CC2340R5 port took 0.63 µs a pass at 48 MHz. The host clock agrees (400,000 passes in 1.96 s). With both caches switched on from the REPL the loop took 453 cycles a pass. The cause of the rest is not found |
| `Pin.value()` write | About 10 µs a call (15.5 µs for the call and loop, minus the empty loop) |
| Free heap after boot | 47.1 of 48 kB after `gc.collect()` |
| `machine.SPI` | `machine.SPI("unicomm13_0_spi", baudrate=1000000).write_readinto(...)` and `read(4)` completed; both read 00 00 00 00 because POCI (PB14) is not wired. Data not checked |
| `machine.I2C` | `machine.I2C("unicomm13_0_i2c")` opens (SDA PB12, SCL PB13) and `writeto(0x10, b"\x00")` returns 1 although nothing sits at 0x10. Every read hangs: `readfrom(0x44, 1)`, `readfrom(0x10, 1)` and `readfrom_mem(0x44, 0x7E, 2)` stop the console until a hardware reset |

Pins are named by Zephyr devicetree label and number: `("gpioa", 12)` is PA12, `("gpioc", 26)` is PC26. Buses are named by their node label: `"unicomm13_0_i2c"`, `"unicomm13_0_spi"`.

## Sizes against the chip

| Image | Flash | RAM | Python heap | Ran |
|---|---|---|---|---|
| MicroPython's `prj.conf` as shipped, watchdog off, no GPIO | 289.7 kB (28.3 % of 1 MB) | 83.5 kB (32.6 % of 256 kB) | 48 kB | yes, banner and REPL |
| Same plus GPIO | 292.3 kB (28.5 %) | 83.6 kB (32.7 %) | 48 kB | yes, `Pin` |
| Same on the I2C pull request | 294.5 kB (28.8 %) | 83.6 kB (32.7 %) | 48 kB | yes, I2C open and write; reads hang |
| Same on the SPI pull request | 294.4 kB (28.8 %) | 83.7 kB (32.7 %) | 48 kB | yes, `SPI` |

Source: the `Memory region` table in each Zephyr link. Nothing was trimmed: MicroPython's networking, sensor, thread-analyzer and file-system settings all fit in 256 kB of RAM, which is why this folder has no replacement `prj.conf` (the CC2340R5 port needs one).

## What the Zephyr tree has to carry

Zephyr `main` at `1405adc9a604` has a stub `lp_mspm33c321a` board: flash, RAM and nothing else (no `zephyr,console`, GPIO nodes disabled, no clock setup, no UART). `zephyr-tree.sh` builds the tree this port needs from `main` and these open pull requests, each pinned to a commit:

| Piece | Pull request | Commit | What it adds |
|---|---|---|---|
| UART console, 160 MHz PLL clock, pin control | [116768](https://github.com/zephyrproject-rtos/zephyr/pull/116768) (carries the pull requests it depends on) | `9975d063bfeb` | UNICOMM12 UART on PA10 and PA11 as `zephyr,console`, 115200 8N1 |
| GPIO | [118511](https://github.com/zephyrproject-rtos/zephyr/pull/118511) | `947aac340145` | `gpioa`, `gpiob`, `gpioc`, the LEDs and buttons |
| I2C (alternative base) | [110526](https://github.com/zephyrproject-rtos/zephyr/pull/110526) | `3868a23ad47a` | UNICOMM I2C controller and target |
| SPI (alternative base) | [119347](https://github.com/zephyrproject-rtos/zephyr/pull/119347) | `41e2efb5c5d8` | UNICOMM SPI controller |

The I2C and SPI pull requests each carry their own copy of the UNICOMM driver and conflict with each other (add/add in `drivers/misc/ti_unicomm/ti_unicomm.c` and `drivers/serial/uart_mspm0.c`), so each gets its own image. The GPIO pull request conflicts with all three in the board file and the SoC `dtsi`; `zephyr-tree.sh` keeps both sides. Pull requests move, so the pinned commits matter.

## Take it with you

You need: a Mac or Linux machine, git, `cmake` and `ninja` on the path, a Python 3.12 venv with `west` (`VENV`), the Zephyr SDK 0.17.4 GCC 12.2.0 `arm-zephyr-eabi` (`TC`; `cc2340r5/zephyr/setup-intel-mac.sh` installs it and the venv), and UniFlash 9.6.0 (DSLite) with an XDS110 probe to flash. Disk: about 1.5 GB for the Zephyr tree and 0.3 GB for the build.

The files, all in this folder (copy these and nothing else from the repo):

| File | What it is |
|---|---|
| `zephyr-tree.sh` | Clones Zephyr, checks out `main` at `1405adc9a604`, merges the pinned pull requests (`uart`, `i2c` or `spi`), runs `west update cmsis_6 picolibc` |
| `build.sh` | Builds MicroPython's `ports/zephyr` for `lp_mspm33c321a` with named variables (`MP`, `ZW`, `TC`, `VENV`, `PICO`, `OUT`, `KIND`) |
| `mspm33c321a.conf` | Added to MicroPython's `prj.conf`: watchdog off, GPIO, I2C, SPI |
| `overlay-uart.overlay`, `overlay-i2c.overlay`, `overlay-spi.overlay` | Devicetree additions per kind: the I2C and SPI pins on UNICOMM13_0 (SDA or PICO PB12, SCL or SCLK PB13, POCI PB14) |
| `repl.py` | Holds the serial port open, sends lines, saves the reply |

From a fresh folder outside this repo:

```bash
git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git mp     # commit 0fd6c573e
./zephyr-tree.sh "$PWD/zw" uart                    # about 10 minutes; kinds: uart, i2c, spi
MP="$PWD/mp" ZW="$PWD/zw" OUT="$PWD/out" KIND=uart ./build.sh       # about 4 minutes; image at out/zephyr/zephyr.hex
```

`build.sh` stops with one line naming the variable when a path is missing. The same steps were run from a scratch copy of only the listed files on 2026-10-25 and built.

## Flash and run

Back up first, and put the board back at the end:

```text
python3 tools/benchmark/backup.py save mspm33c321a --probe <probe serial>              # main flash and NONMAIN, each read twice
# DSLite writes 128-bit words: pad the image to a multiple of 16 bytes and give it its address
python3 -c "b=open('out/zephyr/zephyr.bin','rb').read(); open('p.bin','wb').write(b+b'\xff'*(-len(b)%16))"
arm-zephyr-eabi-objcopy -I binary -O ihex --change-addresses 0x10000000 p.bin p.hex
DSLite flash -c mspm33c321a/MSPM33C321A_<probe serial>.ccxml -e -f -v p.hex             # 1 MB write and verify, about 20 s
```

Restore with the same `flash` command on a hex made from the saved `main-flash.bin` the same way, then `backup.py verify <backup folder>`; the read-back matches the backup byte for byte. Read the 1 MB main flash back through the backup tool, not DSLite's `memory` command: DSLite cannot read the last 512 bytes of main flash ("Memory map prevented reading 0x10100000"). Console: `/dev/cu.usbmodem<probe serial>1`, 115200 baud, through the UNICOMM12 UART on PA10 and PA11 (the board's backchannel serial port, jumpers as shipped). `repl.py` holds the port open:

```bash
python3 repl.py /dev/cu.usbmodem<probe serial>1 out.log --reset -- 'import machine' 'print(machine.Pin(("gpioc", 26), machine.Pin.OUT).value())'
```

Send one-line statements. A multi-line `def` typed line by line is auto-indented wrongly; wrap it as `exec("def f():\n ...")`.

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| ``fatal error: alloca.h: No such file or directory`` | The trimmed Zephyr workspace had no picolibc module | `west update picolibc` (`zephyr-tree.sh` does it) and pass the folder as `PICO` |
| The board builds but prints nothing | Zephyr `main`'s `lp_mspm33c321a` has no UART, GPIO or clock setup | Build in the tree from `zephyr-tree.sh` |
| ``ValueError: device gpioc not found`` | `CONFIG_GPIO` is off; MicroPython's `prj.conf` does not turn it on | Use `mspm33c321a.conf` |
| ``machine_wdt.c`` stops the build | The board has no `watchdog0` alias | `CONFIG_WATCHDOG=n` (in `mspm33c321a.conf`) |
| `Flash Programmer: Error in image size. Length of block is 25172, but it should be divisible by 16` | DSLite writes 128-bit words and the Zephyr hex ends mid-word | Pad the binary with 0xFF and convert it to hex as above. If you stopped DSLite midway and the next call says `Error initializing emulator (Error -261)`, run `xds110reset -s <serial> -a toggle -d 200` and flash again |
| `led.value()` reads 0 right after `led.on()` | The GPIO driver leaves the input stage of an output pin off, so `Pin.value()` reads the input register | Read the output register: `machine.mem32[0x400f4000 + 0x1280]` for GPIOC |
| `machine.I2C(...).readfrom(...)` never returns and the console goes silent | The UNICOMM I2C driver in pull request 110526 does not finish a read (not traced); a write to an absent address returns 1 instead of an error | Press the reset button or run `xds110reset`; use writes only until the driver is fixed |
| Python loops run at about 5 µs a pass | Not found. Both caches off costs 803 cycles a pass, on costs 453; the host clock agrees with the 160 MHz timer | Set `FRI FRD_INTF_CTRL` (0x4002900C) bits 1 and 2 from the REPL for 1.8 times; clear them before you reflash, because stale cache lines can start the old program |
| `machine.ADC` and `machine.PWM` have no device to open | Zephyr has no HSADC driver (`drivers/adc` holds only the ADC12 driver) and its PWM driver includes MSPM0 DriverLib's `dl_timera.h`, which does not exist for this chip | Use TI's DriverLib examples for ADC and PWM on this chip, or wait for the pull requests (119487 adds timers) |
| I2C and SPI cannot be in one image | Pull requests 110526 and 119347 each carry a different copy of the UNICOMM driver | Build two images (`KIND=i2c`, `KIND=spi`) |

## Not checked

- The LED was not watched; the pin state is known from the output register. **Unknown.**
- SPI data waits on a wire from PB12 to PB14. I2C reads wait on a driver fix; whether the Educational BoosterPack was on the board is not confirmed.
- `machine.UART`, `machine.Timer` and `Pin.irq()` were not exercised. The 4-to-1 slowdown against the CC2340R5 loop is measured, not explained.
- The fresh-folder build produced an image; that image was not flashed.

## License and source

MicroPython is MIT-licensed and used unmodified from its v1.29.0 tag. Zephyr is Apache-2.0; the pull requests belong to their authors and are fetched by `zephyr-tree.sh`, not copied here. `build.sh`, `zephyr-tree.sh`, `mspm33c321a.conf`, the three overlays and `repl.py` are this repo's.
