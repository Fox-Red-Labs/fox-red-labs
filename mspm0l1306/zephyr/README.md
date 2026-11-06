# Zephyr on the TI LP-MSPM0L1306 LaunchPad: board file and what fits in 4 kB of RAM

Zephyr 4.4.2 has the MSPM0L1306 chip but no board for this LaunchPad kit, so this folder adds one out of tree. hello_world and blinky link with about 200 to 260 bytes of RAM to spare. The project's benchmark needs 12,824 bytes (12.5 kB) of RAM as written, and it fits in 4,096 only after four changes: smaller stacks, the test buffers moved onto the stack, a 512-byte copy test instead of 1,024, and CoreMark's data cut from 2,000 to 1,200 bytes (1.2 kB). That last change means the CoreMark score cannot be compared with standard scores. A second build runs CoreMark alone at the standard 2,000 bytes (2.0 kB) and links with 16 bytes of RAM to spare. On the board it scores 59.70 per second (1.87 per MHz).

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-06 on an Intel Mac (macOS 15), Zephyr 4.4.2, GCC 12.2.0 from Zephyr SDK 0.17.4 (`ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`), MSPM0 SDK 2.11.00.07 for pin data. Everything here was built, then flashed and run on an LP-MSPM0L1306 (probe <probe serial>) the same day.

## Short version

- **On the board (2026-10-06):** hello_world, blinky and every benchmark image ran. Standard CoreMark under Zephyr: **59.70 per second, 1.87 per MHz** (GCC at -O2 plus loop unrolling; the build named O3), against 2.446 per MHz with TI Arm Clang and no operating system. Interrupt latency through Zephyr: 119 to 137 cycles. Two fixes were needed first: `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0`, after every benchmark image stopped at `***** HARD FAULT *****` during boot (**Third-party behavior**, point 8), and interrupt line 25 instead of 28 for the latency test (**Chip limit**, point 9).

- **Board:** `boards/ti/lp_mspm0l1306/`. Use it with `-DBOARD_ROOT=<this folder>`. Console on UART0 (PA8 TX, PA9 RX) at 115200. Four LEDs and two buttons in the devicetree.
- **Two upstream gaps, both worked around in the board folder.** Zephyr's GPIO driver does not compile for this chip, and there is no pin-mux file for the MSPM0L130x family. Details under "Failures".
- **4 kB is tight.** Zephyr's default interrupt stack is 2,048 bytes (2.0 kB), half the chip. hello_world already uses 93.6 percent of RAM.
- **Benchmark:** `mcu-bench-l1306/` is the project's CoreMark and timing benchmark, copied and fitted to this chip.
- **Build anything:** `./build.sh <target>`. Set `OUT` to the folder for build output.

Contents: Results on the board. Sizes. Hex files. What each one prints. The files. Commands. Failures and how they were passed. Other traps. Zephyr's clock setup on this chip.

## Results on the board

Run 2026-10-06 on probe <probe serial>, flashed with OpenOCD, read from the serial port (`../tools/flash_and_capture.py`). Every run passed CoreMark's own check ("Correct operation validated") and every stack line printed a value above 0, so no stack ran out.

CoreMark, standard 2,000-byte data, GCC 12.2.0, at 32 MHz:

| Build | Iterations | Seconds | CoreMark/s | Per MHz | Main stack left | MSPM0G3507, same build, per MHz |
|---|--:|--:|--:|--:|--:|--:|
| `coremark2k-Os` | 3,000 | 71.45 | 41.99 | 1.31 | 176 B | 1.19 |
| `coremark2k-O2` | 3,000 | 55.83 | 53.73 | 1.68 | 128 B | 1.63 |
| `coremark2k-O3` (really -O2 with loop unrolling, point 7) | 3,000 | 50.25 | **59.70** | **1.87** | 128 B | 1.75 |
| `coremark2k-trueO3` (-O3 -funroll-loops applied) | 3,000 | 52.87 | 56.74 | 1.77 | 104 B | 1.71 |

Seconds come from Zephyr's uptime counter, which runs from the 32 MHz system clock; the measured clock is 32.16 MHz, so the true rates are 0.5% higher. Per MHz assumes 32 MHz exactly. The L1306 needs fewer cycles per iteration than the G3507 because its flash adds one wait state at 32 MHz and the G3507's adds two at 80 MHz. Real -O3 is slower than -O2 with unrolling on this chip, and on the G3507 (136.70 against 139.88) and the CC2340R5 (79.54 against 84.07) too, measured the same day: suspected, the bigger code costs more in flash waits than it saves. TI Arm Clang on the same chip, no operating system, scores 2.446 per MHz.

The fitted benchmark (`bench-Os`, `-O2`, `-O3`; CoreMark at 1,200 bytes (1.2 kB), so its CoreMark is not comparable), CPU cycles, best of five:

| Test | -Os | -O2 | -O2 plus unrolling (O3 build) | true -O3 | What it is |
|---|--:|--:|--:|--:|---|
| timer_read | 102.09 | 81.07 | 78.32 | 77.32 | one `k_cycle_get_32()`; every timed row includes about one |
| empty_loop | 5.00 | 5.00 | 1.63 | 1.63 | loop overhead |
| udiv32_software | 253.25 | 246.24 | 242.24 | 242.24 | the core has no divide instruction |
| float_mul_add_software | 253.04 | 251.03 | 248.79 | 248.54 | no FPU |
| sinf_cosf_software | 9,066.76 | 8,712.27 | 8,708.61 | 8,719.33 | |
| memcpy_512_bytes | 5,157.45 | 873.38 | 869.50 | 897.49 | |
| gpio_toggle_zephyr_api | 42.04 | 32.04 | 26.04 | 27.79 | `gpio_pin_toggle_dt` |
| gpio_toggle_register | 6.04 | 10.03 | 2.04 | 2.04 | DriverLib toggle of PA0 |
| irq_latency, best and worst of 200 | 137 / 137 | 121 / 121 | 119 / 122 | 120 / 121 | pend to the handler's first line through Zephyr's wrapper, including one timer read |
| thread_yield_round_trip | 491.08 | 434.07 | 430.69 | 437.69 | two context switches |
| nn_mlp_64_32_16_4 | 42,333 | 48,469 | 24,981 | 25,215 | one inference, 2,624 multiply-adds |
| nn_cnn_1d_128 | 466,202 | 400,313 | 243,130 | 198,005 | one inference, 20,608 multiply-adds |

The true -O3 column is a fifth build (`bench-trueO3`); its CoreMark at 1,200 bytes (1.2 kB) was 209.75 against 218.80 for the unrolled -O2 build, the same direction as the standard CoreMark above. The neural-net checksums match the G3507's and the CC2340R5's runs (`-22287500` and `-7929100`), so the same code gives the same answers on all three chips.

## Sizes

Flash total is 65,536 B (64.0 kB). RAM total is 4,096 B (4.0 kB). "Links" means the linker accepted it; it does not mean it ran (see "Results on the board" for what ran). Benchmark sizes are after the heap fix in Failures, point 8.

| Build | What it is | Flash used | RAM used | Links? | RAM left |
|---|---|--:|--:|---|--:|
| hello_world | `samples/hello_world` | 9,424 | 3,832 | yes | 264 |
| blinky | `samples/basic/blinky`, LED1 red on PA0 | 11,364 | 3,888 | yes | 208 |
| blinky-rgb-red | same, red channel of the RGB LED on PA26 | 11,372 | 3,888 | yes | 208 |
| blinky-upstream-gpio | blinky with Zephyr's own GPIO driver forced on | | | no: compile error | |
| bench-orig-Os, -O2, -O3 | the G3507 benchmark, unchanged | | | no: compile errors | |
| bench-port-Os, -O2, -O3 | copy with the chip differences fixed, G3507 memory settings | | needs 12,824 | no: overflow by 8,728 | |
| bench-Os | copy fitted to 4 kB, CoreMark at 1,200 bytes (1.2 kB) | 41,940 | 4,064 | yes | 32 |
| bench-O2 | same, speed build | 47,692 | 4,064 | yes | 32 |
| bench-O3 | same, "-O3" (really -O2 plus unrolling, see Failures, point 7) | 56,192 | 4,064 | yes | 32 |
| bench-trueO3 | same, app really at -O3 -funroll-loops | 58,332 | 4,064 | yes | 32 |
| coremark2k-Os | CoreMark alone at the standard 2,000 bytes (2.0 kB) | 25,480 | 4,080 | yes | 16 |
| coremark2k-O2 | same, speed build | 30,400 | 4,080 | yes | 16 |
| coremark2k-O3 | same, "-O3" | 35,760 | 4,080 | yes | 16 |
| coremark2k-trueO3 | same, app really at -O3 -funroll-loops | 37,100 | 4,080 | yes | 16 |
| synchronization | `samples/synchronization` | | needs 6,296 | no: overflow by 2,200 | |
| synchronization, trimmed | same with interrupt and main stacks at 512 | | needs 4,248 | no: overflow by 152 | |
| philosophers | `samples/philosophers` | | needs 17,400 | no: overflow by 13,304 | |
| shell_module | `samples/subsys/shell/shell_module` | | needs 12,776 | no: overflow by 8,680 | |
| logger | `samples/subsys/logging/logger` | | needs 8,192 | no: overflow by 4,096 | |

"Needs" is 4,096 plus the overflow the linker printed.

Where the RAM goes in the samples that do not fit (from their map files):

| Sample | Biggest pieces |
|---|---|
| synchronization | interrupt stack 2,048; two sample threads 1,024 each, fixed in the sample's source; main stack 1,024 |
| philosophers | six philosopher stacks of 2,048 (12,288); interrupt stack 2,048; main stack 1,024 |
| shell_module | shell thread stack 2,048; interrupt stack 2,048; work queue stack 1,024; main stack 1,024; log buffer 1,024; log thread stack 768 |
| logger | interrupt stack 2,048; log buffer 2,048; sample thread 1,024; main stack 1,024; log thread stack 768 |

 These are Zephyr's sample defaults; trimmed further (stacks hard-coded in the samples changed by a build-time copy, minimal shell, smaller log buffer), synchronization, logger, shell_module and all eight Thread-Metric tests ran on the board: `extra/README.md`.

## Hex files

All in `<OUT>/<target>/zephyr/zephyr.hex`. Every image covers main flash only (highest address 0x0000DC70, in bench-O3). Nothing writes NONMAIN, the boot configuration area.

## What each one prints on the serial port (115200, 8N1)

All of these were seen on the board, 2026-10-06.

- **hello_world:** `*** Booting Zephyr OS build v4.4.2 ***`, then `Hello World! lp_mspm0l1306/mspm0l1306`.
- **blinky and blinky-rgb-red:** the boot banner, then `LED state: OFF`, `LED state: ON`, alternating once a second. LED1 needs jumper J2 on and J10 on 2:3 (3.3 V pull-up). The RGB red needs J12.
- **bench-Os, -O2, -O3:** `BENCH START`, `I,` lines (board `lp_mspm0l1306`, timer 32000000 Hz, `I,coremark_total_data_size,1200,not comparable with standard 2000-byte CoreMark scores`, `I,mathacl,not on this chip`), one `R,` line per test, `I,peer_stack_unused`, `COREMARK START`, CoreMark's report with `Profile generation run parameters for coremark.` and, if it passes its own check, `Correct operation validated.`, then the stack lines (`main_stack_unused_after_coremark`, `isr_stack_unused`, `idle_stack_unused`) and `BENCH DONE`. The copy test row is now `memcpy_512_bytes`.
- **coremark2k-Os, -O2, -O3:** `BENCH START`, the `I,` lines with `I,coremark_only,1`, `COREMARK START`, `2K performance run parameters for coremark.`, the report, the stack lines, `BENCH DONE`. CoreMark needs at least 10 seconds of run time to count; 3k iterations at 32 MHz run 50 to 71 seconds.

The stack lines are the check that matters on this chip. It has no memory protection unit, so a stack overflow does not stop the program. It overwrites the next stack down. If any `_unused` value prints as 0, the run is not valid.

## The files

| Path | What it is |
|---|---|
| `boards/ti/lp_mspm0l1306/board.yml`, `Kconfig.lp_mspm0l1306`, `board.cmake`, `lp_mspm0l1306_defconfig`, `lp_mspm0l1306.yaml`, `support/openocd.cfg` | Board files, same shape as Zephyr's `lp_mspm0l2228`. OpenOCD config is the l2228 one: XDS110 probe, `target/ti_mspm0.cfg`. |
| `boards/ti/lp_mspm0l1306/lp_mspm0l1306.dts` | Devicetree: UART0 console, LEDs `led0` (LED1, PA0, active low, open drain), `led1` red PA26, `led2` green PA13, `led3` blue PA27 (active high), buttons `sw0` S1 on PA18 (active high, external pull-down) and `sw1` S2 on PA14 (active low, internal pull-up). |
| `boards/ti/lp_mspm0l1306/gpio_mspm0_l1306.c`, `Kconfig`, `Kconfig.defconfig`, `CMakeLists.txt` | Board-local copy of Zephyr's GPIO driver with a 28-pin table, and the switch that builds it instead of the upstream one. |
| `overlays/blinky-rgb-red.overlay` | Points `led0` at the RGB red LED. |
| `build.sh` | Builds every target in this README. |
| `mcu-bench-l1306/` | The benchmark copy. Changed: `src/main.c`, `CMakeLists.txt`, `prj.conf`. New: `prj-g3507.conf` (the original `prj.conf`), `coremark2k.conf`. Unchanged: `speed.conf`, `src/coremark/`, `src/nn_weights.h`. |
| `tools/stackdepth.py` | Worst-case stack depth from GCC call-graph files. |
| `tools/ramsum.py` | Where the RAM went, from a linker map file. Works on failed links. |

Pin sources: `mspm0l130x.h` in MSPM0 SDK 2.11 (UART0 TX is PINCM9 function 2, RX is PINCM10 function 2, and PA*n* is PINCM *n*+1 for all 28 pins), and the LP_MSPM0L1306 example READMEs in the same SDK (LED jumpers, S1 on PA18 with external pull-down and J11, S2 on PA14 to ground with no pull resistor). The SDK's own board file `LP_MSPM0L1306.syscfg.json` lists the header pins but not the buttons.

## Commands

Build one target, or all of them. The script sets up the environment from `~/zephyrproject/.venv` and `~/zephyr-toolchain`.

```bash
cd mspm0l1306/zephyr
./build.sh blinky
```

```bash
./build.sh all
```

Targets: `hello_world`, `blinky`, `blinky-rgb-red`, `bench-Os`, `bench-O2`, `bench-O3`, `coremark2k-Os`, `coremark2k-O2`, `coremark2k-O3`, `bench-port-Os` (and O2, O3), `bench-orig-Os` (and O2, O3), `synchronization`, `philosophers`, `shell_module`, `logger`, or `custom <name> <app folder> [cmake options]`.

What the script runs, for one sample:

```bash
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd ~/zephyrproject/zephyr
west build -p always -b lp_mspm0l1306 samples/basic/blinky -d <OUT>/blinky -- -DBOARD_ROOT=<path to this folder>
```

The runs above flashed with `../tools/flash_and_capture.py`; `west flash` was not run on this board. The pattern that worked on the G3507, with this board's probe serial number, from `~/zephyrproject/zephyr` with the environment above:

```bash
west flash -d <OUT>/blinky --openocd ~/zephyr-toolchain/xpack-openocd-0.12.0-7/bin/openocd --openocd-search ~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts --cmd-pre-init "adapter serial ML130001"
```

```bash
../tools/ocd.sh "mspm0_board_reset"
```

The second command is the board-level reset. On the G3507 a debugger soft reset left the chip in Zephyr's NMI handler (`../../mspm0g3507/zephyr/README.md`).

## Failures and how they were passed

1. **blinky did not compile.** `drivers/gpio/gpio_mspm0.c:57:9: error: 'IOMUX_PINCM38' undeclared here (not in a function); did you mean 'IOMUX_PINCM28'?` and the same for PINCM39 to PINCM74. Zephyr 4.4.2's GPIO driver has one pin table for the whole MSPM0L series, and it is the 80-pin MSPM0L222x layout. The device headers for MSPM0L110x, L130x and L134x stop at PINCM28. Of Zephyr's four MSPM0 boards, the one with 4 kB of RAM (`pocketbeagle_2/mspm0l1105`) lists pinctrl as supported, not GPIO. Fix: the board folder builds its own copy of the driver with the right table (PA*n* is PINCM *n*+1), and its `Kconfig.defconfig` turns the upstream driver off. If you set `CONFIG_GPIO_MSPM0=y` yourself, the error comes back: `./build.sh custom blinky-upstream-gpio samples/basic/blinky -DCONFIG_GPIO_MSPM0=y -DCONFIG_BOARD_LP_MSPM0L1306_GPIO=n` reproduces it. CMake then prints `No SOURCES given to Zephyr library: drivers__gpio`. That warning is expected and harmless.
2. **No pin-mux file for this chip.** Zephyr's TI module ships `mspm0l110x-pinctrl.dtsi` and `mspm0l222x-pinctrl.dtsi`, nothing for L130x. Fix: the two UART pin nodes are written in the board's devicetree from the SDK header.
3. **The benchmark did not compile.** `error: unknown type name 'DL_MathACL_operationConfig'`, `error: 'MATHACL' undeclared`, `error: 'GPIOB' undeclared (first use in this function); did you mean 'GPIOA'?` The benchmark was written for the G3507, which has a math accelerator and a second GPIO port. The L1306 has neither (`mspm0l130x.h` has no `MATHACL_BASE` and no `GPIOB_BASE`). Fix in the copy: accelerator tests only on the MSPM0G series, register toggle on PA0, board name from the chip.
4. **Then it did not link.** `section 'noinit' will not fit in region 'RAM'` and `region 'RAM' overflowed by 8728 bytes`, the same at -Os, -O2 and -O3. The G3507 settings ask for 12,824 bytes (12.5 kB): main stack 6,144, interrupt stack 2,048, second thread 512, idle 256, copy buffers 2,048, neural-net buffers 1,080, and 736 bytes of everything else.
5. **Fitting it.** Changes in the copy, each needed:
   - Main stack 6,144 to 2,304. Interrupt stack 2,048 to 512. Second thread 512 to 256. Idle stays 256.
   - Copy and neural-net buffers moved from fixed RAM onto the stack, inside two functions that return before CoreMark starts. The same bytes serve CoreMark later.
   - The copy test is 512 bytes, not 1,024. Two 1,024-byte buffers on the stack would make that test the deepest point, deeper than CoreMark.
   - CoreMark data 2,000 to 1,200 bytes (1.2 kB). This is CoreMark's own "profile run" size: the seeds change to 8, 8, 8 and CoreMark still checks its results against known answers. The score is not comparable with any 2,000-byte score. Why it had to go: in the fitted build everything except the main stack takes 1,776 bytes (1.7 kB), which leaves 2,320 for the main stack. CoreMark at 2,000 bytes (2.0 kB) needs at least 2,640 there (next point).
   - Logging was already off. The accelerator code is gone (point 3).
   - The bench now prints how much of every stack was used.
6. **How the stack sizes were chosen.** From GCC's call graph (`-fcallgraph-info=su`, then `tools/stackdepth.py`). Worst direct-call path from `main`: 1,896 bytes (1.9 kB) at -Os and 1,944 at -O2 and -O3 for the fitted bench; 2,640 and 2,680 for CoreMark alone at 2,000 bytes (2.0 kB). The analysis cannot follow calls through function pointers (printf's path to the UART) or count interrupt frames. For scale: the same analysis on the copy with G3507 settings gives 2,784 bytes (2.7 kB) at -Os, and the G3507 build of the same benchmark measured 2,880 bytes (2.8 kB) on hardware. The fitted bench has a 2,304-byte main stack. The CoreMark-alone build has 2,912 and a 320-byte interrupt stack; boot needs 132 to 144 bytes there and the timer interrupt 104 to 120. Measured on the board afterwards: every stack line printed above 0 (smallest margin 104 bytes, the main stack of `coremark2k-trueO3`), so the sizes held.
7. **"-O3" is not -O3.** The benchmark's CMakeLists adds `-O3 -funroll-loops` to the app, but Zephyr's own `-O2` comes later on the compile line, and GCC uses the last `-O`. Seen in the build's `compile_commands.json`, here and on the G3507: `-O3 ... -O2`. So every "-O3" result in this project, on the G3507 too, is -O2 with loop unrolling. The copy keeps the same behavior so its numbers line up with the G3507's. To get a real -O3 in the copy, add `-DBENCH_TRUE_O3=1` to `-DBENCH_O3=1` (`./build.sh coremark2k-trueO3`): it appends `-O3 -funroll-loops` as a source-file property, so the app and CoreMark lines end `-O3 -funroll-loops -O2 -O3 -funroll-loops` and the kernel stays at `-O2`. It links in 4 kB; its static worst stack path is 2,704 bytes (2.6 kB) of the 2,912-byte main stack. `CONFIG_COMPILER_OPT="-O3 -funroll-loops"` does not work: it never reaches the app lines (they still end `-O2`) and moves the kernel and C library to -O3 instead.
8. **Every benchmark image hard-faulted at boot.** The serial port showed `***** HARD FAULT *****` and `Faulting instruction address (r15/pc): 0x00004f10`, which is `sys_heap_init`, called from `malloc_prepare`. Zephyr's C library heap defaults to "all RAM left after the program" (`CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=-1`). These images leave 8 to 16 bytes, and setting up a heap that small writes its bookkeeping past the end of RAM. No warning at build time. Fix: `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0` in `prj.conf` (nothing here calls `malloc`).
9. **Interrupt latency read 17.7 million cycles**. The test pended interrupt line 28, which the G3507 uses for AES. On the L1306 the interrupt controller implements only lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25 and 31 (writing all ones to the enable register reads back `0x8315a217`), so pending line 28 does nothing, and `IRQ_CONNECT` accepts it without a word. Fix: line 25 (I2C1, unused here).

## Other traps for a beginner or an AI agent

- **S1 is also the bootloader pin.** PA18. Holding S1 through a reset starts TI's ROM bootloader instead of your program.
- **LED1 is open drain.** PA0 can only pull low. It needs the J10 pull-up jumper, and J2 to reach the LED. The devicetree marks it `GPIO_ACTIVE_LOW | GPIO_OPEN_DRAIN`.
- **`led0` is LED1.** Zephyr counts from 0, the board's silk screen from 1.
- **Nodes for hardware the chip does not have.** The shared MSPM0 devicetree files define `gpiob`, `gpioc` and `rtc`. The L1306 header has no `GPIOB_BASE` and no RTC. They are disabled; do not enable them.
- **No `clock-frequency` on `cpu0`** in the L-series devicetree. The benchmark reads it. The board sets 32 MHz there; that is a description, not a register setting.
- **The upstream l2228 board puts the app at 0x8000** (MCUboot slot partitions sized for 256 kB). Not copied here; this board links at 0.
- **Every build compiles the C library from source:** 1,099 steps, about 2 minutes per build on this Mac when nothing else runs.
- **`dtc` is not installed** (`Could NOT find Dtc`). Zephyr builds without it, but the extra devicetree checks dtc does were not run.
- **No Kconfig or devicetree warnings** in any build.

## Zephyr's clock setup on this chip

Read from the source, not measured here:

- Devicetree (`dts/arm/ti/mspm0/mspm0.dtsi`; the L-series and L1306 files and this board change nothing): SYSOSC 32 MHz, MCLK from SYSOSC at 32 MHz, ULPCLK 32 MHz with no divider, LFCLK from the internal 32,768 Hz oscillator. HFXT, LFXT, HFCLK, MFPCLK and CANCLK are disabled. There is no PLL node on the L series.
- `drivers/clock_control/clock_control_mspm0.c` at boot calls `DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE)` and nothing else for this devicetree. MCLK stays on SYSOSC, where reset leaves it.
- `soc/ti/mspm0/common/soc.c` sets the brown-out level to 0.
- Kconfig takes the system clock, 32,000,000 Hz, from the MCLK node. The system timer is the Cortex-M SysTick, 10,000 ticks a second.
- TI's own `empty_non_sysconfig` example for this board does the same three things: SYSOSC base frequency, MCLK divider off, brown-out level 0.
- Neither sets flash wait states. The data sheet says the hardware manages them.
