# GCC builds of the MSPM0 SDK examples for the MSPM0G3507 and MSPM0L1306

224 of 224 driverlib examples that ship a `gcc/` makefile built with GCC, unchanged, for both LaunchPad kits. GCC code is a median 15 percent larger than TI Arm Clang (`.text`, 160 bytes at the median). Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes.

Built, and for the LP-MSPM0L1306 also run on silicon: 87 of 87 ran with no fault (section "Run on silicon"). The LP-MSPM0G3507 is not run.

## Headline

| Board | Examples | Ship a gcc makefile | Built with GCC unchanged | Built with TI Arm Clang | Median text, GCC / ticlang |
|---|---|---|---|---|---|
| LP_MSPM0G3507 | 141 | 136 | 136 | 140 | 1.14 |
| LP_MSPM0L1306 | 92 | 88 | 88 | 91 | 1.16 |
| Both | 233 | 224 | 224 (100 percent) | 231 | 1.15 |

- "Unchanged" means the shipped `gcc/makefile`, `device_linker.lds` and sources were not edited. Only the make variables in `imports.mak` were set on the command line (compiler path, SysConfig path, SDK path).
- The 9 examples with no `gcc/` folder are not GCC failures. They are assembly or library-only examples: `blinky_asm`, `cinit_bypass`, `empty_asm`, `empty_driverlib_library` (both boards) and `sram_parity` (G3507). The ticlang side has the same gap for `empty_driverlib_library` (2 of 233 have no ticlang makefile).
- GCC warnings: 6 in total across 4 examples. Example: `dl_timer.c:799:13: warning: '...captPairConfig...' may be used uninitialized [-Wmaybe-uninitialized]`.
- Build time per example, one at a time inside a 4-way parallel run: median 4.5 s with GCC, 5.6 s with ticlang. Most of it is SysConfig generation.

## Size comparison (224 examples built by both)

Both tools measured with Berkeley `size` (text, data, bss) on the linked `.out`. Same source, each toolchain's own shipped makefile flags (GCC `-O2`, ticlang as shipped).

- `.text` GCC / ticlang: median 1.15, quartiles 1.01 and 1.29. GCC is larger in 174 examples and smaller in 50.
- Median difference: +160 bytes of `.text`. `.data` is 0 in almost every example on both.
- Smallest ratio 0.61 (`aes_cbc_256_enc_dec`, 1.1 kB vs 1.7 kB). Largest, the C++ ones: `empty_cpp` 4.3 kB vs 376 bytes (11.6x), `gpio_toggle_output_cpp` 4.4 kB vs 512 bytes. GCC pulls in libstdc++ and exception support that TI's runtime does not. Next largest: `empty_driverlib_src` 2.6x and `adc12_monitor_supply` 1.9x.
- Not comparable: `.bss`. TI's linker file reserves a 512 byte stack inside `.bss` (median 517 on G3507); the GCC linker file does not count a stack (median 8). Compare `.text` only.
- Example: `gpio_toggle_output` on G3507 is 688 bytes with GCC (192 vector table plus 496 code) and 472 with ticlang.

## Method

1. `run.sh` makes a scratch SDK in `/private/tmp/gcc-mspm0-work`: `source`, `kernel`, `.metadata`, `tools` and `imports.mak` are symlinks to `/Applications/ti/mspm0_sdk_2_11_00_07`; the nortos `driverlib` examples for the board are copied. Nothing is written into `/Applications/ti`.
2. For each example, `build_one.sh` runs the shipped makefile in `gcc/` and then in `ticlang/` (`make MSPM0_SDK_INSTALL_DIR=<scratch> SYSCONFIG_TOOL=<sysconfig_cli.sh> GCC_ARMCOMPILER=<shim> TICLANG_ARMCOMPILER=<ticlang>`). The makefiles call SysConfig to generate `ti_msp_dl_config.*` and `device.lds.genlibs`.
3. A build counts only if make returned 0 and a `.out` exists. Sizes come from `arm-zephyr-eabi-size` and `tiarmsize`. Spot check: the G3507 `gpio_toggle_output.out` is an ARM ELF, `Tag_CPU_arch: v6S-M`, Thumb-1, entry 0xc0, vector table at 0.
4. One JSON line per example (built, rc, secs, text, data, bss, warnings, first_error). `analyze.py` prints the tables and failure groups from those lines.

Rerun: `JOBS=4 ./run.sh` (both boards, about 15 minutes), or `./run.sh LP_MSPM0L1306`, or `ONLY=gpio_toggle_output ./run.sh LP_MSPM0G3507`. Then `python3 -I analyze.py`.

## Tool versions

| Tool | Version |
|---|---|
| GCC | arm-zephyr-eabi-gcc 12.2.0 (Zephyr SDK 0.17.4), newlib with `nano.specs`, `nosys.specs`, multilib `thumb/v6-m/nofp` present |
| TI Arm Clang | 5.1.1.LTS (`ti-cgt-armllvm_5.1.1.LTS` in CCS 20.2.1) |
| SysConfig | 1.26.3 |
| MSPM0 SDK | 2.11.00.07 |

The SDK release notes name Arm GNU `gcc-arm-none-eabi-9-2019-q4-major` as the GCC. It was not installed. The Zephyr SDK GCC on this Mac (already installed, 1.1 GB) links Cortex-M0+ bare-metal with the SDK's flags, so no download was needed and no disk space was added. This is GCC 12.2, three major versions newer than the documented one, so the result says "builds with a newer GCC", not "builds with the exact documented GCC".

One shim: the makefiles call `$(GCC_ARMCOMPILER)/bin/arm-none-eabi-gcc` and `-L$(GCC_ARMCOMPILER)/arm-none-eabi/lib/...`. The Zephyr tools are named `arm-zephyr-eabi-*`. `run.sh` writes three tiny wrapper scripts named `arm-none-eabi-gcc|size|objdump` that exec the Zephyr tools, plus a symlink `arm-none-eabi` to the Zephyr target folder. Same compiler, only the prefix differs.

## Run on silicon (LP-MSPM0L1306, 2026-10-14)

**87 of 87 GCC images ran with no fault. 0 faults. 84 of 87 matched what the same example did when built with TI Arm Clang, and 24 of those have a clear pin or serial check (behavior verified).**

| Result | Count |
|---|--:|
| GCC images run (all flashed and verified on the first attempt) | 87 |
| Running when halted after 4 s | 67 |
| Stopped at a `__BKPT` (documented end point, or the example's own error check) | 20 |
| HardFault, lost debugger, flash failure | 0 |
| Same observation as the ticlang run (verdict, LED pins, whether serial text appeared) | 84 |
| Different observation | 3 |
| Behavior verified (pin toggling with the same changing pins, or identical serial text) | 24 |

"Same observation" is the runner's record for the same example from the TI Arm Clang run: verdict, LED pin states, which pins change, and whether bytes came out of the serial port. It does not mean the same code path or cycle count; the program counter at halt differs because GCC code is laid out differently.

### Method

The runner is the one used for the TI Arm Clang builds, unchanged (OpenOCD 0.12.0, probe serial <probe serial>). For each image: refuse it if any part lands outside main flash or SRAM, reset halted, program and verify, system reset, run 4 s while sampling the LED pins (PA0 LED1, PA26 and PA13 and PA27 RGB) and reading the serial port at the example's own baud rate, then halt and read PC, LR, xPSR and the stack. A fault is IPSR 2 or higher after the halt, or a stop in a fault handler. Order and split are the ticlang run's: 74 examples first, then `flashctl_dynamic_memory_protection` and the 12 low-power and watchdog examples one at a time, last. Images came from the build above (unchanged makefiles, GCC 12.2).

### Skip list

Same rule as the TI Arm Clang runs, applied to the 88 GCC images:

- Skipped, 1: `flashctl_nonmain_memory_write` (writes NONMAIN, the boot configuration; a bad write can lock the chip).
- The other ticlang skips (`boot_manager/boot_application`, the three `bsl/secondary_bsl_*`, the three `bsl/bsl_host_mcu_to_*`) have no `gcc/` makefile, so no GCC image existed.
- Not in the run because they ship no `gcc/` folder: `blinky_asm`, `cinit_bypass`, `empty_asm`, and every example outside `driverlib` (demos, msp_subsystems, battery gauge and so on ship no GCC makefile). 87 of the 147 images the TI Arm Clang run covered had a GCC counterpart.
- The wire, second-board and sensor examples ran too, as in the TI Arm Clang run; they are marked "running" and prove only that nothing faulted.

### Behavior verified (24)

- Pins: `gpio_toggle_output`, `gpio_toggle_output_cpp`, `gpio_toggle_output_hiz`, `systick_periodic_timer`, `nvic_interrupt_disable`, `nvic_interrupt_grouping`, `timx_timer_mode_periodic_sleep`, `flashctl_blank_verify`, `flashctl_multiple_size_read_verify`, four `spi_controller_*` examples, and the low-power ones `sysctl_power_policy_sleep_to_standby`, `sysctl_power_policy_sleep_to_stop`, `timx_timer_mode_periodic_standby`, `timx_timer_mode_periodic_stop`, `wwdt_interval_timer_lfosc_standby`, `wwdt_window_mode_periodic_reset`, `flashctl_dynamic_memory_protection`. Each toggled the same LED pins as the ticlang build.
- Serial text: `uart_rw_multibyte_fifo_poll` (`MSP!MSP!`), `uart_tx_multibyte_fifo_dma_interrupts` and `uart_extend_manchester_send_packet` (8 bytes, identical to ticlang), and `uart_tx_console_multibyte_repeated_fifo_dma` (the `==== MSPM0 Console Test ====` banner and the numbered `> Testing...N` lines, 171 bytes against 195 for ticlang in the same 4 s; GCC also showed one 12 byte garbled run after the second banner, not seen on the ticlang run).

### The 3 that differ

All three read a pin nobody drove, so a difference is expected and repeated on two reruns.

- `comp_hs_dac_vref_external`: the comparator input is floating; the LED followed noise on the ticlang run and stayed low with GCC.
- `uart_tx_hw_flow_control`: the CTS pin is unconnected. GCC sent the alphabet (78 to 101 bytes in 4 s); ticlang sent nothing and waited on CTS.
- `i2c_controller_target_dynamic_switching`: no I2C target is attached. GCC reached the example's error `__BKPT` inside the window; ticlang was still retrying. Both end in the same NACK path.

### Restore check

The board's 64 kB flash was read and saved before the run, and its SHA-1 compared with the saved as-received image (853 bytes differed, all in the vector table at 0x04 to 0x3FF; the demo ran). After the run the as-received image was written back (erase all 64 sectors, write, verify) and the whole flash read back identical, SHA-1 d7e1f9a0f0ebfc170a8892414e54a149c7249195. The board is on TI's out-of-box demo as received. The restore was repeated after the three reruns of the differing examples, with the same readback.

### Limits

One 4 s run per image (three examples twice more). Pin sampling is 8 reads 250 ms apart, so a fast toggle shows as toggling and a slow one can look still. "Ran" says no fault; only the 24 above compared behavior. Results rows: `toolchain_gcc_*` in `data/results/mspm0l1306.csv`.

## Failures

None on the GCC side: 0 compile errors, 0 link errors, 0 missing files across 224 builds. The only non-builds are the 9 examples that ship no `gcc/` makefile (listed above).

## What this does not show

- Run on silicon for the L1306 only (see "Run on silicon"). The G3507 builds are not run. Running without a fault is weaker than a measured behavior: only the 24 marked behavior verified were compared with the ticlang run.
- Only nortos driverlib examples for two boards. Other boards, FreeRTOS, TI Drivers and the library examples were not built.
- The official Arm GNU Toolchain 9-2019-q4 was not run.
- Sizes use each makefile's own flags (not matched optimization levels), so the ratio mixes compiler and flag differences.
