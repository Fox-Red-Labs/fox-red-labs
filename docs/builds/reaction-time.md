# Time how fast a microcontroller reacts to an interrupt

Count the CPU cycles from an interrupt to the first line of its handler, on the LP-MSPM0L1306 with no wiring, or on the LP-EM-CC2340R5.

The page: [site/project-reaction-time.html](../../site/project-reaction-time.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Measure it on the LP-MSPM0L1306

Build and run the timing program; read the answer from the chip's RAM. No wires.

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

- An Intel Mac, macOS 15.7.9.
- CCStudio 21.0.1, which brings TI Arm Clang 5.1.1.LTS, and MSPM0 SDK 2.11.00.07. They sit at the paths set on the first lines of `bench/timing/build.sh`; change them there if yours differ.
- xPack OpenOCD 0.12.0-7 and the Arm `nm` tool from the Zephyr SDK 0.17.4 compiler, in `~/zephyr-toolchain`. Running `NO_FLASH=1 ./setup-intel-mac.sh` in `mspm0g3507/zephyr` installs both without flashing anything. It also sets up a Zephyr checkout and virtual environment that this project does not use, so it takes a few minutes and some disk.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. Take the board folder alone with the sparse-checkout line in its README, or clone the whole repository.
- The board ships with TI's out-of-box demo. Save it first with `python3 tools/frl/frl.py backup --board mspm0l1306 --probe <serial>`, and put it back afterwards with `python3 tools/frl/frl.py restore "<backup folder>" --probe <serial>`.

### Steps

1. **Mac, terminal.** Find your board's debug probe serial number with TI's `xdsdfu` tool from CCStudio. The scripts talk to one probe only.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: A line `Serial Num: <your serial>` for the LaunchPad.

   If nothing is listed, replug the board with a data cable and close any serial window open on it.

2. **Bench.** Plug the LP-MSPM0L1306 into the Mac with the micro-USB cable. No wires and no jumpers move.

   You should see: The board powers up and shows its out-of-box demo.

   If a script later says `Probe ... missing`, the serial number in step 1 does not match a connected board.

3. **Mac, terminal.** Build the timing program at -O3. It sets the CPU to 32 MHz, times loops and a GPIO toggle, then pends a spare interrupt, I2C1 on line 25, and reads SysTick on the handler's first line.

   ```text
   cd mspm0l1306/bench/timing
   ./build.sh O3
   ```

   You should see: `build-O3/timing.out` and `timing.map` exist.

   If the compiler is not found, change the `SDK` and `CC` lines at the top of `build.sh`.

4. **Mac, terminal.** Flash it, let it run 1.5 seconds, halt it and read the 11-word result struct from RAM. Replace <serial> with your probe.

   ```text
   PROBE=<serial> ./run.sh O3
   ```

   You should see: `** Verified OK **`, then two lines of RAM words. The first word is 7141e001, which means the program finished. The second word, 01e84800, is the CPU clock: 32,000,000 Hz. On the second line the first word is the best interrupt time in cycles and the second is the worst: `00000019 00000019`, which is 25 and 25. The third is the sample count, `000000c8`, which is 200.

   If `Verified OK` does not appear, the flash failed: check the serial from step 1. If the first word is not 7141e001, the program did not finish; run it again.

5. **Mac, terminal.** Optional: repeat at -O2 and -Os to see that the compiler setting changes the answer.

   ```text
   ./build.sh O2
   PROBE=<serial> ./run.sh O2
   ./build.sh Os
   PROBE=<serial> ./run.sh Os
   ```

   You should see: -O2 reads `0000001c 0000001c`, which is 28 cycles. -Os reads `00000019 00000019`, which is 25 cycles.

   A number that differs by a few cycles is worth rerunning twice. Every saved run gave the same count on all 200 tries.

### Expected result

The chip answers an interrupt in 25 cycles at -O3, the same on all 200 tries: about 0.8 microseconds at 32 MHz. At -O2 it is 28 cycles.

### What this shows

- On an LP-MSPM0L1306 with no operating system, the time from raising an interrupt to the first line of its handler is 25 cycles at -Os and -O3 and 28 at -O2. It was the same on every one of 200 tries, so there is no jitter at this resolution.
- The same program shows what the operating system adds. Through Zephyr the same measurement reads 119 to 137 cycles, and under FreeRTOS 38 to 39. See the Zephyr and FreeRTOS projects.
- A GPIO toggle is one register write at 1.00 cycles at -O2 and -O3. It was not checked with a scope.

### Limits

- Reaction time to an outside signal. The interrupt is raised in software by writing the pending bit, so the pin, input filter and any peripheral that would raise the interrupt are not in the number.
- A like-for-like number across chips. Each count includes one SysTick read, and a read pair costs 2 cycles. The timers, clocks and flash wait states differ per board.
- How long it takes a handler to do useful work: the number stops at the handler's first line.
- Pin speed. The one-cycle toggle is the CPU side only; no scope was on the bench.
- Interrupt line 25 (I2C1) is used because this chip's interrupt controller has only the 13 lines that have a peripheral; the program does not use I2C1 otherwise.
- The count includes one SysTick read of 2 cycles and the store that pends the interrupt.
- -O2 is 3 cycles slower than -Os and -O3 on interrupt entry. Why was not investigated.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Clock | 32 MHz SYSOSC; cycle counter is SysTick on the CPU clock (the Cortex-M0+ has no DWT counter) | `mspm0l1306/bench/timing/main.c` |
| Interrupt entry, pend to handler | 25 / 25 (-Os), 28 / 28 (-O2), 25 / 25 (-O3) cycles best / worst of 200 |  |
| GPIO toggle | 1.12 (-Os), 1.00 (-O2, -O3) cycles for one register write; 1.05 cycles per pass of a driver-call loop at -O2 and -O3 (13.00 at -Os) |  |
| Other chips, same method | LP-MSPM0G3507 at 80 MHz: 29 to 36 cycles; LP-EM-CC2340R5 at 48 MHz: 28 to 29; LAUNCHXL-F2800137 at 120 MHz: 30 (code in RAM) to 32 (from flash) | `data/results/mspm0g3507.csv` |
| Tools | TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, xPack OpenOCD 0.12.0-7, checked 2026-10-06 |  |

## Measure it on the LP-EM-CC2340R5

Build the bare-metal benchmark, flash it with UniFlash, read the answer from the serial port.

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
- SimpleLink Low Power F3 SDK 9.21.00.36 LTS, CCStudio 21.0.1 for TI Arm Clang 5.1.1 LTS, SysConfig 1.26.3 and UniFlash 9.6.0, in `/Applications/ti/`. SysConfig 1.26.3 came with the SimpleLink Wi-Fi SDK installer (10.20.00.39, which you do not need otherwise; you can get SysConfig alone from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed). CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. The program and `run.sh` are in `cc2340r5/baremetal/`.
- Your probe's serial number (`xdsdfu -e`). Put yours in `cc2340r5/CC2340R5.ccxml` and in the `SERIAL_DEV` name, `/dev/cu.usbmodem<serial>1`. Then set the `CCXML` line near the top of `run.sh` to where your copy of the repository is.

### Steps

1. **Bench.** Clip the LP-XDS110ET onto the LP-EM-CC2340R5's 20-pin edge, set the TGT VDD jumper to XDS, and plug the probe into the Mac with its USB-C cable.

   You should see: The Mac sees the probe and two serial ports. The second one is `/dev/cu.usbmodem<serial>1`.

   If a flash later fails with `The target failed to see a correctly formatted SWD header` (error -615), unplug the USB cable, pull the probe off the board, press it back on and plug in again. That cleared it here. The likely cause is the seating.

2. **Mac, terminal.** Build and check without touching the hardware. The script also refuses to flash unless the generated boot configuration keeps the debug port, chip erase, flash program and return to factory all allowed.

   ```text
   cd cc2340r5/baremetal
   ./run.sh O3 --build-only
   ```

   You should see: A memory summary and `CCFG OK: debug port, chip erase, flash program and return to factory all allowed.` The -O3 image is 36.8 kB of flash.

   If the build fails, the last 30 lines of the build log print; a tool path in `run.sh` is the usual cause.

3. **Mac, terminal.** Build, flash with UniFlash and capture the serial report. The script opens the serial port first, then flashes with `dslite.sh --mode flash ... -e -f -u build-O3/bench.hex`, and stops capturing at `BENCH DONE` or after 300 seconds. The run includes CoreMark, so it takes a while.

   ```text
   ./run.sh O3
   ```

   You should see: The report scrolls by, ending in `BENCH DONE`. It is saved to `results/run-O3.txt`. It contains `R,irq_latency_best,29.00,1,...` and `R,irq_latency_worst_of_200,29.00,1,...` and `I,irq_samples,200`.

   If it says the serial port is not present, the board is not plugged in or `SERIAL_DEV` has the wrong serial. Do not add `-v` (verify) to the flash command: it reports a mismatch in the boot configuration area although the program ran.

4. **Mac, terminal.** Optional: run the -Oz build too.

   ```text
   ./run.sh Oz
   ```

   You should see: `R,irq_latency_best,28.00,1,...`, the same 28 for the worst of 200.

   The old program on the board repeats its report every 10 seconds, so make sure the capture holds this build's own `I,optimization` line before you trust a file.

### Expected result

The chip answers an interrupt in 29 cycles at -O3 and 28 at -Oz, about 0.6 microseconds at 48 MHz. It is the same on all 200 tries.

### What this shows

- On an LP-EM-CC2340R5 with no operating system, the time from raising an interrupt to the first line of its handler is 29 cycles at -O3 and 28 at -Oz. It is the same on all 200 tries.
- Through Zephyr on the same chip the same measurement reads 48 to 60 cycles.

### Limits

- Reaction time to an outside signal. The interrupt is pended through the NVIC by software, so no pin or peripheral is in the number.
- A like-for-like number across chips: each count includes one SysTick read, 5 cycles here. The timers, clocks and flash wait states differ per board.
- Run-to-run spread over many boards or sessions: each build was run once on one board.
- The interrupt is the radio block's line, LRFD IRQ0 on line 6. It matches the one the Zephyr run used. The handler is a direct vector installed with `HwiP_plug`, with no wrapper.
- The clock was read back as 48 MHz and a 0.3-second host-timed check gave 48.02 and 48.07 MHz.
- Verify (`-v`) reports a mismatch at 0x4E020750 on every image flashed on this board; the program area verified and the program ran each time.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Interrupt entry, pend to handler | 29 cycles at -O3 and 28 at -Oz, best and worst of 200 identical; includes one 5-cycle SysTick read | `cc2340r5/baremetal/README.md` |
| Through Zephyr | 48 best, 60 worst of 200; includes a 9-cycle timer read quantized to 12 cycles | `data/results/cc2340r5.csv` |
| Under FreeRTOS | 39 cycles, with one run at 202 when an interrupt met the 1 ms tick; includes one counter read | `data/results/cc2340r5.csv` |
| Clock and flash | 48 MHz; flash wait states read live as 2 (1T) and 2 (2T); cache, prefetch and micropredictor on | `cc2340r5/baremetal/README.md` |
| Tools | F3 SDK 9.21.00.36, SysConfig 1.26.3, UniFlash 9.6.0, TI Arm Clang 5.1.1; macOS 15.7.9 | `cc2340r5/baremetal/README.md` |
