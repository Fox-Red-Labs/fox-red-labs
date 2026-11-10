# How do you run a program on the TI LP-AM243 LaunchPad development kit from a Mac, with the boot switches as shipped?

**Give the board its own 5 V, 3 A USB-C charger, plug in the micro-B debug cable, and run `/Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/ram_run.js <image.out>` on a single-core image: it loads into RAM and starts in about 4 s, with flash and switches untouched.** CoreMark gives 2,931 per second on one R5F core at 800 MHz and 11,730 on all four at once, and upstream Zephyr's blinky runs the same way.

Terms: [vocab](../docs/vocab.md), [FAQ](FAQ.md). 1 kB = 1,024 bytes. **XDS110** is the debug probe on the board; **DSLite** is the command-line loader inside CCStudio; **SysConfig** is TI's pin and driver configuration tool; **IPC** is messages between cores; **CC pins** are the USB-C connector's configuration-channel pins, where a charger states its current; **CS_DAP_0** is the debug-port access point the debugger connects through; **TMU** is a hardware trigonometry unit; **M4F** is the chip's Cortex-M4F core; **LD9** is the red power-error LED. **R5F** is the chip's Arm Cortex-R5F real-time core. **OCSRAM** is the on-chip RAM the SDK calls MSRAM. **TCM** is the fast memory beside each core.

## The board

| Item | Value | Read from |
|---|---|---|
| Chip | AM2434 in the ALX package: 4 Cortex-R5F, 1 Cortex-M4F, 2 PRU-ICSSG | The debugger reads AM2434_ALX; SPRSP65J |
| Clock | R5F at 799.98 MHz, against the Mac's clock over 31 one-second lines | `data/results/am243x.csv` row `coremark` |
| Memory | 2 MB OCSRAM, 64 kB TCM per core in dual-core mode, no DDR on this package | SPRSP65J |
| Flash | 64 MB quad SPI on the board; the debugger's window repeats after 32 MB | SPRUJ12F; backup 2026-11-05 |
| Debugger | XDS110 on micro-B J20, console `/dev/cu.usbmodem<serial>1` at 115200 | The probe, 2026-11-05 |
| Power | USB-C J10 must advertise 3 A on its CC pins | SPRUJ12F; this board |
| Math accelerator (TMU) | Not listed in the data sheet | SPRSP65J |

## Ten-minute path (load about 4 s)

You need the board, a 5 V, 3 A USB-C charger (the board's user guide SPRUJ12F section 3.1 requires one), a micro-B cable, CCStudio 21.0.1, TI Arm Clang 4.0.4.LTS, SysConfig 1.27.0 and the MCU+ SDK in `~/ti`. The SDK has no Mac installer: run TI's Linux `.run` installer in an amd64 Linux container, then copy the folder to `~/ti`.

1. Bench: plug USB-C into J10 (the power input), then micro-B into J20 (the debug connector). LD9 is dark and LD6, LD7 and LD8 are on. Red LD9 means the charger did not advertise 3 A.
2. Terminal: run `am243x/prebuild.sh`. It builds 15 SDK examples; each image is the `.out` file in that example's `am243x-lp/r5fss0-0_nortos/ti-arm-clang` folder under `~/ti/mcu_plus_sdk_am243x_12_01_00_33/examples` (CoreMark is `benchmarks/coremark`).
3. Terminal: open the console and run an image:

```
python3 am243x/tools/ram_capture.py <image.out> run.log --until "All tests have passed"
```

4. Read `run.log`. A power cycle returns the board to the firmware in its flash.

For Zephyr, set `AM243X_BARE=1` in front of the same command and pass `zephyr.elf`.

## One-command build

`am243x/prebuild.sh` builds 15 SDK examples, including blink, CoreMark, Dhrystone, four bootloaders, IPC and PRU. One example builds in 2.2 s after `make clean`. It does not write to the board.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| CoreMark, TI's example | 2,931 per second (TI's page lists 3,015.63) | -O3, code in OCSRAM, 15k iterations | measured | `coremark` `ti-port-o3-ocsram` |
| CoreMark, same files and port as the other boards | 2,931 per second, 3.664 per MHz | -O3, CoreMark's code in TCM, 30k iterations | measured | `coremark` `ticlang-bare-o3-tcm` |
| CoreMark on the other three cores | 2,926 to 2,928 per second | -O3, OCSRAM, one core at a time | measured | `coremark` `ticlang-bare-o3-ocsram-core1-alone` and on |
| CoreMark on all four cores at once | 2,931 and 2,933 per core, 11,730 in all; the same from TCM | -O3, one image per core, each in its own 256 kB of OCSRAM, timed by each core's cycle counter | measured | `coremark` `ticlang-bare-o3-ocsram-all4-sum` and `-tcm-all4-sum` |
| CoreMark with TI Arm Clang 5.1.1 | 3,151 per second, 3.939 per MHz (+7.5%); TCM 3,154 | -O3, OCSRAM, R5FSS0-0 alone; 5.1.1 is the copy inside CCStudio | measured | `coremark` `ticlang-bare-o3-ocsram-cgt511` |
| IPC round trip, TI's ipc_notify_echo | 3.19 us per partner with four partners (three R5F, one M4F), 2.08 with three, 1.61 with one | 400,000 round trips, three runs each, System Reset and a 12 s wait before every start | measured | `ipc_roundtrip_us` `freertos-to-nortos-4-remotes`, `-3-remotes`, `-1-remote` |
| Time to first blink, command line | 31 s | Board already powered and surveyed, SDK installed; edit half period 1 s to 250 ms, build, RAM load, verify 16 pin edges 250 ms apart; scripted, nobody at the bench. The steps take 13 s; the rest is waiting between scripted steps | measured | `time_to_blink` `cli`; TIMELINE.md |
| PRU0 output register write | 5.3 ns (unrolled), 10 ns (loop); PRU clock 200 MHz | PRU cycle counter, no pad routed | measured | `pru_toggle_ns`, `pru_clock_mhz` |
| Dhrystone, TI's example | 1.941 DMIPS per MHz (TI's page lists 1.9675) | -O3 | measured | `dhrystone` `ti-example-o3` |
| Dhrystone, inlining off | 1.505 DMIPS per MHz | -O3 | measured | `dhrystone` `ticlang-bare-o3-noinline` |
| FOC step | 245 cycles, 0.31 us | TI's software math, OCSRAM | measured | `foc_step` `sdk-ocsram` |
| Interrupt latency | 182 cycles, 228 ns best case (up to 248 cycles) | Software-pended, through TI's dispatcher | measured | `irq_latency` `bare` |
| GPIO pin write | 147 cycles, 0.184 us | Raw store to the GPIO block, loop and counter read included | measured | `gpio_toggle` `register` |
| Eighteen fresh AI agents, three jobs | 3 of 3 with our notes on blink, CoreMark and the red-LED question; without them 3, 3 and 1 of 3. Agent time per run, ours against none: blink 2.9 to 4.1 min against 4.1 to 6.1; CoreMark 1.8 to 2.0 min against 5.9 to 10.7 (the cost per run is in `agent_cost` rows) | Sonnet 5.5, 2026-11-08, each result checked by us | measured | `agent_*` rows in `data/results/am243x.csv` |
| Zephyr | hello_world and blinky run | Upstream `lp_am243/am2434/r5f0_0`, from RAM | measured | `zephyr_support` `upstream` |

Limits: one board, on the charger we had. GPIO stores cost about 15 times the cycles they do on the AM263P (147 against 10.02, `gpio_toggle` rows), so a fast software-toggled pin belongs on a PRU core: a PRU write takes 5 to 10 ns by the PRU's cycle counter; no scope has seen the pin. TI's multi-core IPC example hangs on a second start unless you run `am243x/tools/sysreset.js` and wait 12 s first. The 31 s blink starts at an already powered board, so there is no first-power time.

## If you see this

- If `ipc_notify_echo` hangs on its second start, run `am243x/tools/sysreset.js`, wait 12 s, start again.
- If you see error `-1170` at `CS_DAP_0` and a red LED, give the board a 3 A USB-C charger, then replug the micro-B (the expected fix; not yet confirmed with a single 3 A source).
- If `memory.loadProgram` hangs in a script, use `am243x/tools/ram_run.js`.
- If Zephyr stops with `Synchronous External Abort`, set `AM243X_BARE=1`.

The rest are in [FAQ.md](FAQ.md).

## Where these facts come from

Checked 2026-11-08 on an Intel Mac with CCStudio 21.0.1, MCU+ SDK for AM243x 12.01.00.33, TI Arm Clang 4.0.4.LTS, SysConfig 1.27.0 and Zephyr 4.5.0-rc2 (a915b2f1267). Rows are in `data/results/am243x.csv` and logs in `am243x/bench/`. Documents: AM243x data sheet SPRSP65J and EVM user's guide SPRUJ12F.
