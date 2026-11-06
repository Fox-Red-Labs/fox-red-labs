# What an AI coding assistant should know before running MicroPython on a TI LP-MSPM33C321A LaunchPad development kit through Zephyr

That Zephyr's released board file for this kit has no serial port, GPIO, I2C or SPI driver, so the build needs Zephyr `main` plus four open TI pull requests, and that `machine.ADC` and `machine.PWM` have no driver at all. This file is the short list. Each item comes from runs on one board on 2026-10-25.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [LP-MSPM33C321A FAQ](../../../mspm33c321a/FAQ.md) in the Fox Red Labs repo. In short: **Zephyr** is an open-source real-time operating system; **picolibc** is its small C library; **DSLite** is TI's command-line flasher and memory reader; **REPL** is MicroPython's prompt; **UNICOMM** is the block that runs as a UART, SPI or I2C port. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `ports/micropython/zephyr-mspm33c321a/` are in the Fox Red Labs repo; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM33C321A: Arm Cortex-M33 at 160 MHz with FPU, 1 MB flash, 256 kB RAM (`mspm33c321a/README.md`) |
| Console | UNICOMM12 UART, TX PA10 and RX PA11, 115200 8N1, reaches the Mac as `/dev/cu.usbmodem<probe serial>1` |
| LEDs | Red PC26, green PC27, blue PA2 (`mspm33c321a/AGENTS.md`) |
| I2C and SPI pins | UNICOMM13_0: SDA or PICO PB12, SCL or SCLK PB13, POCI PB14 (`ports/micropython/zephyr-mspm33c321a/overlay-i2c.overlay`, `overlay-spi.overlay`) |
| Fit | 292.3 kB of flash and 83.6 kB of RAM with MicroPython's own `prj.conf` and its 48 kB heap |
| Not on the chip, Zephyr path | No HSADC driver, no PWM driver for the M33 timers |

## Start here

| Step | Command |
|---|---|
| Probe serial `<serial>` | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| MicroPython | `git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git mp` (commit `0fd6c573e`) |
| Zephyr tree | `ports/micropython/zephyr-mspm33c321a/zephyr-tree.sh "$PWD/zw" uart` (kinds `uart`, `i2c`, `spi`; about 10 minutes) |
| Build | `MP=$PWD/mp ZW=$PWD/zw OUT=$PWD/out KIND=uart ports/micropython/zephyr-mspm33c321a/build.sh` (about 4 minutes) |
| Back up | `python3 tools/benchmark/backup.py save mspm33c321a --probe <serial>` |
| Pad the image | Pad `out/zephyr/zephyr.bin` with 0xFF to a multiple of 16 bytes, then `arm-zephyr-eabi-objcopy -I binary -O ihex --change-addresses 0x10000000 p.bin p.hex` |
| Flash | `DSLite flash -c mspm33c321a/MSPM33C321A_<serial>.ccxml -e -f -v p.hex` (about 20 s) |
| Talk to it | `python3 ports/micropython/zephyr-mspm33c321a/repl.py /dev/cu.usbmodem<serial>1 out.log --reset -- 'import machine'` |
| Restore | Flash the saved `main-flash.bin` the same way, then `python3 tools/benchmark/backup.py verify <backup folder> --probe <serial>` |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| ``fatal error: alloca.h: No such file or directory`` | The Zephyr workspace has no picolibc module | `west update picolibc`, pass the folder as `PICO` | The build reaches the link |
| Build succeeds, serial port silent | `lp_mspm33c321a` in Zephyr `main` has no UART node or clock setup | Use the tree from `zephyr-tree.sh` | Banner `MicroPython v1.29.0 ... zephyr-lp_mspm33c321a` |
| ``ValueError: device gpioc not found`` | `CONFIG_GPIO` is off | Add `mspm33c321a.conf` | `Pin(("gpioc", 26), Pin.OUT)` opens |
| Build stops in `machine_wdt.c` | No `watchdog0` alias on this board | `CONFIG_WATCHDOG=n` | The link finishes |
| `Flash Programmer: Error in image size. Length of block is 25172, but it should be divisible by 16` | DSLite writes 128-bit words | Pad the binary, convert to hex as above | `Program verification successful` |
| `Error initializing emulator: (Error -261 ...)` after DSLite was stopped midway | The probe was left mid-operation | `xds110reset -s <serial> -a toggle -d 200`, flash again | The flash verifies |
| `Pin.value()` reads 0 on an output pin that is high | The input stage of an output pin is off | Read GPIOx DOUT31_0 at base + 0x1280 | 0x4000000 after `led.on()` |
| `I2C.readfrom(...)` hangs the console | The UNICOMM I2C read path of pull request 110526 | Hardware reset; do not read until the driver changes | Console answers again |
| The REPL answers but loops take 5 µs a pass | Not found; caches off is 803 cycles a pass | Switch the caches on from the REPL for 1.8 times; clear them before reflashing | 453 cycles |
| DSLite `memory -r 0x10000000,1048576` stops 512 bytes short | DSLite cannot read the last 512 bytes of main flash | Verify the restore with `backup.py verify` | `MATCH` for main flash and NONMAIN |

## Rules for the assistant

1. **Give DSLite a hex padded to 16 bytes.** An unpadded Zephyr hex fails at the last block and a stopped write can leave the probe needing a reset.
2. **Write one-line statements to the REPL.** A multi-line `def` is auto-indented wrongly; use `exec("def f():\n ...")`.
3. **Never read an I2C device in this port until the driver changes.** A read stops the console; only a reset brings it back.
4. **Back up before the first write and verify at the end.** The restore here matched byte for byte.
