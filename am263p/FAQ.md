# TI LP-AM263P LaunchPad development kit on a Mac: errors, causes and fixes

Checked 2026-10-09 with MCU+ SDK for AM263Px 26.01.00.08, CCStudio 21.0.1 (DSLite 21.0.0.4166), TI Arm Clang 5.1.1.LTS on an Intel Mac (macOS 15.7.9), LP-AM263P Rev A, probe firmware 3.0.0.29. Each answer starts with the fix and names what caused it. 1 kB = 1,024 bytes. Start-here guide: [README](README.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Connecting and loading

### My serial terminal shows nothing (or a stray character or two) after `DSLite load`. Where is the program's output?

Open the console port first, then run the load. The SDK's examples print their messages once, a few milliseconds after the program starts. A terminal opened after the load sees an empty line. The console is the first of the probe's two ports at 115200 8N1: `/dev/cu.usbmodem<serial>1`; the second port (`...4`) stayed silent. With the first port open before the load, `hello_world` printed `Hello World!` and `i2c_led_blink` printed `I2C LED Blink Test Started ...`. The earlier captures of one stray character (`P)`) came from opening the port after the load; what produced that character is **Unknown (suspected: the port was opened as the probe reset the target)**. Checked 2026-10-09.

### DSLite stops with error -615 when it connects. What do I do?

Check the board's power, then check the connection file. Two causes were seen. With only the micro-B cable plugged in, the chip has no power and the probe finds no debug port; the USB-C port needs a 5 V, 3 A supply. And a connection file copied from a Cortex-M33 board carries a property called `SWD Mode`; on this chip it gives -615 even with power. Remove the property. DSLite then connects (the GEL script reports `Board LP, Part Standard`). Checked 2026-10-09.

### `DSLite load` hangs for 90 seconds or longer and times out. Is the board stuck?

Run it again. Loads of the 0.9 MB examples took 4.4 to 7.6 seconds each. The first load of `i2c_led_blink` took 78 seconds, and one earlier try timed out at 90 seconds and then loaded in 5 seconds on the retry. Every load runs the connection file's GEL script, which resets the chip's clocks, so a load always restarts the program. Checked 2026-10-09; the cause of the slow first load was not found.

## Building

### The make ends with `ModuleNotFoundError: No module named 'construct'`. Did the build fail?

The program built; only the boot-image step failed. The SDK's makefile builds a `.mcelf` boot image after the `.out` file, using a Python script that needs the `construct` and `pyelftools` packages. For a RAM load you only need the `.out`. To make boot images, install both in a virtual environment, because `pip` refuses to install system-wide on current macOS. Checked 2026-10-09.

### How do I tell the SDK where CCStudio, SysConfig and the compiler are on a Mac?

Pass them on the make line. `TOOLS_PATH=$HOME/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0 DEVICE=am263px PROFILE=release`. The SDK pins older tool versions (CCStudio 20.5.0, SysConfig 1.27.0, TI Arm Clang 4.0.4.LTS); the newer ones built every example tried. Checked 2026-10-09.

### My own build of CoreMark runs 6 times slower than TI's.

Check that the optimization flag is a capital O. A build script that passed `-o3` instead of `-O3` compiled with no optimization: 223 CoreMark per second instead of 1,464. TI's makefile passes `-O3`. Checked 2026-10-09.

## Before you load SDK examples

### Which SDK examples change the flash?

`ospi_flash_io`, `ospi_flash_xip`, `ospi_flash_diag` and the `sbl_*` examples erase and write the external flash. `ospi_flash_diag` erases and writes a 2 kB test block at flash offset `0x80000` (source lines 107 to 143); loading it destroys what was there. The boot image sits at `0x0` to `0x30000`. Read the example's source for `Flash_erase` and `Flash_write` before loading it, and back up the flash first. Checked 2026-10-09.

### An SDK example failed, but its source says it needs no wires. Is it broken?

On the LP-AM263P, try it again after a full reset first. A load into RAM over the debugger resets the CPU, not the peripherals, so timers, crossbars and pins keep what the example before it set up. In the first pass through TI's examples, 4 of the 9 that printed a failure (`adc_global_force`, `eqep_freq_cal_interrupt`, `eqep_speed_direction`, `fsi_hld_loopback_interrupt`) passed when loaded straight after the debugger's System Reset. `tools/sysreset.js` does that reset from the terminal (`/Applications/ti/ccs2101/ccs/scripting/run.sh tools/sysreset.js`); CCStudio offers the same reset type as "System Reset" (the reset list read over its scripting interface: CPU Reset, System Reset). Checked 2026-10-11.

### TI's CAN FD loopback example prints nothing on the LP-AM263P. Is the board broken?

Probably not: load TI's `mcan_loopback_polling` example first, then the CAN FD one. In MCU+ SDK 26.01, `canfd_loopback_polling`, `canfd_loopback_interrupt` and `canfd_loopback_interrupt_std_exd` (FreeRTOS and no-RTOS builds) printed nothing in 30 s when loaded right after a System Reset. Halted over the probe, `canfd_loopback_polling` was sitting in the C library's `abort()`, before its first console line. Loaded straight after `mcan_loopback_polling`, it printed "Internal loopback testing for 100 iterations Passed" and "All tests have passed". Suspected: a check in its start-up fails unless an MCAN example has set the peripheral up first; which check fails was not found. Checked 2026-10-11.

### DSLite or CCStudio scripting stops with "database disk image is malformed". What now?

Remove the damaged cache slot and connect again. CCStudio 21 keeps a device-database cache per debug server under `~/.ti/ccs2101/<n>/<m>/` (0.6 to 2.4 GB each). When one is damaged, every connect fails with `Error: Could not fetch next record for SQL statement (database disk image is malformed)` and a hint to run `fsclean` from `/Applications/ti/ccs2101/ccs/ccs_base/common/bin/`. Running `lsof +D ~/.ti/ccs2101` finds the slot no process holds. Removing that slot whole worked, and the next load rebuilt it (43 s for that first load, then normal). Setting `TI_APPDATA_DIR` to an empty folder for one command also works. Never remove a slot a running DSLite or CCStudio holds. Seen once, on the Intel Mac, 2026-10-11.

### I load a new program, but the console shows the one I loaded before.

On the LP-AM263P, issue a System Reset from the debugger, then load again. This was seen with DSLite (CCStudio 21) after trying TI's lockstep connection file: it left both R5F clusters' FORCE_WFI register at 0x7, and from then on each load with TI's usual connection file reran the previous program. Two examples loaded in turn each printed the other's banner. A probe reset (`xds110reset`) did not clear it; the debugger's System Reset did (`tools/sysreset.js`). Quick check after any change to how the board is reset or connected: load two programs that print different banners, one after the other. Checked 2026-10-11.

### Why do some of TI's multicore examples not build one core at a time?

Because each is one project for several cores. In MCU+ SDK 26.01 for the LP-AM263P, the examples under `drivers/ipc/`, `empty/` for cores other than R5FSS0-0 (core 0), and a few others are parts of a "system" project. Its SysConfig step defines the shared-memory ranges for all cores. Built alone, a core's linker file has no `USER_SHM_MEM` or `LOG_SHM_MEM`, and TI Arm Clang stops with `warning: memory range not found: LOG_SHM_MEM`, then `error: no valid memory range available for placement of ".bss.log_shared_mem"` and `error: program will not fit into available memory`. Run the system project's `syscfg` step first, then build each core with `SYSTEM_FLAG=true`. Checked 2026-10-11.

## Running code from flash (XIP)

### How much slower is code run from the LP-AM263P's flash than from RAM?

With TI's 64 kB flash cache on, CoreMark and 11 of TI's 12 AI examples ran within 2% of RAM, while TI's memcpy benchmark still took 2.4 times as long; with no extra cache, up to 8.7 times. Flash is slower than RAM by design; how much slower here also depends on the failed tuning, whose cause is unknown. TI's memcpy benchmark took 14.6 ms from on-chip RAM, 8.7 times as long from flash with no extra cache, 2.4 times with the 64 kB cache, and 9% less time with TI's smart placement. CoreMark was 1,464.5 per second from RAM, 1,394.5 from flash and 1,464.5 from flash with the cache. The flash ran at 16.7 MHz because its high-speed read tuning failed (next question); TI's pages, with tuning on, show plain flash at 3.7 times RAM. Measured 2026-10-10, three loads of each; the full tables are on the [execute-in-place experiment page](../docs/experiments/2026-10-10-am263p-xip-ladder.md).

### Do TI's AI examples run as fast from flash?

Eleven of twelve are within 2% once the 64 kB cache is on (ten with their outputs confirmed). Suspected: a model stays slow when the code and data one inference touches are larger than the cache; that was measured only for the slow one. With no extra cache the cost ran from nothing (motor_fault, 0.960 ms all three ways) to 7.8 times (generic_timeseries_forecasting). That model stays 6.9 times slower with the cache: its map file shows 100 kB of code and 66.8 kB of model constants in flash, more than the cache holds, so they are read from flash on every inference. Keep a model like that in RAM. The first inference after a load, with no extra cache, is 0.1 to 1.2 ms slower than later ones while the caches fill. The two smallest models are not free with no extra cache: forecasting_pmsm_rotor_temp takes 16% longer and hvac_indoor_temp_forecast 26% longer. Measured 2026-10-10 on TI's examples from MCU+ SDK 26.01 at 400 MHz.

### The console says `Flash_norOspiPhyTune : PHY enabling failed!!! Continuing without PHY...`. Does the flash still work?

Yes, at a lower clock. Without PHY the flash runs at the input clock divided by the divider, 133.33 MHz / 8 = 16.7 MHz in TI's LaunchPad settings. Reads and writes were correct (zero mismatched bytes on every write checked), but code run from flash was 8.7 times slower than from RAM on TI's memcpy benchmark, where TI's page shows 3.7 times with PHY on. TI's flash debug guide (SPRADS5, section 5.3.3) says the message means the tuning pattern is in flash but does not read back correctly. Programs were loaded over the debugger in development boot; whether a flash boot through TI's bootloader tunes correctly on this board is not tested. Checked 2026-10-10.

### Can opening the flash write to it, even if my program only reads?

Yes. When PHY tuning cannot read its 128-byte pattern, TI's flash driver (`Flash_norOspiPhyTune` in `source/board/flash/ospi/flash_nor_ospi.c`, MCU+ SDK 26.01) erases the 4 kB sector at the flash's PHY tuning offset and writes the pattern there. On the LaunchPad that offset is `0x80000`, just below where TI's flashing guide puts applications (`0x81000`). Back up that sector before running any program that opens the flash with PHY on. Checked 2026-10-10.

### How do I put the flash back after running code from it?

Erase what you wrote, write back the block at `0x80000` from your backup, then read the flash again and compare. The writes sat at 1 MB (up to 166 kB, two 128 kB blocks) and 3 MB (one block); after the restore ran, the lower 16 MB matched the backup byte for byte. That returns the board to the backup, not to TI's factory image: before the first backup, TI's `ospi_flash_diag` example had already rewritten the block at `0x80000`, so what TI shipped there is unknown. `tools/flash_backup.sh` reads only the lower 16 MB on a freshly powered board (the flash wraps at 16 MB in its power-on mode); after the flasher has run it reads all 32 MB, and the upper half read blank. Checked 2026-10-11.

### TI's `fan_blade_anomalydetection` example prints "All tests have passed!!". Did it pass?

No. Read the `Golden vectors matched` line. On the LP-AM263P, TI's unmodified example (MCU+ SDK 26.01, r5fss0-0 nortos) prints `Golden vectors matched: 0 not matched: 192` and `Reconstruction error is 3549, Threshold is 86`, then the pass line, which it prints whatever the counts are. Feature extraction matched TI's reference exactly, so the model's output is what differs. The other eleven AI examples with test vectors matched all their golden values. Checked 2026-10-10.

### How do I run code from flash without changing the boot switches?

Load a small flash writer into RAM, let it write the code, then load the rest of the program into RAM. Link the code and constants at a flash address (0x60100000 and 0x60300000 were used), split the build into a flash part and a RAM part, load the writer (it erases, writes and reads back, then prints `XIPFLASH OK ... mismatched_driver 0 mismatched_mmap 0`), then load the RAM part. The flash stays readable as memory between debugger loads, so one write serves several runs. `bash tools/xip_run.sh flash_xip 1` in `am263p/` does all of it; Back up the flash first with `tools/flash_backup.sh` (it reads the lower 16 MB of the 32 MB part). Checked 2026-10-10, 32 writes, all read back correctly.

## Using all four cores

### DSLite says "Does not match the target endianness, not loaded" when I load the third core of the LP-AM263P. Is my image wrong?

On the LP-AM263P, probably not. After a reset of the board, R5FSS1-0 (core index 2) needed its start-up script run once; R5FSS1-1 is expected to behave the same, not checked. Load that core once with the connection file that runs the GEL script, then later loads work. The image was little-endian like the others (`tiarmreadelf -h`). `tools/mcload.py` does this by default (`MC_GEL=all`: the GEL connection file for every core). In those runs the third core's console lines did not reach the console although it ran, so pass results back through shared memory. Checked 2026-10-11.

### Can each R5F core run a different operating system?

On the LP-AM263P, yes, for the combinations that were run. TI's own multicore examples put FreeRTOS or ThreadX on R5FSS0-0 and no operating system on the other three, and they were run that way. TI's FreeRTOS also ran on R5FSS0-0 beside Zephyr 4.4.2 on R5FSS0-1, sharing memory; Zephyr needs this repo's out-of-tree board files in `zephyr/`. Zephyr on cores 2 and 3 is not run. Loaded straight after a TI program with the no-GEL connection file, Zephyr did not start; loaded after a TI program with the GEL connection file, it did. Results: [four cores and mixed operating systems](../docs/experiments/2026-10-11-am263p-four-cores-mixed-os.md). Checked 2026-10-11.

### Does Zephyr run on the LP-AM263P?

On the LP-AM263P, partly, with this repo's files. Upstream Zephyr 4.4.2 has no AM263P support. The out-of-tree SoC and board (`zephyr/`) boot `samples/hello_world` on R5FSS0-0 and print on the USB serial port, and run code on R5FSS0-1. There is no timer driver for the AM263P's RTI timers, so `k_sleep` and timeouts do not work, and there is no pin-control driver (UART0's clock and pins are set in `soc.c`). You need a Zephyr workspace first (Zephyr's Getting Started guide: `west init`, `west update`, a toolchain). Build: `west build -b lp_am263p/am263p4/r5f0_0 zephyr/samples/hello_world -- -DBOARD_ROOT=<repo>/am263p/zephyr -DSOC_ROOT=<repo>/am263p/zephyr -DDTS_ROOT=<repo>/am263p/zephyr`. Checked 2026-10-11.

### My FreeRTOS timer on core 0 says a long run took microseconds. Why?

On the LP-AM263P, its clock can stall under heavy interrupts. In TI's IPC echo test, `ClockP_getTimeUsec()` on the FreeRTOS core read 664 µs in a first run for an exchange the other cores timed at about 3.9 s (that log was overwritten; in the saved logs the last-started core's own clock reads 3.81 to 3.89 s for the exchange): the echo interrupts kept its timer tick from running. Time such runs with the CPU cycle counter (`CycleCounterP_getCount32()`, 2.5 ns per count at 400 MHz, wraps after 10.7 s). Checked 2026-10-11.

### I loaded core 0 last and the programs on cores 1 to 3 stopped.

On the LP-AM263P, load core 0 (R5FSS0-0, the first core of cluster 0) first. Every connect to R5FSS0-0 with TI's usual connection file runs its chip setup, which clears on-chip RAM (`MSS_L2_Mem_Init` in `AM263Px.gel`). Programs already running on the other cores lose their code and data; their progress markers in shared memory went from 2, 3, 3, 3 to all zeros at that load and never came back. Connects to cores 1 to 3 do not clear RAM. If core 0's program must start after the others, give it a start delay and load the others during the wait (`START_DELAY_S` in the loop's source). Checked 2026-10-11.

### Does a busy core slow a control loop on another core of the LP-AM263P?

Not on average; the worst case can rise. The current loop on core 0 (R5FSS0-0; ADC interrupt, one FOC step, PWM update, 200,000 passes at 50 kHz) read the same to the cycle, mean 493.5 and worst 844 cycles, alone, beside CoreMark on the other three cores, and beside them copying 64 kB blocks in their own on-chip RAM (179 MB/s together with the library `memcpy`, 779 MB/s with this repo's 8-byte loop). With the copies moved into the RAM just below core 0's block (899 MB/s together), the mean and best stayed the same but the worst pass, the first after the load with cold caches, rose from 2.11 µs to 2.51 to 2.70 µs. If your loop's worst case matters, put its hot path in TCM, the core's own tightly coupled memory: with the ADC handler, `foc_step`, TI's interrupt dispatcher, the variables and the stacks in TCM, the worst pass was 1.26 µs (502 to 506 cycles) alone and beside the same copies, and the mean fell 3.2% to 1.194 µs. How: a linker script that places those sections in TCM, and the `-DFRL_TCM` build flag. Flash and DMA neighbours were not run. Results: [lockstep and a loaded neighbour](../docs/experiments/2026-10-11-am263p-lockstep-and-loaded-loop.md). Checked 2026-10-11.

### Why does `memcpy` copy only about 60 MB/s in my LP-AM263P build?

In these builds it is a byte loop; copy 8 bytes at a time yourself if speed matters. In the copy test (CoreMark's four-core source with `-DFRL_MEMSTRESS`, built inside TI's multicore example project with its release makefile; MCU+ SDK 26.01, TI Arm Clang 5.1.1), `memcpy` linked as `__aeabi_memcpy8`, which branches to `TI_memcpy_small`: one `ldrb` and one `strb` per byte. Copying a 64 kB block in on-chip RAM it ran at 58.6 to 61.4 MB/s per core. A plain C loop of 8-byte loads and stores (`frl_copy64` in the same file, with `-DFRL_WORDCOPY`) ran at 235 to 297 MB/s. Which makefile setting makes the linker pick the small version was not isolated. Checked 2026-10-11.

## Lockstep

### What is lockstep for on the LP-AM263P, and do I want it?

Use it when your design needs the chip to catch its own hardware faults; otherwise split mode gives you twice the cores. In lockstep, the second core of each R5F cluster runs the same instructions as the first, and TI's compare logic (the CCM, per TI's documentation) raises an interrupt when they differ. Functional-safety designs use it. You keep two programmable cores instead of four, at the same speed, and TI gives each TCM bank 64 kB instead of 32 kB (TCM-A was checked). TI's bootloader starts in lockstep and switches to split mode only when the application image has a program for the second core. Checked 2026-10-11.

### How do I run the LP-AM263P's R5F cores in lockstep from the debugger?

On the LP-AM263P, reset the chip from the debugger, then skip TI's dual-core switch. The chip comes out of reset in lockstep, TI's usual connection file switches both clusters to dual-core (split mode) on every connect, and TI's lockstep connection file did not switch them back in these tries. Nothing here writes the flash or needs a switch moved. The steps, all in `tools/lockstep.js`:

1. Connect to core 0 with a connection file that has no GEL script (`AM263Px_nogel_<serial>.ccxml`).
2. Issue the debugger's System Reset.
3. Run TI's own GEL steps for unlocking, ROM eclipse, core release, memory init and clocks, but not `Configure_Dual_Core_mode`.
4. Load your program on core 0 and run it.

Run it with `/Applications/ti/ccs2101/ccs/scripting/run.sh tools/lockstep.js` (set `IMAGE` to your `.out`), or `python3 tools/lockstep_run.py <image.out> <seconds> <log>`, which also captures the console. To check the mode, read MSS_CTRL `R5SS0_STATUS_REG` (0x50D0002C) and `R5SS1_STATUS_REG` (0x50D0004C): bit 8 (0x100) set means lockstep. In these runs a debug connect to core 1 or core 3 then failed (DSLite error -1170), as expected for a checker core. To go back to split mode, load anything with TI's usual connection file. Checked 2026-10-11.

### Is lockstep slower?

No, on the LP-AM263P (AM263P4, Cortex-R5F at 400 MHz). TI's CoreMark example ran at 1,464.52 per second in lockstep and 1,464.53 in dual-core mode. The current loop (trigger to new PWM value) took 1.236 µs with the TMU in both, 1.455 against 1.454 µs with TI's software math library, and 2.068 against 2.060 µs with the C library sine (about 3 cycles). Lockstep runs one program on two cores in step so a fault in either shows up as a mismatch; it gives up half the cores, not speed. Results: [lockstep and a loaded neighbour](../docs/experiments/2026-10-11-am263p-lockstep-and-loaded-loop.md). Checked 2026-10-11.

## Motor control

### How long does one current-loop pass take on the LP-AM263P?

On the LP-AM263P, 1.24 µs from the PWM event to the new PWM value with the TMU, 1.45 µs with TI's software mathlib and 2.06 µs with the C library (means of 1,000 passes, 3 runs each). That is ADC conversion, interrupt entry, one FOC step and the compare write, with the code in on-chip RAM at 400 MHz; at 50 kHz it is about 6% of the period. The worst pass, 2.14 µs with the TMU, is the first one after a load, when the caches are cold. Comparison: [current loop on three chips](../docs/experiments/2026-10-11-current-loop-three-mcus.md). Checked 2026-10-11.

### Why does the LP-AM263P take longer than the LP-AM13E230 to reach its ADC interrupt handler?

On the LP-AM263P, 0.63 µs passes from the PWM event to the handler's first line, against 0.37 µs on the LP-AM13E230. The interrupt goes through TI's interrupt crossbar and dispatcher on the R5F, and the time includes the ADC conversion, which was not measured alone. Suspected: the dispatcher and the conversion; in a software-pended test a handler on the R5F's fast interrupt (FIQ) entered in 144 cycles against 195 through the dispatcher (best of 200; worst 205 against 227), not run with the ADC. The control code itself runs about 1.6 times faster on the AM263P (0.61 against 0.97 µs). Checked 2026-10-11.

### Can I compare the LP-AM263P's current-loop numbers directly with the AM13E230's and F2800137's?

As a guide, not to the last tenth of a microsecond. On the LP-AM263P the code ran from on-chip RAM with -Os; on the AM13E230 from flash with -O2; on the F2800137 from flash with TI's C2000 compiler. The AM263P figures are medians of 3 runs, the others single runs. Each chip's ADC converts at its own speed inside the trigger-to-handler time. With the C library, the AM263P's worst pass was a later one, not the first; why was not found. Details: [current loop on three chips](../docs/experiments/2026-10-11-current-loop-three-mcus.md). Checked 2026-10-11.

## PRU display

### Can the LP-AM263P drive an RGB LCD from its PRU?

Yes for the bus: one PRU generated RGB565 at 800x480 and 45.6 frames a second, or 480x480 at 76.3, from a frame buffer in on-chip RAM with no CPU time; no panel has been attached. The AM263P has no display controller, so the PRU does the job in firmware; only PRU1 has pads for all 20 signals (PRU0 has 16). Source: `docs/experiments/2026-10-11-am263p-pru-rgb-scanout.md`.

### Why does the PRU scan-out fail at a 28.6 MHz pixel clock on the AM263P?

Use 9 PRU cycles a pixel (22.2 MHz) or less. The PRU's read widget brings 64 bytes from on-chip RAM in 262 PRU cycles (48.9 MB/s), and at 7 cycles a pixel a 32-pixel block is used every 224. Source: rows `pru_read_bw` in `data/results/am263p.csv`.

### Why does the PRU read widget's status never equal the value I configured?

Read the status into r18 and wait for 0x05. On the AM263P the status reads 0x0B and 0x08 while a block arrives and 0x05 when it is complete, whatever the configuration, and a read before then returns a partly filled copy of the same block without moving on.

### Does the AM263P have a GPMC bus like the AM263x?

No: there is no GPMC base address, pin or driver in the AM263Px SDK 26.01, and its clock code is commented out under `FIXME REMOVE GPMC FOR AM263PX`. The AM263x data sheet lists a 16-bit GPMC; the AM263Px data sheet does not, and no table compares the two. Use the PRU for a parallel display.

## Traps that cost a board its shipped flash

- `ospi_flash_diag` erases and writes the flash block at `0x80000` the moment it runs. Back up the flash before loading any example that opens it, and read the flash driver's open function for erase and write calls as well as the example's own code.
- If the board looks silent, open the serial port before the load: output printed during the load is gone by the time a port opened afterward starts reading.
- `-o3` in lower case is not an optimization flag; TI Arm Clang builds an unoptimized program and CoreMark scores low. Use `-O3` (the CoreMark entry above).
- For the PRU read widget, read its status into the register the TI helper uses and drain it before configuring it, or the first block is dropped. Probe a TI helper block before building on it.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- TI's lockstep connection file leaves both R5F clusters' FORCE_WFI at 0x7; each load then reruns the previous program (`am263p/FAQ.md`, 2026-10-11)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset | remote | failed | `am263p/FAQ.md`, 2026-10-11 |
| Debugger System Reset (tools/sysreset.js), then load again | remote | proven | `am263p/FAQ.md`, 2026-10-11 |

<!-- recovery:end -->
