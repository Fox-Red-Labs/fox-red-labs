# Measure how fast the chip runs, with CoreMark

Build the standard CoreMark benchmark, run it on the board and work out iterations per second. The same source runs on every board, so the numbers compare.

The page: [site/project-speed-test.html](../../site/project-speed-test.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Bare-metal CoreMark on the LP-MSPM0L1306

Build with TI Arm Clang, run from RAM-checked scripts, read the result through the debugger.

Status: Works. Boards: l1306.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0l1306/AGENTS.md`):

```bash
git sparse-checkout add mspm0l1306 mspm0g3507/zephyr tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0L1306 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with CCStudio 21.0.1 with TI Arm Clang 5.1.1.LTS, the MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7 and the Zephyr SDK's `arm-zephyr-eabi-nm`. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. You also need your probe's serial number (step 2).

### Steps

1. **Mac, terminal.** Install the tools once, all free.
- Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login). It brings TI Arm Clang 5.1.1.LTS.
- Get the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed).
- Get SysConfig 1.26.3 ([TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed).
- Get xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon).
- Get the Zephyr SDK 0.17.4 Arm toolchain, `toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz`, from the Zephyr SDK releases on GitHub. Unpack it into `~/zephyr-toolchain`, as `mspm0g3507/zephyr/setup-intel-mac.sh` does. The run script uses its `arm-zephyr-eabi-nm` to find the result in RAM.
- Clone this repository into `<repository folder>`. CoreMark's source is in `mspm0l1306/bench/coremark`. This repo's scripts point at SysConfig 1.26.3. It came with the SimpleLink Wi-Fi SDK installer on the test Mac and sits in `/Applications/ti/sysconfig_1.26.3`. CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and `ls ~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm` finds the tool.

   If a tool lives elsewhere, change the path at the top of `build.sh`, `run.sh` or `tools/ocd.sh`; the scripts expect each tool at the path set there.

2. **Bench.** Plug the board into the Mac. List your probe's serial number with `xdsdfu -e`, run from CCStudio's `ccs_base/common/uscif/xds110/` folder. Then set it for this shell with `export PROBE=<serial>`. Every script flashes through `mspm0l1306/tools/ocd.sh`, which refuses to run without `PROBE`. Close any serial window open on the board.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: `xdsdfu` lists the probe and its serial number.

   If you want to put the board back as it came, read the out-of-box image before you flash anything; the folder `mspm0l1306/data` has the image read from the test board.

3. **Terminal.** From the repository folder, go to the CoreMark folder and build at -O3 and 32 MHz.

   ```text
   cd mspm0l1306/bench/coremark
   ./build.sh O3 32
   ```

   You should see: A build with no errors that leaves `build-O3-32/coremark.out` and its map.

   If the build cannot find the SDK or compiler, the paths at the top of `build.sh` do not match your install.

4. **Terminal.** Flash it, fill RAM with a pattern, run 16 seconds and read the result from RAM through the debugger. No serial window is needed. The last number is how many seconds to let it run.

   ```text
   ./run.sh O3 32 16
   ```

   You should see: `Correct operation validated`, `Iterations : 1000` and `Total ticks : 408864304`, or very close, and a stack line saying the stack stayed inside its reserved space. The line `Iterations/Sec : 83` is rounded because float printing is off to save RAM.

   If the stack line says OVERFLOW, the score is not valid even if it looks plausible, because this chip has no memory protection. If the number is off by more than a few percent, check `CPUCLK_FREQ` in the generated `ti_msp_dl_config.h`. Check the optimization level too: -Os and -Oz score 47.35 and 41.63. Then run it three times.

5. **Terminal.** Work out the score yourself: iterations divided by ticks at the clock speed.

   You should see: 1k iterations divided by 408,864,304 ticks at 32 MHz is 78.27 per second, or 2.446 per MHz.

   If your tick count differs from run to run, the setup is the cause: every recorded run, by hand and with an AI assistant, gave 408,864,304.

### Expected result

CoreMark prints `Correct operation validated` and its tick count is 408,864,304 or very close, so the score works out to 78.27 per second at 32 MHz. Build and run take well under a minute.

### What this shows

- The LP-MSPM0L1306 scores 78.27 CoreMark per second, 2.446 per MHz, at 32 MHz with TI Arm Clang -O3 and no operating system. The run passes CoreMark's own correctness check.
- Every saved run gave the same tick count, 408,864,304, so a different number points at the setup.

### Limits

- That other chips or compilers score the same. The comparison across boards is on the Benchmarks page. A different compiler or operating system changes the score: GCC 12.2.0 -O3 gave 54.62 on this board, and 59.70 under Zephyr.
- The printed `Iterations/Sec : 83` as the score: it is rounded; the score is computed from the ticks.
- That this is an official EEMBC-certified result: it is this run of the unmodified EEMBC source, with a new port layer.
- CoreMark's 2 kB work area sits on the stack, so the build reserves a 3 kB stack. The deepest use in this run was 2.9 kB, which leaves little room on a 4 kB chip.
- The scripts hard-code Mac paths and the probe serial, and need the Zephyr SDK's `nm`.

### Numbers and sources

| | Value | Source |
|---|---|---|
| CoreMark | 78.27 per second, 2.446 per MHz at 32 MHz, TI Arm Clang -O3; -Os 47.35, -Oz 41.63; at 4 MHz 2.631 per MHz (no flash wait state) |  |
| Ticks and iterations | 1k iterations, 408,864,304 ticks in every saved run |  |
| Memory | 4 kB of RAM, 3.9 kB used with 144 bytes spare; the stack reached 2.9 kB | `mspm0l1306/README.md` |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7 |  |

## Bare-metal CoreMark on the LP-EM-CC2340R5

The SimpleLink route: a clip-on probe, UniFlash, and the report read from the probe's serial port.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debug probe |

### Before you start

- A Mac with the SimpleLink Low Power F3 SDK 9.21.00.36 LTS, CCStudio 21.0.1 with TI Arm Clang 5.1.1, SysConfig 1.26.3 and UniFlash 9.6.0. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. You also need your probe's serial number (step 2).

### Steps

1. **Mac, terminal.** Install the tools once, all free. Get the SimpleLink Low Power F3 SDK 9.21.00.36 LTS from [TI's SDK page](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK) (0.55 GB download, 2.0 GB installed). Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login) for TI Arm Clang 5.1.1 LTS. Get SysConfig 1.26.3 ([TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed). Get UniFlash 9.6.0 from [TI's UniFlash page](https://www.ti.com/tool/download/UNIFLASH) (0.3 GB download, 1.9 GB installed). Clone this repository into `<repository folder>` for `cc2340r5/baremetal`. This repo's scripts point at SysConfig 1.26.3. It came with the SimpleLink Wi-Fi SDK installer on the test Mac and sits in `/Applications/ti/sysconfig_1.26.3`. CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.

   You should see: `/Applications/ti/uniflash_9.6.0/dslite.sh --help` prints its options and `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS` exists.

   If your tools live elsewhere, change the paths at the top of `baremetal/run.sh`.

2. **Bench.** Clip the LP-XDS110ET probe onto the LP-EM-CC2340R5 and plug the probe's USB-C cable into the Mac. Put your probe's serial number in `cc2340r5/CC2340R5.ccxml`; it holds another probe's serial. Set the serial port name in `run.sh` to match: `SERIAL_DEV`, default `/dev/cu.usbmodemLS470GTU1`. Change the `CCXML` line near the top of `run.sh` if your copy of the repository is not in `<repo>`.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: `xdsdfu` lists the probe and its serial number.

   If a flash later fails with `The target failed to see a correctly formatted SWD header` (error -615), unplug the cable, pull the probe off the board, press it back on and plug in again.

3. **Terminal.** From `cc2340r5/baremetal`, build the -O3 variant without touching the hardware. The script also checks that the generated boot configuration, CCFG, keeps the debug port, chip erase, flash program and return-to-factory open. It refuses to flash if not.

   ```text
   ./run.sh O3 --build-only
   ```

   You should see: A build with no errors, the memory configuration printed, and files in `build-O3/` and `results/build-O3.txt`.

   If the script says a CCFG permission is not `CCFG_PERMISSION_ALLOW`, it did not flash on purpose; a wrong setting there can lock the chip.

4. **Terminal.** Build, flash with UniFlash and capture the serial report. A listener opens `/dev/cu.usbmodemLS470GTU1` at 115200 first. Capture stops at `BENCH DONE` or after 300 seconds.

   ```text
   ./run.sh O3
   ```

   You should see: `results/run-O3.txt` holds the report: CoreMark `Correct operation validated` and about 111.09 iterations per second, or 2.31 per MHz at 48 MHz. The board repeats the report every 10 seconds.

   If the serial port looks silent, check the port name in `SERIAL_DEV` and read at 115200. `screen /dev/cu.usbmodemLS470GTU1 115200` shows the report by hand; quit with Ctrl-A then K.

### Expected result

The serial report ends in `BENCH DONE` and shows CoreMark passing its own check at about 111 iterations per second at 48 MHz.

### What this shows

- The LP-EM-CC2340R5 scores 111.09 CoreMark per second, 2.31 per MHz, at 48 MHz with TI Arm Clang -O3 and no operating system.
- Within 1% per MHz of the LP-MSPM0G3507 at full speed, which has the same core and compiler and scores 2.30 per MHz at 80 MHz.

### Limits

- A repeat: each build was run once, so the score is a single measurement.
- That the compiler alone explains the gap to Zephyr with GCC on the same chip, which scored 84.07. That comparison changes the compiler, the operating system and Zephyr's timer setup at once.
- An official EEMBC-certified result.
- The probe is a second board to buy; the LP-EM-CC2340R5 cannot be powered or flashed without one.
- UniFlash's verify (-v) reports a mismatch in the boot configuration area on this chip, so the script flashes without it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| CoreMark | 111.09 per second at -O3 (2.31 per MHz), 57.54 at -Oz; both pass CoreMark's own check (crcfinal 0x4983) | `cc2340r5/baremetal/README.md` |
| Clock | 48 MHz, read back as 48 MHz from the SDK; a 0.3 s host-timed check of the cycle counter gave 48.02 and 48.07 MHz | `cc2340r5/baremetal/README.md` |
| Flash use | 36.8 kB for the -O3 build, 25.6 kB for -Oz; code runs from flash with 2 wait states and the cache on | `cc2340r5/baremetal/README.md` |
| Tools | SimpleLink Low Power F3 SDK 9.21.00.36 LTS, CCStudio 21.0.1 (for TI Arm Clang 5.1.1 LTS), SysConfig 1.26.3 and UniFlash 9.6.0, in `/Applications/ti/`. SysConfig 1.26.3 came with the SimpleLink Wi-Fi SDK installer. CCStudio 21.0.1 carries its own 1.28.1, which this repo's scripts do not point at. | `cc2340r5/baremetal/README.md` |
