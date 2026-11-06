# TI LP-AM263P LaunchPad development kit: notes for an AI coding assistant

That the board has no flash inside the chip and an external flash that some SDK examples erase, that the serial port must be open before a load, and that a handful of other traps cost hours if nobody warns it. Each item comes from runs on one board unless it says otherwise.

Errors and fixes: the [LP-AM263P FAQ](FAQ.md) in the Fox Red Labs repo. Terms: **R5F** is the chip's Arm Cortex-R5F real-time core (four of them); **DSLite** is the command-line loader inside CCStudio; **OCSRAM** is the on-chip RAM the SDK calls OCRAM; **TCM** is a core's tightly coupled memory; **TMU** is the math unit for sine and cosine; **lockstep** is the mode where the second core of each pair repeats the first to catch faults. 1 kB = 1,024 bytes. Checked 2026-10-11 on an Intel Mac, macOS 15.7.9, with MCU+ SDK for AM263Px 26.01.00.08, CCStudio 21.0.1 (DSLite 21.0.0.4166), TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and XDS110 probe firmware 3.0.0.29, on one LP-AM263P Rev A (PROC171A). The board is no longer on the bench; nothing here can be rerun.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `am263p/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The board

| Part | Fact |
|---|---|
| Chip | AM263P4: four Cortex-R5F cores at 400 MHz; reading `0x50D80038`, bits 31 to 13 are `0x3088D` (`am263p/README.md`, `data/results/am263p.csv`) |
| Memory | 32 MB external flash; no flash inside the chip. The data sheet lists 3 MB of on-chip RAM; a program using above 2 MB did not start here (`am263p/RESULTS-am263p.md`) |
| LEDs | Eight green LEDs on an I2C expander (TPIC2810, address 0x61), not on chip pins, so the SDK has no GPIO blink example (`am263p/README.md`) |
| Serial | Console is the probe's first port, `/dev/cu.usbmodem<serial>1`, 115200 8N1; the second port (`...4`) stayed silent (`am263p/FAQ.md`) |
| Power | USB-C at 5 V and 3 A powers the board; the micro-B cable only feeds the probe. A weak source brings up only the 3.3 V rail and lights the red `SAFETY_ERROR` LED (`am263p/README.md`, `am263p/FAQ.md`) |
| Debugger | XDS110 on the board, probe firmware 3.0.0.29. CCStudio 21.0.1 has no LP-AM263P board file; the connection file was built by hand (`am263p/FAQ.md`) |
| Not on the chip | No GPMC bus: no base address, pin or driver in SDK 26.01 (`am263p/FAQ.md`). No display controller: a PRU generates RGB565 (`am263p/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MCU+ SDK 26.01.00.08, CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0. The SDK pins older versions (CCStudio 20.5.0, SysConfig 1.27.0, TI Arm Clang 4.0.4.LTS); the newer ones built every example tried (`am263p/FAQ.md`) |
| Build TI's blink | In `examples/drivers/i2c/i2c_led_blink/am263px-lp/r5fss0-0_nortos/ti-arm-clang` run `make i2c_led_blink.release.out DEVICE=am263px PROFILE=release TOOLS_PATH=$HOME/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0` (5.9 s after `make clean`, `data/results/am263p.csv` row `blink_build`) |
| Open the console first | A reader on `...1` must be running before the load; the examples print once, milliseconds after start (`am263p/FAQ.md`) |
| Load | `DSLite load -c am263p/AM263Px_ZCZ_C_<serial>.ccxml -t 120 i2c_led_blink.release.out` into RAM; 4.4 to 7.6 s per 0.9 MB image, first load 78 s (`time_to_blink` in `data/results/am263p.csv`, `am263p/FAQ.md`) |
| Check | The reader prints `I2C LED Blink Test Started ...`; the LEDs go on, then off one at a time, ten times, about 16 s (`am263p/README.md`) |
| Back up first | `am263p/tools/flash_backup.sh` reads the flash; read it twice and compare (`am263p/FAQ.md`, `am263p/README.md`) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Empty console after `DSLite load` | The port was opened after the program printed | Open `...1` before the load | The banner appears (`am263p/FAQ.md`) |
| DSLite error -615 | Only the micro-B cable plugged in, or a `SWD Mode` property copied from a Cortex-M33 connection file | Plug in the USB-C supply; delete the property | DSLite connects (`am263p/FAQ.md`) |
| `ModuleNotFoundError: No module named 'construct'` | The SDK's `.mcelf` boot-image step needs `construct` and `pyelftools` | Ignore it for a RAM load | The `.out` exists (`am263p/FAQ.md`) |
| `DSLite load` times out at 90 s | Cause not found | Run it again | The retry loaded in 5 s (`am263p/FAQ.md`) |
| `Error: Could not fetch next record for SQL statement (database disk image is malformed)` | A damaged device-database cache slot under `~/.ti/ccs2101/` | Remove the one slot no process holds (`lsof +D ~/.ti/ccs2101`) | The next load rebuilds it (`am263p/FAQ.md`) |
| `Does not match the target endianness, not loaded` on the third core | R5FSS1-0 needs its start-up script run once after a reset | Load that core once with the GEL connection file | Later loads work (`am263p/FAQ.md`) |
| Console shows the previous program after a load | Lockstep connection file left FORCE_WFI at 0x7 | Debugger System Reset (`am263p/tools/sysreset.js`) | Two programs with different banners load in turn (`am263p/FAQ.md`) |
| `Flash_norOspiPhyTune : PHY enabling failed!!! Continuing without PHY...` | Flash tuning pattern does not read back | Flash still works at 16.7 MHz; back up the sector at `0x80000` first | Zero mismatched bytes on every write checked (`am263p/FAQ.md`) |
| `warning: memory range not found: LOG_SHM_MEM` when building one core | The example is part of a multicore system project | Run the system project's `syscfg` step, then build each core with `SYSTEM_FLAG=true` | The core links (`am263p/FAQ.md`) |

## Rules for the assistant

1. **Back up the flash before loading any example that opens it.** `ospi_flash_io`, `ospi_flash_xip`, `ospi_flash_diag` and the `sbl_*` examples erase and write; `ospi_flash_diag` erases a 2 kB block at `0x80000`, and the boot image sits at `0x0` to `0x30000` (`am263p/FAQ.md`). The backup of this board lost one block, because `ospi_flash_diag` had already rewritten it (`am263p/FAQ.md`, "Mistakes to avoid").
2. **Opening the flash can write it.** With PHY tuning failing, TI's flash driver erases the 4 kB sector at `0x80000` and writes a pattern there (`am263p/FAQ.md`). Read the driver's open path as well as the example.
3. **Spell the optimization flag `-O3`.** A script that passed `-o3` built without optimization: 223 CoreMark per second instead of 1,464 (`am263p/FAQ.md`).
4. **Reset the debugger before each load when comparing examples.** 17 examples gave a different result depending on whether the chip was reset first (`examples_ran_clean` in `data/results/am263p.csv`); 4 of 9 early failures passed after `am263p/tools/sysreset.js` (`am263p/FAQ.md`).
5. **Load core 0 (R5FSS0-0) first.** Connecting to it with TI's usual connection file clears on-chip RAM and stops programs already running on cores 1 to 3 (`am263p/FAQ.md`).
6. **Time long runs with the cycle counter, not the RTOS clock.** On the FreeRTOS core `ClockP_getTimeUsec()` read 664 µs for an exchange of about 3.9 s under heavy interrupts (`am263p/FAQ.md`).
7. **Do not trust a pass line from `fan_blade_anomalydetection`.** It printed `Golden vectors matched: 0 not matched: 192` and then "All tests have passed!!" (`am263p/FAQ.md`).
8. **Zephyr needs the out-of-tree files in `am263p/zephyr/`.** Upstream 4.4.2 has no AM263P support; the port boots `hello_world` on R5FSS0-0, has no RTI timer driver, so `k_sleep` does not work (`am263p/FAQ.md`, `am263p/RESULTS-am263p.md`).
9. **The third core (R5FSS1-0) ran but its console lines did not reach the console.** Pass results back through shared memory (`am263p/FAQ.md`).

## What was measured

All at 400 MHz on R5FSS0-0 with both L1 caches on, loaded into RAM, unless the line says flash (`data/results/am263p.csv`).

| What | Number | Source |
|---|---|---|
| CoreMark -O3, code in OCSRAM / TCM | 1,464.2 / 1,497.8 per second (3.661 / 3.744 per MHz) | `data/results/am263p.csv` rows `coremark` |
| TI's CoreMark example | 1,464.5 per second | `am263p/RESULTS-am263p.md` |
| Dhrystone, -O3 no inlining / TI's example | 1.497 / 1.941 DMIPS per MHz | `data/results/am263p.csv` rows `dhrystone` |
| Sine plus cosine, TMU / TI mathlib / C library | 26.9 / 77.8 / 300.3 cycles | `data/results/am263p.csv` rows `math_sincos` |
| FOC step, TMU / TI mathlib / C library | 160.3 / 247.4 / 412.0 cycles | `data/results/am263p.csv` rows `foc_step` |
| Current loop, TMU / mathlib / C library | 1.24 / 1.45 / 2.06 µs | `data/results/am263p.csv` rows `control_loop` |
| Same loop in lockstep, TMU | 1.236 µs | `am263p/RESULTS-am263p.md` |
| Same loop beside CoreMark on the other three cores | 1.234 µs, same as alone | `am263p/RESULTS-am263p.md` |
| CoreMark from flash: no cache / 64 kB L2 | 1,394.5 / 1,464.5 per second | `data/results/am263p.csv` rows `ti-port-o3-xip`, `ti-port-o3-xip-l2-64k` |
| TI's memcpy benchmark, RAM / flash / flash with 64 kB cache | 14.60 / 126.55 / 34.57 ms | `data/results/am263p.csv` rows `xip_memcpy_tasks` |
| `memcpy` in a 64 kB block, library / 8-byte loop | 58.6 to 61.4 / 235 to 297 MB/s | `am263p/RESULTS-am263p.md`, `am263p/FAQ.md` |
| Four cores at once, CoreMark total | 5,854.0 per second | `data/results/am263p.csv` row `ti-port-o3-ocsram-all4-sum` |
| IPC round trip, FreeRTOS to bare-metal, 1 / 3 partners | 3.49 / 3.81 µs | `data/results/am263p.csv` rows `ipc_roundtrip_us` |
| IRQ / FIQ to first handler line | 195 / 144 cycles | `data/results/am263p.csv` rows `irq_latency` |
| SDK examples: built / printed a pass | 249 of 284 / 118 of 208 run | `data/results/am263p.csv` rows `examples_built`, `examples_ran_clean` |

Not run: ADC rate, UART loop speed, external-edge interrupt latency and power (they need wires or a meter), and a flash boot (`am263p/RESULTS-am263p.md`, `am263p/README.md`).

## Where the evidence is

`am263p/RESULTS-am263p.md` (tables), `am263p/FAQ.md` (every error with its cause tag), `data/results/am263p.csv` (every row), `docs/experiments/2026-10-10-am263p-xip-ladder.md`, `docs/experiments/2026-10-11-am263p-four-cores-mixed-os.md`, `docs/experiments/2026-10-11-am263p-lockstep-and-loaded-loop.md`.

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
