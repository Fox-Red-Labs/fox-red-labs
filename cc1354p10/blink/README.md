# How do you blink the LED on a TI LP-CC1354P10-1 from a Mac terminal?

Works (2026-10-22): the red LED (DIO6) blink image builds in 6 s and runs on the board 29 s after the clock starts (build, power-cycle the probe, load, run, first counter read). `blink_count` read 8, 18, 27, 31, 35, 38, 42, 46 over 3.8 s: about 9.8 toggles a second, a 0.2 s blink period.

Setup: LP-CC1354P10-1, with its USB hub port switched by `uhubctl` (the debugger connects only in the first seconds after power-on). SimpleLink Low Power F2 SDK 8.33.00.16 from TI's GitHub (tag `lpf2-8.33.00.16`, sparse checkout, 149 MB), its three missing libraries built with `cc1354p10/radio-rx/build_f2_libs.py`, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0.

What differs from the SDK's own `drivers/empty`: TI's GitHub copy of the SDK carries no `examples/` folder, so this blink is `drivers/empty`'s shape written out here (the same SysConfig board, LED and no-RTOS main; the makefile adapted from the one `cc1354p10/baremetal` already uses). The toggle is 0.1 s and a counter makes it readable by the debugger.

Files: `blink.c`, `blink.syscfg`, `makefile`, `main_nortos.c`, `cc13x4_cc26x4_nortos.cmd`, `load_and_read.py`. Build: `mkdir build && make -C build -f ../makefile SYSCONFIG_TOOL=<sysconfig_cli.sh> TICLANG_ARMCOMPILER=<ti-cgt-armllvm_5.1.1.LTS> GCC_ARMCOMPILER= IAR_ARMCOMPILER=`. Load and read: `ccs/scripting/python/run.sh load_and_read.py build/blink.out <blink_count address from the linker map>`. Terms: [docs/vocab.md](../../docs/vocab.md); **CCFG** is the chip's boot configuration area. 1 MB = 1,024 kB.

Next: [bare-metal benchmark](../baremetal/README.md), [FAQ](../FAQ.md).

Limits: the LED level was not read (the GPIO output register cannot be read while the program runs), so the proof is the counter, not the pin. One run. The image's CCFG is the SysConfig default; it differs from the board's own in two words (0x4 and 0x28); debug and bootloader fields are the same.

Backup and restore: main flash (1 MB) and the CCFG read twice identical before the run; the board's own image and CCFG were flashed back and read back identical.
