# Run Zephyr on a LaunchPad from the terminal

One script installs Zephyr 4.4.2, builds blinky, flashes an LP-MSPM0G3507 and the LED blinks. No IDE, no Homebrew.

The page: [site/project-run-zephyr.html](../../site/project-run-zephyr.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Zephyr blinky on the LP-MSPM0G3507

The upstream board, one script, about 3 minutes on an Intel Mac and about 56 s (build only) on an M4 Mac.

Status: Works. Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac (Intel or Apple silicon) or a Linux machine. Tested on an Intel MacBook on macOS 15.7.9 (flashed), an M4 Mac on macOS 26.3 (build only, 56 s) and Ubuntu 24.04 arm64 in a container (build only, 93 s). Linux x86_64 is pinned but was not run. Logs: `mspm0g3507/zephyr/logs/setup-*-20261018.txt`.
- git, curl and tar (on a Mac: `xcode-select --install`). No Homebrew and no administrator password: if the machine has no Python 3.12 the script installs one into the tools folder with uv, and gets west, CMake and ninja with pip.
- About 2 GB of free disk for Zephyr, its four modules, the compiler and OpenOCD.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. Take the board folder alone with the sparse-checkout line in its README, or clone the whole repository. The script is `mspm0g3507/zephyr/setup-intel-mac.sh`.
- Close any CCStudio debug connection before step 3; it holds the debug probe.

### Steps

1. **Mac, terminal.** Check that git, curl and tar are installed (on a Mac, `xcode-select --install` brings them; 1.8 GB installed). You do not install Python 3.12, ninja, west or CMake: the script fetches them (Python 3.12 through uv) with no administrator password and no Homebrew. It also gets Zephyr 4.4.2 from GitHub, the Arm compiler from Zephyr SDK 0.17.4 and xPack OpenOCD 0.12.0-7 (2 MB), each checked against a SHA-256, about 2 GB in all.

   You should see: `git --version`, `curl --version` and `tar --version` each print a version.

   If a download fails, rerun the script; it keeps what it already fetched. Apple silicon built the same way in a build-only run on an M4 Mac; it was not flashed.

2. **Bench.** Plug the LP-MSPM0G3507 into the Mac with the micro-USB cable. Plug in exactly one board, or you must name the probe in step 3.

   You should see: The board's green power LED comes on. Its debug probe shows up as an XDS110 with USB id 0451:bef3.

   If step 3 later reports no debug probe, list the probes it can see as shown in step 3 and check the USB cable is a data cable.

3. **Mac, terminal.** Run the script from the folder it lives in (replace `<repository folder>` with your clone). It builds a Python environment and gets Zephyr v4.4.2 and four modules: hal_ti, cmsis, cmsis_6 and picolibc. It downloads the compiler and OpenOCD and checks each against a SHA-256 checksum. It builds `samples/basic/blinky` for `lp_mspm0g3507`, checks the board is there, flashes it with `west flash`, then sends one board-level reset. If two boards are plugged in, set `PROBE_SERIAL` to the one you mean.

   ```text
   cd <repository folder>/mspm0g3507/zephyr
   ./setup.sh
   # two boards plugged in? name the probe:
   # PROBE_SERIAL=<your probe serial> ./setup.sh
   ```

   You should see: About 3 minutes later, 3:00 and 3:06 in two clean runs, the script prints three things. First a build memory report, FLASH 11.7 kB of 128 KB, 9.10%. Then OpenOCD flash output ending `wrote 11932 bytes`. The last line is `The blue LED should now blink once a second.`

   If it says `Found 2 debug probes` or `Probe ... is not connected`, it built but did not flash (exit code 3): plug in exactly one board, or set PROBE_SERIAL. List probes with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` (CCStudio) or `system_profiler SPUSBDataType`, where the XDS110 is 0451:bef3.

4. **Bench.** Look at the board's blue LED.

   You should see: The blue LED blinks once a second.

   If the LED stays dark, the chip is probably sitting in Zephyr's NMI handler after the debugger's soft reset. Press the board's reset button; the script's last command does the same thing with OpenOCD's `mspm0_board_reset`.

### Expected result

The blue LED on the LP-MSPM0G3507 blinks once a second, running Zephyr 4.4.2's unmodified blinky sample. It takes about 3 minutes from the start of the script to the blink on a Mac that already had python3.12, git and ninja. The first run, while the workarounds were still being found, took about 9.5 minutes.

### What this shows

- Zephyr 4.4.2's upstream blinky for `lp_mspm0g3507` builds and runs from the command line on an Intel Mac. It uses pip and prebuilt downloads instead of Homebrew and a source build of OpenOCD.
- The image is small: 11.7 kB of flash, 9.10% of 128 kB, and 3.9 kB of RAM.
- The install is repeatable: two clean runs from the script took 3 minutes 00 seconds and 3 minutes 06 seconds.

### Limits

- That this is Zephyr's officially supported path. Zephyr 4.4.2 asks for Zephyr SDK 1.0.1, which has no Intel macOS build, so the script uses the Arm compiler from SDK 0.17.4 through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`. It works here, but it is not an officially supported pairing.
- That it works on Apple Silicon, Windows or Linux, or with other Zephyr versions: only v4.4.2 was flashed and run, on macOS 15.7.9; the M4 Mac and the Linux container only built it.
- Anything beyond blinking an LED: no serial console, sensor, radio or network sample was run in this build.
- After a debugger soft reset the chip sat in Zephyr's NMI handler and did not blink; the record does not say why. The script ends with a board-level reset that clears it, and the reset button does the same.
- The script only checks python3.12, git and ninja by using them; if one is missing it stops with that tool's own error.
- Two checks were made before flashing a board. Neither is in Zephyr's guide. The December 2025 fix for the "bricked LaunchPad" bug, Zephyr PR 102668, was confirmed present in the source, and the image was confirmed to contain main flash addresses only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Zephyr | v4.4.2, shallow clone; modules hal_ti, cmsis, cmsis_6, picolibc only | `mspm0g3507/zephyr/setup-intel-mac.sh` |
| Compiler | arm-zephyr-eabi from Zephyr SDK 0.17.4 (GCC 12.2.0) via ZEPHYR_TOOLCHAIN_VARIANT=cross-compile | `mspm0g3507/zephyr/setup-intel-mac.sh` |
| Flashing tool | xPack OpenOCD 0.12.0-7, darwin-x64 build, run through `west flash` | `mspm0g3507/zephyr/setup-intel-mac.sh` |
| Host | Intel Mac, macOS 15.7.9; west and CMake from pip in a Python 3.12 virtual environment | `mspm0g3507/zephyr/README.md` |
| Image | 11.7 kB of flash (9.10% of 128 kB), 3.9 kB of RAM |  |
| Flash | 11.7 kB written in 2.87 s |  |
| Time | 3 min 00 s and 3 min 06 s from clean (two runs); about 9.5 min for the first run; 0 minutes hands-on in the first run | `mspm0g3507/zephyr/README.md` |
