# TI LP-AM243 LaunchPad development kit: notes for an AI coding assistant

Three things to know first: the board stays dark on a USB-C source without a 3 A advertisement, the SDK's bootloaders need one exact SysConfig, and Zephyr's own images expect a bootloader's setup in front of them. Each line below comes from a build on the Mac or from TI's public documents, and says which.

Terms: [vocab](../docs/vocab.md). Errors and fixes: the [LP-AM243 FAQ](FAQ.md) in the Fox Red Labs repo. Terms: **R5F** is the chip's Arm Cortex-R5F real-time core (four of them); **DSLite** is the command-line loader inside CCStudio; **SBL** is TI's secondary bootloader; **OCSRAM** is the on-chip RAM. Checked 2026-11-08 on an Intel Mac with MCU+ SDK for AM243x 12.01.00.33, TI Arm Clang 4.0.4.LTS, SysConfig 1.27.0 and CCStudio 21.0.1, XDS110 probe firmware 3.0.0.15.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Paths in backticks starting `am243x/` are in the Fox Red Labs repo, where the evidence for each line lives.

## The board

| Part | Fact |
|---|---|
| Chip | AM2434 (ALX package): four Cortex-R5F up to 800 MHz, one Cortex-M4F, two PRU-ICSSG; 2 MB OCSRAM, no DDR on this package (TI data sheet SPRSP65J, `docs/arrivals/am243x.md`) |
| Flash | 64 MB quad SPI NOR at 0x60000000; the SDK's `sbl_ospi` loads the application from offset 0x80000 (`examples/drivers/boot/sbl_ospi/am243x-lp/r5fss0-0_nortos/example.syscfg`, `am243x/prebuild.sh`) |
| Power | USB-C must advertise 5 V at 3 A on its CC pins (no USB PD). A weaker or shared source lights red LD9 and the SoC stays off while the probe still enumerates (TI user guide SPRUJ12F section 3.1; `docs/arrivals/am243x.md`) |
| Debugger | XDS110 on micro-B J20; plug it in after the USB-C power. Connection file `am243x/AM243x_LP_S24L0096.ccxml` (drivers: cs_dap, cortexR, cortexM, debugssm, ctset2, pru, etbcs, csstm) |

## Run a program without moving a switch

`/Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/ram_run.js <image.out>` halts R5F core 0, resets only that core, stops TIMER8 and closes the interrupt controller (what the last program left on stops the next one in its interrupt handler), writes the ELF segments and starts it, RAM only, about 4 s. `am243x/tools/ram_capture.py <image> <log>` does the same with the console open. It works with SW4 as shipped (QSPI boot: `0x43000030` reads `0x2113`). CCS `memory.loadProgram` and `sciclient_ccs_init` hang in that mode. The console is `/dev/cu.usbmodemS24L00961` at 115200; open it before the load. R5F core 0 runs at 799.98 MHz by a 31-point fit against the Mac clock. Debug boot, SW4 1 to 4 ON and 5 to 8 OFF, is TI's documented route (`am243x/tools/init_load.js`, not yet run in that mode). The flash window at `0x60000000` repeats after 32 MB; a backup proves the first 32 MB only (backup 2026-11-05, contents end near 17 MB).

To read the chip while it runs, open a session on `CS_DAP_0` and read `<address>@System_View`, never connecting to a core: 972 samples a second with TI's LED example still passing (`am243x/tools/live_probe.js`, 2026-11-06). Connecting to a core halts it.

## Start here

| Step | This board |
|---|---|
| Tools | The SDK installer is Linux and Windows only; it was installed in an amd64 container and copied to the Mac. Use SysConfig 1.27.0: 1.28.0 and 1.28.1 fail every bootloader build (`am243x/prebuild.sh`, `am243x/FAQ.md`) |
| Build everything | `am243x/prebuild.sh` builds 15 SDK examples (blink, CoreMark, Dhrystone, four bootloaders, IPC, PRU); all build (`am243x/prebuild.sh`) |
| Build TI's blink | `make -s all PROFILE=release` in `examples/drivers/gpio/gpio_led_blink/am243x-lp/r5fss0-0_nortos/ti-arm-clang` with the paths in `am243x/prebuild.sh`: 2.2 s after `make clean` (`data/results/am243x.csv` row `blink_build`) |
| Never run | `ospi_flash_diag` erases the first flash block (its source calls `OSPI_norFlashErase`; `am243x/prebuild.sh`). Never run an OTP key writer: it burns fuses (`docs/arrivals/am243x.md`) |
| Zephyr | Upstream `lp_am243/am2434/r5f0_0` builds blinky and hello_world; `lp_am243/am2434/m4` builds hello_world, not blinky (no `led0` alias). Through a TI SBL (not tried here, SW4 moves) the image is converted with the SDK's `elf2rprc.js`, `xipGen.out.mac`, `multicoreImageGen.js` and the signing script (`data/results/am243x.csv` row `zephyr_support`). From RAM with no bootloader: `AM243X_BARE=1 am243x/tools/ram_capture.py <zephyr.elf> <log>` (a stub sets up the MPU and caches first); hello_world prints, blinky toggles on the console (interval not timed) (`am243x/bench/zephyr-*-2026-11-06.log`). `lp_am243` images set up no MPU or cache, so a plain load faults on the first LDREX in OCSRAM. |

## Several cores, a newer compiler, the PRU

| Topic | Fact |
|---|---|
| All four R5F cores | CoreMark on all four at once from one image per core, each in its own 256 kB of OCSRAM (0x70080000, 0x700C0000, 0x70100000, 0x70140000): 2,931 to 2,933 per core, 11,730 in all, within 0.1% of one core alone, from OCSRAM and from TCM (`data/results/am243x.csv` rows `coremark` `ticlang-bare-o3-ocsram-all4-sum`, `ticlang-bare-o3-tcm-all4-sum`; `am243x/tools/all4.sh`). Cores 1 to 3 must skip `System_init`: it talks to the device manager through core 0's message thread (`am243x/bench/all4/main_all4.c`). The result table in OCSRAM at 0x70060000 is cached: write back and invalidate by hand. |
| Compiler | TI Arm Clang 5.1.1 is already inside CCStudio 21.0.1 (`ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS`). Same CoreMark source, -O3: 3,151 per second against 2,931 with 4.0.4 (+7.5%); TCM still gives nothing over OCSRAM (3,154). `CG=<compiler folder> am243x/tools/build_bench.sh ...` selects it; the SDK libraries built with 4.0.4 link unchanged (`coremark` `ticlang-bare-o3-ocsram-cgt511`, `-tcm-cgt511`) |
| Mailbox IPC | TI's `ipc_notify_echo` loads one image per core with `ram_run.js` (M4F: `AM243X_CORE=Cortex_M4F_0`). Run `am243x/tools/sysreset.js` and wait 12 s before every start: nine starts in one power-up passed, 3.19 us per partner with four, 2.08 with three, 1.61 with one (`ipc_roundtrip_us`, `am243x/bench/logs/ipc/warm-*.txt`; `am243x/tools/ipc_build.sh`, `ipc_run.sh`). Without the chip reset a second start hangs in `IpcNotify_syncAll` on core 0, also after core resets, a drained mailbox and a changed load order (`am243x/bench/logs/ipc/all-hang-second-start.txt`) |
| PRU | A PRU0 firmware loaded and started from an R5F FreeRTOS image (`am243x/tools/pru_build.sh`) runs at 200.000 MHz by the R5F's clock; a write to R30 takes 1 cycle unrolled (5.3 ns with the loop) and 2 cycles in a four-instruction loop (10 ns); the pad was not routed and its rate not seen (`pru_toggle_ns`, `pru_clock_mhz`). The PRU's own counter is C11 offset 0xC with COUNTER_ENABLE (bit 3) in C11 offset 0 |

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-08. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

No lockup is recorded for this board.

<!-- recovery:end -->
