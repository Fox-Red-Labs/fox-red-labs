# ST NUCLEO-U083RC: from plug-in to a changed LED blink on a Mac

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer: 14 seconds with ST's command-line tools, once they are installed; the install took about 35 minutes. With Zephyr already installed, 7 min 51 s.** `./blink.sh 250` builds ST's blink example with a 250 ms delay, flashes it and verifies it. Measured on 2026-10-08 on an Intel Mac with macOS 15.7.9 (Apple Silicon not run). Every number is in [RESULTS-nucleo-u083rc.md](RESULTS-nucleo-u083rc.md).

The AES result, since it is why the board is here: through each vendor's own driver calls, its hardware AES and the TI LP-MSPM0G3507's gain about the same over the tiny-AES-c software library on 1 kB (19.6 to 37.1 times here, 20.3 to 36.5 there; ECB encrypt 19.6 here and 24.5 there; the MSPM0 figures are from one run). Writing the AES registers directly, which the MSPM0 run did not measure, gives 65 to 74 times here. The compilers and clocks differ: GCC 12.2 at 48 MHz here, TI Arm Clang at 80 MHz there.

This board is the low-cost ST Cortex-M0+ with a hardware AES block and a true random-number generator, so the "crypto with and without the accelerator" test is a fair M0+ against M0+ comparison with the LP-MSPM0G3507 (whose hardware AES ran 20.3 to 36.5 times faster than tiny-AES-c, `data/results/mspm0g3507.csv`). It is also ST's ultra-low-power line; power figures need a meter. It is STM32U0, not STM32G0: every table says so.

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. Words used here: **STLINK-V2EC** is ST's debug probe built into the board, on USB-C (the board marks its socket STLINK; tools call it ST-LINK); **STM32CubeCLT** is ST's command-line tool set; **STM32CubeU0** is ST's free example and driver package for this chip family; **AES** is the standard block cipher, and **ECB** and **CBC** are two ways of using it on more than one 16-byte block; **tiny-AES-c** is a small open-source AES library in plain C; **TRNG** is a true random-number generator; **MSI** is the chip's internal clock oscillator, which ST's examples set to 48 MHz. STM32U083 and STM32U073 are one chip family (STM32U0), and ST's tools print "STM32U073/STM32U083".

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-nucleo-u083rc.md` (the measured results); `blink.sh`; `prebuild.sh` (builds every target that needs no board; it writes outside the repository); `stm32-originals/` ST's own examples on this board. Results are rows in `data/results/nucleo-u083rc.csv` at the top of the repository, not in this folder.

**Not in this folder:** ST's STM32Cube packages (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add nucleo-u083rc tools data nucleo-g071rb mspm0g3507/mcu-bench
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact |
|---|---|
| Chip and core | STM32U083RC: Arm Cortex-M0+ up to 56 MHz with a 1 kB instruction cache (ST data sheet DS14463 Rev 2, read 2026-10-07) |
| Memory | 256 kB flash, 40 kB RAM with parity (data sheet) |
| On the chip | AES-128 and AES-256 in hardware, a TRNG, a 12-bit ADC, one DAC channel, USB full-speed device, a segment-LCD driver (data sheet) |
| ST's own figures | 134 CoreMark (2.4 per MHz at 56 MHz), 52 µA per MHz running, 16 nA in Shutdown (data sheet) |
| Debugger | STLINK-V2EC probe on USB-C, socket CN1 marked STLINK (board MB1932, user manual UM3256 Rev 2) |
| LEDs and buttons | Green user LED LD4 on PA5; blue user button B1 on PC13 (UM3256) |
| Headers | ARDUINO Uno V3 sockets and ST morpho pins (UM3256) |
| Serial | A serial port appears on the Mac next to the ST-LINK (USB 0483:3752) |

## Ten-minute path (Zephyr, measured 2026-10-08: 7 min 51 s from plug-in to a changed blink)

You need a USB-C cable and, for this path, nothing from ST: Zephyr 4.4.2 with GCC 12.2 and the open-source OpenOCD flasher, **already installed**. The 7 min 51 s does not include installing them. This repo's install script for an Intel Mac, `mspm0g3507/zephyr/setup-intel-mac.sh`, put all three on this Mac in about 9.5 minutes (timed on a TI board on 2026-10-05; not timed for this board). All steps run in the Mac's Terminal, from the `nucleo-u083rc` folder of this repo.

1. Plug the cable into CN1, the socket marked STLINK. LD3 turns green and LD4 blinks: ST's demo. The Mac sees an ST-LINK (USB 0483:3752) and a serial port; no USB drive appears.
2. Fetch the sources (public GitHub, no login; 45 s here): `./prebuild.sh --fetch-sources`
3. Copy Zephyr's blink sample, set it to 250 ms and build it (43 s here):

```bash
cp -R ~/zephyrproject/zephyr/samples/basic/blinky /tmp/blinky && sed -i '' 's/SLEEP_TIME_MS   1000/SLEEP_TIME_MS   250/' /tmp/blinky/src/main.c
cd ~/zephyrproject/zephyr && source ../.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-
west build -p always -b nucleo_u083rc /tmp/blinky -d /tmp/blinky-build
```

4. Flash it with OpenOCD (1.2 s here). Zephyr's board file for this board has no OpenOCD runner, so call OpenOCD directly:

```text
OCD=~/zephyr-toolchain/xpack-openocd-0.12.0-7
$OCD/bin/openocd -s $OCD/openocd/scripts -f interface/stlink.cfg -c "transport select swd" -f target/stm32u0x.cfg -c "program /tmp/blinky-build/zephyr/zephyr.elf verify reset exit"
```

LD4 now blinks four times as fast as Zephyr's default (on 250 ms, off 250 ms). When it changes speed, the path works.

If OpenOCD stops with `invalid command name "swd"`, or says `couldn't bind gdb to socket on port 3333`, the [FAQ](FAQ.md) has the fix for each.

ST's own first-hour path for a Mac is STM32CubeIDE (UM3256 section 1 lists it). It needs 6 GB of free disk.

## One-command build (ST's command-line tools, 14 s after the install)

```bash
./blink.sh 250
```

Run it from the `nucleo-u083rc` folder. It builds ST's `GPIO_IOToggle` example with the delay set to 250 ms using ST's GCC from STM32CubeCLT, flashes it with ST's `STM32_Programmer_CLI` and verifies it (6 s build, 5 s flash here). It needs STM32CubeCLT in `/opt/ST` and ST's examples from `./prebuild.sh --fetch-sources`.

`./prebuild.sh` builds everything else that needs no board into a build folder outside the repository (the script prints its path): Zephyr blink, ST's versions of the five examples ported to the LP-MSPM0G3507, ST's AES and random-number examples, CoreMark with three compilers, all four room-occupancy models and every ST example for this board (204 built; 6 secure-boot and option-byte projects skipped on purpose). It refuses to run while the Mac is busy.

Installing STM32CubeCLT 1.22.0 took about 35 minutes of one person's time here (an estimate; the clicks on st.com were not stamped): a guest download from st.com, an installer that macOS 15 blocks because it is unsigned, and 3.4 GB of free disk. The [FAQ](FAQ.md) has the fix for each.

## What was measured

All rows measured 2026-10-08 on an Intel Mac, macOS 15.7.9, unless the Level column says otherwise. Every number and method: [RESULTS-nucleo-u083rc.md](RESULTS-nucleo-u083rc.md).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Plug-in to a changed blink, Zephyr | 7 min 51 s (471 s) | Zephyr 4.4.2, GCC 12.2 and OpenOCD already installed; install not included | measured | `time_to_blink` (zephyr) in `data/results/nucleo-u083rc.csv` |
| Zephyr blink build and flash | 43 s build, 1.2 s flash | same setup | measured | `blink_build`, `blink_flash` (zephyr, openocd) |
| Plug-in to a changed blink, ST command-line tools | 14 s | `./blink.sh 250`, STM32CubeCLT installed; 6 s build, 5 s flash | measured | `time_to_blink`, `blink_build`, `blink_flash` (cli) |
| Installing STM32CubeCLT 1.22.0 | about 35 minutes | one person's time; an estimate, the clicks on st.com were not stamped; 3.4 GB of free disk | claim | Stopwatch estimate, 2026-10-08 |
| ST examples built | 204 built, 6 secure-boot and option-byte projects skipped on purpose | every ST example for this board | computed | `examples_built` (stm32cubeu0-1.3.0) |
| CoreMark, GCC 12.2, 48 MHz | 97.3 per second (2.03 per MHz) | -O3 with loop unrolling, bare metal | measured | `coremark` (gcc-bare-o3-unroll-48mhz) |
| CoreMark, ST's published figure | 134 (2.4 per MHz at 56 MHz) | compiler not stated here | claim | ST data sheet DS14463 Rev 2 |
| Hardware AES against tiny-AES-c, 1 kB, through ST's driver calls | 19.6 to 37.1 times; ECB encrypt 19.6 | GCC 12.2 at 48 MHz; the TI LP-MSPM0G3507 gains 20.3 to 36.5 times with TI Arm Clang at 80 MHz (one MSPM0 run) | measured | `aes_speedup` (1kb-gcc12-o3-hal) |
| Hardware AES against tiny-AES-c, 1 kB, writing the AES registers directly | 65 to 74 times | the MSPM0 run did not measure this | measured | `aes_speedup` (1kb-gcc12-o3-reg) |

Also in the results page: ST's own examples on this board ([stm32-originals/README.md](stm32-originals/README.md)); the experiments [ST's AES example](../docs/experiments/2026-10-08-nucleo-u083rc-st-aes-example.md), [all 40 kB of RAM](../docs/experiments/2026-10-08-nucleo-u083rc-40kb-ram.md) and [the flash wait state](../docs/experiments/2026-10-08-nucleo-u083rc-flash-cache.md).

## If you see this

- If you see `stm32u0x.cfg:29: Error: invalid command name "swd"`, choose the transport before the target file: `-f interface/stlink.cfg -c "transport select swd" -f target/stm32u0x.cfg`.
- If you see `couldn't bind gdb to socket on port 3333`, another OpenOCD holds the port. Add `-c "gdb port disabled; tcl port disabled; telnet port disabled"` before the interface file.
- If you see `Examination failed` after ST's `FLASH_EraseProgram` example, the board is not broken: wait a few minutes, or unplug the USB cable and plug it back in, then try again.

The rest, with causes: [FAQ](FAQ.md).

## Limits

Not run: power (needs a meter), the wired loop tests (DAC to ADC on PA4 to PA0, UART loop on D8 to D2), the button examples, ST's own first-hour path with STM32CubeIDE (needs 6 GB of free disk) and Apple Silicon.

## Where these facts come from

Checked 2026-10-08 on an Intel Mac, macOS 15.7.9 (Apple Silicon not run). Tools: Zephyr 4.4.2 with GCC 12.2; xPack OpenOCD 0.12.0-7; STM32CubeCLT 1.22.0 (ST's GCC, `STM32_Programmer_CLI`); STM32CubeU0 1.3.0 examples (sweep id `stm32cubeu0-1.3.0`). Chip and board facts: ST data sheet DS14463 Rev 2 (read 2026-10-07) and user manual UM3256 Rev 2. Files: `data/results/nucleo-u083rc.csv`, `RESULTS-nucleo-u083rc.md`, `AGENTS.md`, `FAQ.md`.
