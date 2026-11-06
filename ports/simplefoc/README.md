# SimpleFOC on the LP-AM13E230 with the DRV8376EVM: the open FOC library runs its loop on TI silicon, the motor run waits for the supply

SimpleFOC (Arduino-FOC 2.4.0, MIT) lists no TI chip. It now builds for the LP-AM13E230 with no change to its files: 23.1 kB of flash, and 160 lines of port code (143 in the chip layer, 17 changed in the pin file) plus a 146-line sketch. Its control loop runs in 6.7 µs (open loop) and 9.6 µs (closed loop on the Halls) per pass on the 200 MHz core with the motor supply off. **The motor has not turned yet:** the bus read 3.7 V on 2026-11-03, so the supply was off, and no firmware here switches it.

Terms: [docs/vocab.md](../../docs/vocab.md), [LP-AM13E230 FAQ](../../am13e230/FAQ.md). FOC (field-oriented control): the motor's phase voltages are set from the rotor angle so the torque stays steady. Open-loop velocity: SimpleFOC turns the field at the commanded speed and never looks at the shaft. 1 kB = 1,024 bytes.

## How it is ported, and why this way

SimpleFOC is written for the Arduino API, but its chip-specific part is small and named: `src/drivers/hardware_api.h` lists the functions a new chip supplies (`_configure6PWM`, `_writeDutyCycle6PWM`), and the core files use about a dozen Arduino calls (`micros`, `digitalRead`, `pinMode`, `delay`) plus a `Print` class.

| Route | Verdict |
|---|---|
| The Arduino framework with a TI core | Rejected: no Arduino core exists for the AM13, and Zephyr (which the lab's Arduino route needs) has no PWM, ADC or GPIO driver for this board file (`am13e230/FAQ.md`). |
| Rewrite the SimpleFOC math in C | Rejected: it would no longer be SimpleFOC, and the library's value is that its classes, tuning and examples carry over. |
| **SimpleFOC unmodified, its own `hardware_api.h` layer implemented on TI DriverLib, an `Arduino.h` shim, TI Arm Clang, no framework** | **Taken.** `hal/am13_hal.cpp` (81 lines) implements the 6-PWM entry points on MCPWM4 and the Arduino calls on the DWT counter and GPIO; `shim/Arduino.h` (62 lines) holds constants and a `Print` that discards its text. |

Choices that follow from it:

- **6-PWM driver, not 3-PWM.** The DRV8376 takes six inputs. The lab's proven pin file (`am13e230/bldc-drv8376/bldc.syscfg`, 17 lines changed here: the UART removed) puts each B output as the complement of its A output, and the driver adds the dead time (the lab's sine image ran that way). The `dead_zone` argument is therefore unused.
- **Polling, not interrupts, for the Halls.** SimpleFOC's `HallSensor` has a polling mode: `loopFOC()` reads the three pins every pass. The sensor class is used as shipped.
- **Voltage mode only.** SimpleFOC's current-sense path (`LowsideCurrentSense`) needs another per-chip layer (`_configureADCLowSide`); not written. The sketch reads the three sense outputs itself, for the over-current trip and the log.
- **The sketch runs unattended** and writes `g_log` in RAM; a probe read halts the core, so it reads once, after the driver is off (`tools/run_simplefoc.py`). Guards, all in `app/main.cpp`: bus 8 to 15 V, nFAULT, 1.5 A on four passes in a row, a Hall edge every second in closed loop, time caps, driver off on any exit. The sense gain (1 V/A) is the lab's unverified assumption.

## What was expected, before any run

| Quantity | Expected | Why |
|---|---|---|
| Flash | 20 to 30 kB | Seven small classes, no TMU, no real-time library; TI's FOC image is 61.7 kB. |
| Loop time per pass | 6 to 12 µs open loop, 8 to 15 µs closed loop | Float math at 200 MHz with a look-up sine; TI's control interrupt is at most 5 µs with the TMU and a current loop. |
| Open loop, 20 Hz electrical (300 rpm) | Turns at exactly the commanded speed, peak current 0.3 to 0.5 A | V/f ramp from 0.25 V; back-EMF at 20 Hz is 0.73 V. |
| Closed loop on the Halls | Holds 300 rpm, with more speed ripple than TI's 2.5 to 4.1 percent | SimpleFOC's angle from a Hall sensor moves in 60 degree steps, and the motor's sectors are uneven (14.7 to 18.4 percent of a turn). |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Flash used | 23.1 kB (TI's Hall FOC port: 61.7 kB) | TI Arm Clang 5.1.1.LTS `-O2`; text 23,080 plus data 4; includes the sketch and the log code | measured | `port_flash_bytes` row, linker map |
| SimpleFOC unmodified | 3,097 lines compiled, 0 changed | `SHA256SUMS` in `third-party/simplefoc/` | measured | `third-party/simplefoc/SOURCE.md` |
| Port code | 160 lines (hal 81, shim 62, pin file 17 changed) plus sketch 146 | non-blank lines; TI's port changed about 650 lines of its 14,400 | measured | `simplefoc_glue_lines` rows |
| Loop pass, open loop | 1.35k cycles mean (6.74 µs), 1.57k worst; 957,817 passes | `loopFOC()` + `move()`, DWT counter, motor supply off, driver enabled, 20 kHz PWM | measured | `simplefoc_loop_cycles` row, `results/2026-11-03-dry/` |
| Loop pass, closed loop | 1.92k cycles mean (9.58 µs), 1.92k worst; 714,602 passes | same, Hall polling and SVPWM included, alignment skipped | measured | same |
| Speed ripple, current, spin | not run | bus 3.7 V: supply off | waiting | `motor_speed_ripple` rows (status waiting) |

TI's numbers on the same hardware, for the comparison to fill: 2.5 to 4.1 percent speed ripple at 20 Hz, peak current 0.48 to 0.58 A, control interrupt at most 5 µs (`am13e230/foc-drv8376/README.md`, `data/results/am13e230.csv`). The two loops are not the same work: TI's runs a current loop with the TMU, SimpleFOC's here is voltage mode.

## Ten-minute path

1. Build (about 5 s, bench-free): `ports/simplefoc/build.sh` (set `OUT` to a folder outside the repo; `FRL_DRY=1` builds the check that needs no motor supply).
2. Wire as for the six-step program (`am13e230/bldc-drv8376/README.md`, "Hardware"), 12 V on VBAT and PGND, USB-C last.
3. Back up the flash (`python3 tools/frl/frl.py backup --board am13e230 --probe <serial>`), then take the bench lock, then run: `python3 ports/simplefoc/tools/run_simplefoc.py <simplefoc.out> --serial <serial> --out <folder> --wait 70`.
4. It prints the analysis: speed ripple over the last 5 s of each 8 s hold, peak current, loop time, and the status of each run.

## If you see this

- `no bus voltage` and `vm_V` near 3.7: the motor supply is off (the floating sense input reads that); switch it on.
- Run 2 stops with `sensor align failed` or `no Hall edge`: SimpleFOC's alignment needs a free shaft; check the five Hall wires and that S6 and S4 are on BP.
- `overcurrent trip`: the sense gain is unverified; read `ipk_mA` before raising `TRIP_MA`.

## Where these facts come from

Checked 2026-11-03 on an Intel Mac, macOS 15.7.9: AM13E2x SDK 26.01.00.03.STS, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, DSLite from UniFlash 9.6.0, probe firmware 3.0.0.41, one Rev E2 LP-AM13E230. Files: `results/2026-11-03-dry/`, `results/2026-11-03-run1/`, `data/results/am13e230.csv`, experiment `docs/experiments/2026-11-03-am13e230-simplefoc.md`. Credit: SimpleFOC by Antun Skuric and contributors (MIT). Not affiliated with TI or the SimpleFOC project.
