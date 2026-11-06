# How fast is the TI LP-EM-CC2340R5 (CC2340R52) on bare metal with TI Arm Clang?

Terms: CoreMark, SysTick, NVIC, VIMS and CCFG are defined below and in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

EEMBC CoreMark scores 111.09 at -O3 (2.31 per MHz at 48 MHz) and 57.54 at -Oz, with TI Arm Clang 5.1.1 and no operating system, measured on the board on 2026-10-05. Interrupt latency from pend to handler is 28 to 29 CPU cycles (about 0.6 microseconds at 48 MHz), and a GPIO register toggle takes 2.1 cycles per loop pass at -O3.

Built and flashed from an Intel Mac (macOS 15.7.9) with SimpleLink Low Power F3 SDK 9.21.00.36, SysConfig 1.26.3 and UniFlash 9.6.0. The board carries the CC2340R52 chip, 36 kB of RAM; the CC2340R53 is a 64 kB version on a separate board, LP-EM-CC2340R53. 1 kB = 1,024 bytes.

Contents: [results](#results-measured-2026-10-05), [run it](#run-it), [recipe](#recipe), [what it measures](#what-it-measures), [memory](#memory-from-the-linker-map-memory-configuration), [what to do next](#what-to-do-next), [files](#files).

Words used on this page (more in [docs/vocab.md](../../docs/vocab.md)):

- **CoreMark**: EEMBC's standard benchmark for small processors. Higher is faster; "per MHz" divides out the clock so chips at different speeds compare.
- **SysTick**: the Arm core's built-in 24-bit timer. Here it counts every CPU cycle.
- **VIMS**: the CC23xx flash cache and prefetch unit, between the CPU and flash.
- **NVIC**: the Arm core's interrupt controller.

## Results, measured 2026-10-05

| | -O3 | -Oz |
|---|--:|--:|
| CoreMark, iterations per second | **111.09** | 57.54 |
| CoreMark per MHz | **2.31** | 1.20 |
| GPIO toggle, TI driver call, cycles | 19.2 | 31.0 |
| GPIO toggle, register write in a loop, cycles | 2.1 | 9.0 |
| One register write (16 back-to-back), cycles | 2.0 | 2.25 |
| Interrupt latency, pend to handler, direct vector, cycles (includes one SysTick read) | 29 | 28 |
| Flash used (measured build) | 36.8 kB | 25.6 kB |

Each build was run once (measured once); the loop rows are the best of 5 passes within that run. Both runs pass CoreMark's own check ("Correct operation validated", crcfinal 0x4983). Clock read back as 48 MHz (`ClockP_getCpuFreq`); a short host-timed check of the cycle counter gave 48.02 and 48.07 MHz, which agrees but over only 0.3 s. Flash wait states read live from the chip: 2 (1T) and 2 (2T), the factory-trimmed values. Cache, prefetch and micropredictor on (`CCHCTRL` 0x7). Interrupt latency was the same on all 200 tries in each build.

How it compares:

- **Against Arm's figure.** Arm's published CoreMark for the Cortex-M0+ is 2.46 per MHz (Arm Application Note 350, CoreMark on Cortex processors). 2.31 is 94% of it, with flash wait states in the path.
- **Against the MSPM0G3507**, same core, same compiler: 2.46 per MHz at 32 MHz and 2.30 at 80 MHz (rows `coremark_per_mhz` in `data/results/mspm0g3507.csv`). The two chips land within 1% of each other per MHz at full speed.
- **Against Zephyr with GCC on this same chip** (`coremark` rows in `data/results/cc2340r5.csv`): CoreMark 84.07 there (the build named O3, which is really -O2 with loop unrolling), 111.09 here, 32% more. That comparison changes the compiler and the operating system at once, and Zephyr's timer setup on this chip as well, so the 32% is not the compiler alone.

Every number above is a row in `data/results/cc2340r5.csv`.

## Run it

Run from this folder. `--build-only` touches no hardware.

```bash
./run.sh O3 --build-only
```

```bash
./run.sh Oz --build-only
```

```bash
./run.sh O3
```

```bash
./run.sh Oz
```

The first two build and check the CCFG. The last two also flash with UniFlash and capture the serial output to `results/run-<variant>.txt` (a folder the script makes). Flashing runs `/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c <connection file> -e -f -u build-<variant>/bench.hex` after a listener opens `/dev/cu.usbmodem<probe serial>1` at 115200. The connection file is the board folder's `CC2340R5.ccxml`; `run.sh` names its full path in the `CCXML` line near the top, so change that line if your copy of the repo is somewhere else. Change the probe serial in the connection file, and the port name (`SERIAL_DEV`), to match yours. `-e` prints progress messages; it is `--verbose`, not erase. Capture stops at `BENCH DONE` or after 300 seconds. The script also saves the host time for every line, for the clock check below.

The script refuses to flash unless the generated `ti_devices_config.c` has `allowDebugPort`, `allowChipErase`, `allowFlashProgram` and `allowReturnToFactory` all `CCFG_PERMISSION_ALLOW`.

**Our mistake, now fixed in `run.sh`:** the first -Oz capture stopped on the previous program's `BENCH DONE` line, because the old program repeats its report every 10 seconds, and saved the wrong run. The listener now waits for a `BENCH DONE` that follows this build's own `I,optimization` line.

## Recipe

| Item | Value |
|---|---|
| Board | LP-EM-CC2340R5 (CC2340R52: Arm Cortex-M0+, 512 kB flash, 36 kB RAM), XDS110 probe. Board revision: not recorded |
| SDK | SimpleLink Low Power F3 SDK 9.21.00.36 LTS, NoRTOS |
| Starting point | `examples/nortos/LP_EM_CC2340R5/drivers/empty` (ticlang makefile and linker file) |
| SysConfig | 1.26.3 CLI |
| Compiler | TI Arm Clang 5.1.1.LTS (from CCStudio 21.0.1) |
| Flags | `-O3` or `-Oz`, plus `-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -gdwarf-3` and the SysConfig `.opt` file, as in the SDK makefile |
| CoreMark | EEMBC sources unmodified (commit 1f483d5, SHA-1 in `coremark/coremark-source.sha1`), new `core_portme.c/.h`. `PERFORMANCE_RUN` (seeds 0, 0, 0x66), `MEM_STACK`, 2k iterations |
| Code location | Flash. No `.TI.ramfunc` section in either map |
| CPU clock | 48 MHz, SysConfig default. `ClockP_CPU_FREQ` is 48000000 for CC23X0 in `kernel/nortos/dpl/ClockPLPF3.h`, printed at run time from `ClockP_getCpuFreq()` |
| Flash wait states | Not set by this program. `hw_fcfg.h` says the factory trim is "Flash 1T wait states (default: 2 WS trimmed)" and "Flash 2T wait states (default: 5 WS untrimmed, 2 WS trimmed)". The program prints the live `VIMS.FLWS1T`, `VIMS.FLWS2T` and the factory trim values (`I,vims_flws1t`, `I,fcfg_flws1t` and so on) |
| Cache | VIMS has a cache, prefetch and micropredictor (`VIMS.CCHCTRL`, `hw_vims.h`). Not set by this program; the live register is printed as `I,vims_cchctrl` |
| CCFG | SysConfig default, byte-identical to the stock `empty` example's generated `ti_devices_config.c` (diffed 2026-10-05) |
| Power | `PowerLPF3_DISALLOW_STANDBY` and `PowerLPF3_DISALLOW_IDLE` constraints set, so the CPU never sleeps and SysTick counts every cycle |
| Supply | USB, through the LaunchPad. Not measured |
| Timer | SysTick on the CPU clock, 24-bit, with a wrap counter: one tick is one CPU cycle (`bench_timer.h`) |
| Date | Built 2026-10-05 |

Changes from the SDK `empty` example, all of them:

1. `bench.syscfg` is `empty.syscfg` plus the four UART2 lines copied from the SDK's `uart2echo.syscfg` (XDS110 backchannel UART).
2. `lpf3_nortos.cmd`: stack 1 kB raised to 4 kB, because CoreMark keeps its 2 kB work area on the stack. Heap stays at the SDK's 8 kB.
3. `makefile`: more source files, `OPT` instead of the fixed `-Oz`, runs from `build-<variant>/`, and `-Wl,--search_path=.` so the linker file finds the generated `.genmap`.
4. `bench.c` replaces `empty.c`. `main_nortos.c` is the SDK's, unchanged.

This follows the project's benchmark method (docs/methodology.md, section 11) and mirrors the MSPM0G3507 bare-metal work in mspm0g3507/mcu-bench/baremetal/.

## What it measures

Same method as the MSPM0G3507 bare-metal pass: loops timed by the cycle counter, best of 5.

| Row | What |
|---|---|
| `timer_read` | one `bench_cycles()` call |
| `empty_loop` | loop overhead |
| `gpio_toggle_driver` | `GPIO_toggle(CONFIG_GPIO_LED_0)`, the TI driver call |
| `gpio_toggle_register` | direct write to `GPIO.DOUTTGL31_0` in a loop |
| `gpio_toggle_str16` | one write to `DOUTTGL31_0`: 16 back-to-back STR in inline asm, minus a SysTick read pair |
| `systick_read_pair` | two back-to-back SysTick reads |
| `irq_latency_best`, `_worst_of_200` | pend LRFD IRQ0 (IRQ 6, the line the Zephyr run used) through the NVIC, read SysTick as the handler's first line. Direct vector, installed with `HwiP_plug`, no wrapper |

The LED used is the SDK's `LED0`, DIO14 (red). The Zephyr run toggled DIO15 (green). Same register either way.

Output on the XDS110 UART at 115200, in the shared benchmark program's format: `BENCH START`, `I,key,value` lines, `R,name,cycles,iterations,description` lines, CoreMark's own report (including `crclist`, `crcmatrix`, `crcstate`, `crcfinal` and "Correct operation validated"), then `BENCH DONE`. The whole report repeats every 10 seconds, with `I,pass,<n>`.

Clock check: `I,cycles_now` is printed at the start and end of each report. Cycles between two passes divided by the host seconds between those lines gives the real CPU clock.

## Memory, from the linker map MEMORY CONFIGURATION

| Build | Flash used, of 512 kB | SRAM used, of 36 kB | CCFG |
|---|--:|--:|--:|
| -O3 | 36.8 kB (0x9318) in the measured build; 36.6 kB (0x9244) after the rebuild that removed the compiler's version string | 15.8 kB (0x3F02) | 2 kB |
| -Oz | 25.6 kB (0x6692) in the measured build; 25.4 kB (0x65BE) after the rebuild | 15.8 kB (0x3F02) | 2 kB |

SRAM includes the 8 kB heap and the 4 kB stack, both reserved, not used. Whole program; split by component is not done.

## What to do next

- Energy per CoreMark iteration and sleep current need a power meter. **Tool or docs:** the LP-XDS110ET probe's EnergyTrace has no Mac support.
- Split the memory figures by component.

## Files

| File | From |
|---|---|
| `bench.c`, `bench_timer.h`, `coremark/core_portme.c`, `coremark/core_portme.h` | written here |
| `main_nortos.c` | SDK `empty` example, unchanged |
| `bench.syscfg`, `lpf3_nortos.cmd`, `makefile` | SDK `empty` example, changed as listed above |
| `coremark/core_*.c`, `coremark/coremark.h`, `LICENSE.md` | EEMBC, unmodified |
| `run.sh` | build, check, flash, capture |
