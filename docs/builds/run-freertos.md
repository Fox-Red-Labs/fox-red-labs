# Run FreeRTOS tasks on a LaunchPad kit

Build FreeRTOS from TI's SDK files, flash it, and watch counted tasks run: on the 4 kB-RAM LP-MSPM0L1306, or with TI's example on the LP-EM-CC2340R5.

The page: [site/project-run-freertos.html](../../site/project-run-freertos.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## FreeRTOS tasks on a 4 kB chip (LP-MSPM0L1306)

No TI example exists for this chip. Build the kernel from TI's SDK files and run 1 to 11 tasks.

Status: Works. Boards: l1306.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0l1306/AGENTS.md`):

```bash
git sparse-checkout add mspm0l1306 mspm0g3507/zephyr
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0L1306 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- An Intel Mac. Everything was built and run on macOS 15.7.9 only.
- CCStudio 21.0.1, which brings TI Arm Clang 5.1.1.LTS, and the MSPM0 SDK 2.11.00.07. Install them where `freertos/build.sh` expects them, under `/Applications/ti/`. The paths are the first lines of the script; change them if yours differ. The FreeRTOS kernel V11.2.0 is inside the SDK and is compiled unedited.
- xPack OpenOCD 0.12.0-7 and the Arm `nm` tool from the Zephyr SDK 0.17.4 compiler, in `~/zephyr-toolchain`. Running `NO_FLASH=1 ./setup-intel-mac.sh` in `mspm0g3507/zephyr` installs both without flashing anything. It also sets up a Zephyr checkout and virtual environment that this project does not use, so it takes a few minutes and some disk.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. Take the board folder alone with the sparse-checkout line in its README, or clone the whole repository.
- The board ships with TI's out-of-box demo. Save it first with `python3 tools/frl/frl.py backup --board mspm0l1306 --probe <serial>`, and put it back afterwards with `python3 tools/frl/frl.py restore "<backup folder>" --probe <serial>`.

### Steps

1. **Mac, terminal.** Find your board's debug probe serial number with TI's `xdsdfu` tool from CCStudio. The scripts talk to one probe only and refuse to run without it.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: A line `Serial Num: <your serial>` for the LaunchPad. Use it as PROBE in steps 4 to 6.

   If nothing is listed, replug the board with a data cable and run it again; close any serial window open on the board.

2. **Bench.** Plug the LP-MSPM0L1306 into the Mac with the micro-USB cable. No wires and no jumpers move for this build.

   You should see: The board powers up and shows its out-of-box demo.

   If a script later says `Probe ... missing`, the serial number in step 1 does not match a connected board.

3. **Mac, terminal.** Build the one-task blink. `build.sh` compiles the five kernel files from the SDK straight into the image and stops if any part falls outside main flash or the 4 kB of SRAM.

   ```text
   cd mspm0l1306/freertos
   ./build.sh blink
   ```

   You should see: `build-blink/sizes.txt` reads `FLASH used 3,784 bytes of 65,528` and `SRAM used 1,008 bytes of 4,096; 3,088 bytes left`. On a lightly loaded Mac each image takes under a minute.

   If the compiler is not found, change the `SDK` and `CC` lines at the top of `build.sh` to where your installers put them.

4. **Mac, terminal.** Flash the blink, paint the chip's RAM with a pattern before any code runs, let it run 5 seconds, read the results, then decode and check them. Replace <serial> with your probe.

   ```text
   PROBE=<serial> ../tools/paintrun.sh build-blink/blink.out build-blink/blink.map fr_blink 6 5000
   python3 check.py build-blink
   ```

   You should see: LED1 (red) is on half a second and off half a second. `check.py` prints `ok    magic 0xB1E00001`, `ok    ticks = 500 x (toggles - 1): the tick runs at 1,000 per second`, and `ALL OK`. The blink task never used 23 of its 48 stack words.

   If the flash step prints `flash failed` or `Probe ... missing`, check the serial from step 1. If OpenOCD says it cannot reset the target or find the MEM-AP, see the L1306 FAQ entry on that error (`mspm0l1306/FAQ.md`).

5. **Mac, terminal.** Now see how many tasks fit. This image makes 11 minimal tasks with 192-byte stacks and all memory static, one checker task and the idle task.

   ```text
   ./build.sh edge-static
   PROBE=<serial> ../tools/paintrun.sh build-edge-static/edge-static.out build-edge-static/edge-static.map fr_edge 12 5000
   python3 check.py build-edge-static
   ```

   You should see: `check.py` prints `made 11 alive 11`, `ok    every minimal task ran in the last second` and `ALL OK`.

   A twelfth task does not link. The linker stops with `program will not fit into available memory` (see `build-edge-static/search.txt`). That is the answer. It is not a fault.

6. **Mac, terminal.** Optional: time what a task costs. This image times a task switch, a semaphore, a notification, a queue round trip and an interrupt waking a task, 1k samples each.

   ```text
   ./build.sh bench
   PROBE=<serial> ../tools/paintrun.sh build-bench/bench.out build-bench/bench.map fr_bench 43 3000
   python3 check.py build-bench
   ```

   You should see: A table of cycle counts per test and `ALL OK`. A task switch round trip is 499 cycles best case at 32 MHz in this run, about 16 microseconds.

   If `check.py` does not print ALL OK, it names the check that failed; `magic 0xBE4C0001` means every test finished.

### Expected result

The red LED blinks from one FreeRTOS task using 1 kB of RAM. With 11 minimal tasks the chip's 4 kB of RAM are almost used: 3.9 kB, with 140 left. Every task runs. A task switch round trip takes 397 to 499 cycles, which is 12 to 16 microseconds at 32 MHz.

### What this shows

- FreeRTOS fits and runs on the MSPM0L1306 with static allocation, using TI's own kernel V11.2.0 and Cortex-M0+ port files unchanged. The one-task blink takes 3.7 kB of flash and 1 kB of RAM.
- 11 minimal tasks fit with static allocation, and a twelfth does not link. With a heap the figure is 10, as predicted: the 11th task creation failed for lack of heap on the board.
- TI's own FreeRTOS settings file for this board needs 5.4 kB of zero-filled RAM plus the stack and does not link on a 4 kB chip.

### Limits

- That a useful application fits. 11 tasks is 11 loops that sleep one tick; a real sensor-and-UART task set was not run.
- Low-power behavior: tickless idle is off in these settings and sleep current was not measured.
- That the timing numbers compare with other RTOS runs one-to-one. The Zephyr thread-yield figure of 431 to 491 cycles used a different timer and build.
- Anything about TI's FreeRTOS example makefiles: they were not used. They link a kernel library from a CCStudio workspace, and the SDK's 48 FreeRTOS example makefiles fail from a terminal as shipped.
- Task stacks of 48 words come from a stack analysis: the worst case is 44 words for a task that sleeps. High-water marks on the board confirmed it. A task that calls deeper code needs more.
- Part of the 256-byte interrupt stack is never given back. The Cortex-M0+ port does not reset the main stack pointer when the scheduler starts, so 100 to 132 bytes stay used.
- The settings file changes TI's: static allocation only, 3 priority levels, 4-character task names, no software timers, no tickless idle.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Kernel | FreeRTOS V11.2.0 from MSPM0 SDK 2.11.00.07 (tasks.c, list.c, queue.c, the TI_ARM_CLANG ARM_CM0 port), compiled unchanged, -O2 | `mspm0l1306/freertos/README.md` |
| Toolchain | TI Arm Clang 5.1.1.LTS from CCStudio 21.0.1; xPack OpenOCD 0.12.0-7; Intel Mac, macOS 15.7.9 | `mspm0l1306/freertos/README.md` |
| Clock | 32 MHz SYSOSC, 1,000 Hz tick; cycle counter is TIMG0 at the CPU clock | `mspm0l1306/freertos/README.md` |
| Blink image | 3.7 kB of flash, 1 kB of RAM (3,088 left) |  |
| Task count, static | 11 tasks plus checker and idle; 3.9 kB of RAM used, 140 left; 12 does not link | `mspm0l1306/freertos/README.md` |
| Task switch round trip | 499 / 640 / 502.9 cycles best / worst / average with asserts and stack check on; 397 / 537 / 400.6 with them off (`bench-lean`); 1k samples | `mspm0l1306/freertos/README.md` |
| Interrupt to task | pend to the first line of the task woken by `vTaskNotifyGiveFromISR`: 583 / 724 / 585.7 cycles; by a semaphore from an ISR: 645 / 789 / 651.2 | `mspm0l1306/freertos/README.md` |
| TI's settings file | `.bss` of 0x15a4 (5.4 kB) on 4 kB of RAM: does not link | `mspm0l1306/freertos/README.md` |

## TI's FreeRTOS example on the LP-EM-CC2340R5

The SDK's own example already runs on FreeRTOS; build it with make and flash with UniFlash.

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

- An Intel Mac, macOS 15.7.9.
- SimpleLink Low Power F3 SDK 9.21.00.36 LTS, a 536 MB macOS installer. CCStudio 21.0.1, for TI Arm Clang 5.1.1 LTS. SysConfig 1.26.3 and UniFlash 9.6.0. Install all of them in `/Applications/ti/`. SysConfig 1.26.3 came with the SimpleLink Wi-Fi SDK installer (10.20.00.39, which you do not need otherwise; you can get SysConfig alone from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed). CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. The SDK example is already copied into `cc2340r5/src/`, and the connection file is `cc2340r5/CC2340R5.ccxml`.
- Your probe's serial number. Get it from `xdsdfu -e`, in CCStudio's `ccs_base/common/uscif/xds110/` folder. Put yours in the connection file.

### Steps

1. **Bench.** Clip the LP-XDS110ET onto the LP-EM-CC2340R5's 20-pin edge, set the TGT VDD jumper to XDS, and plug the probe into the Mac with its USB-C cable.

   You should see: The Mac sees the probe and two serial ports; no driver install was needed.

   If the next flash fails with `The target failed to see a correctly formatted SWD header` (error -615), unplug the USB cable, pull the probe off the board, press it back on firmly and plug in again. That cleared it here. The likely cause is the seating, but that is not proven.

2. **Mac, terminal.** Build TI's example. Change the three paths if your installers put the tools somewhere else. The SDK's own build settings point at folders that do not exist, so pass the paths on the command line.

   ```text
   cd cc2340r5/src/freertos/ticlang
   make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
        SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
        TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
   ```

   You should see: About 22 seconds later, `empty.out` and `empty.hex` are in that folder.

   If the build cannot find a tool, one of the three paths is wrong.

3. **Mac, terminal.** Use the connection file in `cc2340r5/` after putting your probe serial number in it. Do not write one by hand: a hand-written one left SWD off and the errors did not say so. If you need a new one, let CCStudio write it by importing the example, then take it from `targetConfigs/CC2340R5.ccxml` in the imported project.

   ```text
   /Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace <any empty folder> -application com.ti.ccs.apps.importProject -ccs.location <SDK>/examples/rtos/LP_EM_CC2340R5/drivers/empty/freertos/ticlang/empty_LP_EM_CC2340R5_freertos_ticlang.projectspec
   ```

   You should see: Four seconds later the file `targetConfigs/CC2340R5.ccxml` exists inside the imported project. Replace <any empty folder> with a new empty folder and <SDK> with the SDK folder.

   If flashing later says `Unable to access the DAP` or `invalid processor ID`, the file has SWD switched off: use CCStudio's file.

4. **Mac, terminal.** From `cc2340r5/` flash the hex file and run it. Leave `-v` (verify) off: it reports one mismatch at 0x4E020750 in the boot configuration area and ends with a failure although the program was written and runs.

   ```text
   cd ../../..
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u src/freertos/ticlang/empty.hex
   ```

   You should see: About 7 seconds later the flash finishes. `-e` here is `--verbose`: it prints progress and does not erase.

   If two probes are plugged in, the tool does not say which it used: put the serial number of the one you mean in the connection file.

5. **Bench.** Look at the board's red LED.

   You should see: The red LED (DIO14) is on for one second and off for one second.

   If it does not blink, a previous owner's program may still be running; flash again and check that step 4 finished without an error.

### Expected result

The red LED on the LP-EM-CC2340R5 blinks once every two seconds (one second on, one off), running TI's SDK example on its default RTOS, FreeRTOS. The empty FreeRTOS example leaves 23 kB of the chip's 36 kB of RAM free.

### What this shows

- On this chip TI's examples run on FreeRTOS by default; the example's folder is `freertos/` and it builds from a terminal with make.
- The FreeRTOS example uses 11.7 kB of flash and 13 kB of RAM, about 6.2 kB of flash and 3.1 kB of RAM more than the same example with no RTOS.

### Limits

- How many tasks fit, or any task timing: this build only blinks an LED. The task-count and task-switch measurements were made on the LP-MSPM0L1306, and none was made here.
- That it works with the probe-less setup: the board needs the separate XDS110 probe.
- Other systems than an Intel Mac on macOS 15.7.
- Verify (`-v`) reports a mismatch at 0x4E020750 on every image flashed on this board; the program area verified and the program ran each time.
- The first run took about 9 minutes of machine time and 3 hands-on, nearly all of it a connection problem that cleared when the probe was reseated.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Example | SimpleLink Low Power F3 SDK 9.21.00.36 LTS, CCStudio 21.0.1 for TI Arm Clang 5.1.1 LTS, SysConfig 1.26.3 and UniFlash 9.6.0, in `/Applications/ti/`. SysConfig 1.26.3 came with the SimpleLink Wi-Fi SDK installer. CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it. | `cc2340r5/README.md` |
| Memory, FreeRTOS | 11.7 kB flash, 13 kB RAM used, 23 kB RAM free |  |
| Memory, no RTOS | 5.6 kB flash, 9.9 kB RAM used, 26.1 kB RAM free |  |
| Time | build 22 s, flash 7 s; first run about 9 minutes of machine time, 3 hands-on |  |
| Tools | UniFlash 9.6.0, SysConfig 1.26.3, CCStudio 21.0.1; chip CC2340R52 (Cortex-M0+, 48 MHz, 512 KB flash, 36 KB RAM) | `cc2340r5/README.md` |
