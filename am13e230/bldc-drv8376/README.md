# LP-AM13E230 with the DRV8376EVM: spin a Hall-sensor brushless motor in six-step and sine modes

> **Current state, 2026-10-16:** Rotation was restored with a measured DAC current-limit reference and a new Hall table (commissioning revision 17): two unloaded runs of 133 and 279 consecutive forward Hall sectors, no skipped sectors and no driver fault. The speed table below was measured on 2026-10-15, before the repair, and has not been reproduced since. The firmware and steps that restored rotation are in the [FOC port README](../foc-drv8376/README.md).

Terms: [LP-AM13E230 FAQ](../FAQ.md). Six-step: the simplest way to run a three-phase motor, one pair of phases on at a time, switched by the three Hall sensors. Board page: [LP-AM13E230 README](../README.md). Experiment: [E196](../../docs/experiments/2026-10-15-am13e230-bldc-drv8376evm.md).

The DRV8376EVM is a 40-pin BoosterPack, so it plugs straight onto the LaunchPad. Its six PWM inputs land on the kit's motor PWM unit (MCPWM4) and its three Hall inputs on plain input pins. The chip is only the power stage: this program does the commutation. You type at a serial terminal: learn the Hall table once, then go, duty, direction, brake.

Status 2026-10-15, 22:50: it spins. 928 rpm at 20% duty, 2,160 at 40%, 3,566 at 60% forward; 2,047 and 3,326 in reverse; on a 12.4 V bus from a 12 V, 0.5 A adapter, unloaded, no faults. Learn, run, reverse, brake and fault clear all from the serial port. Rows in `data/results/am13e230.csv` (`bldc_rpm`). Motor: Anaheim Automation BLY171S-24V-4000 (label read 2026-10-15): 24 V, 4,000 rpm, 1.8 A rated, 1.5 ohm line to line, 2.45 V per 1,000 rpm, eight poles. Its calculated locked-rotor current at rated voltage is 16 A; a duty cap alone does not establish a safe phase-current limit.

## Build and flash

```bash
./build.sh --flash
```

Builds with TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 from `bldc.syscfg` and `main.c` (the same flags as `../blink.sh`), then flashes over the onboard XDS110. Build 3 s, flash 5 s, 32 kB of flash.

## Hardware (USB unplugged first)

1. On the EVM, hall-power jumper J13 to the GVDD position (5 V; the BLY17 Halls want 4.5 V or more). The five Hall wires: red HALL POWER, black HALL GROUND, blue HALL A, green HALL B, white HALL C. Phases yellow, red, black to OUT A, B, C (any order works; the program learns the table).
2. On the LaunchPad, switches S6 and S4 to BP, so PB6, PB7, PA11 and PA12 reach the headers instead of the CAN transceiver.
3. Plug the EVM onto J1 to J4 with the screw terminals pointing away from the USB-C connector. The Hall-block socket (EVM J8) covers J2/J4 on the right; the nFAULT-side socket (EVM J9) covers J1/J3, its pin 20 (3.3 V) on J1 pin 1.
4. Motor supply, 4.5 to 65 V DC, to VBAT (+) and PGND. 12 to 20 V is plenty for a first spin. The board is reverse-polarity protected: a swapped supply does nothing rather than something bad.
5. USB-C into the LaunchPad, then the motor supply on.

What lands where (DRV8376EVM user's guide SLVUD20, tables 2-1 and 2-2, against the kit's header silkscreen):

| EVM signal | Header pin | LaunchPad pin | Program |
|---|---|---|---|
| INHA, INLA | J4 40, 39 | PB3, PA7 | MCPWM4 1A, 1B |
| INHB, INLB | J4 38, 37 | PB1, PA2 | MCPWM4 2A, 2B |
| INHC, INLC | J4 36, 35 | PB2, PA25 | MCPWM4 3A, 3B |
| HALL A, B, C | J4 34, 33, 32 | PB7, PB6, PA12 | inputs (10 k pull-ups to 3.3 V on the EVM) |
| nSLEEP | J1 9 | PA23 | output, high = awake |
| nFAULT | J1 8 | PB4 | input, low = fault; EVM J9 pin 6 |
| Old fault route | J1 10 | PA22 | diagnostic only; R25 absent, reads high during faults |
| DRVOFF | J2 13 | PB18 | output, high = outputs off |
| MCU LED | J2 11 | PB11 | output, on when the program runs |
| VSENA, VSENVM | J3 26, 23 | PA17, PA16 | ADC: PWM-mode check, bus volts |

## Running it

Open the backchannel at 115200. `../tools/uart_cmd.py "<keys>" <seconds> --stop` sends keys 0.3 s apart, prints the answer, and coasts at the end (`uart_cmd.py "2g" 6 --stop`), or use a serial terminal and type. `--gap <seconds>` changes spacing between keys. The current image starts in 6x PWM encoding with drive disabled. PA4 is an ADC input and its DAC is disabled.

| Key | Does |
|---|---|
| `l` | Learn, about 25 s, hands free. Holds the rotor on the six phase axes at 20% duty twice round and reads the Hall state at the end of each hold (the axes sit in the middle of the Hall sectors; the six line vectors sit on the boundaries, where the reading flips). Then tries the twelve tables that fit that order, forward and reverse, 0.8 s each, scoring each by net sectors travelled, and keeps the best of each direction. Needed once per power-up. |
| `g` | Go: run at the set duty (15% if none set), ramping 1% every 20 ms |
| `0` to `9` | Duty 0 to 90%, capped at 60% in `MAX_DUTY` |
| `+` `-` | Duty up or down 5% |
| `f` `r` | Forward, reverse (stops first; `g` again) |
| `s` | Stop, outputs floating (coast) |
| `b` | Brake: all three low sides on |
| `a` | Trim the commutation advance by one step (0, +1, -1); 0 was best here |
| `c` | Full sleep/wake reset; checks fault and leaves outputs disabled |
| `F` | Read PA22, PB4 and DRVOFF pad status |
| `A` | Read PA4 ADC input with outputs disabled; needs a temporary lead from the ILIMIT test point TP17 to J9 pin 1 |
| `B` | Check PB4 with weak pull-down, pull-up and no pull, then restore its configuration |
| `v` `?` `q` `j` | Status, help, the last Hall states seen, Hall sector widths while running |
| `p` `u` | Speed mode on (digits 1 to 4 set 500 to 2000 rpm, `g` runs, `w` prints the rpm log of the last step, `k` `K` step Kp and Ki) and off (duty mode) |
| `m` `n` | Sine modulation on (needs a learned table) and six-step |
| `S` | Startup test: 20 starts from standstill at the set duty, time to one turn |
| `O` `H` `R` `V` | Open-loop start with no sensor: align, ramp to the target (H, 25 Hz steps) over the ramp time (R, 500 ms steps) with V/f slope (V, 10 percent per 100 Hz steps), then hand over to the Halls |
| `i` | Phase currents: the rest level when stopped (should be VREF/2), mean and rms per phase for one second when running |
| `d` `x` `y` `z` `M` `T` `h` `i` `Q` `W` | Diagnostics: mode check, duty read-back, timing read-back, swapped-order check, toggle the 6x/3x encoding, load the built-in table, hold one line vector, read rest/mean/rms current, A-high with and without a return path, sub-blanking pulse bursts |

While running it prints a line a second: duty, direction, Hall state, rpm (mechanical, four pole pairs, from the last six Hall edges), bus volts (through the EVM's 143 k over 6.04 k divider), fault. Live values over the probe: `../tools/run_image.py bldc.out 0 g_rpm:4 g_duty:4 g_hall:4 g_fault:4 --no-flash`.

## Extra commands

`C` cross-checks the auxiliary ADC slot using ISENA internally with drive disabled; `E` dumps a timestamped speed trace after stopping. Status reports speed or duty mode, target rpm and PI gains. The six-step and sine duty sweeps and the PI speed-step comparisons in both directions have not run since the repair.

## What it measured (2026-10-15)

Duty sweep, `sweep.sh`: three 4.5 s runs from standstill at each point, median shown, low and high in `data/results/am13e230.csv` (`bldc_rpm`). Unloaded, 20 kHz PWM, bus 12.4 to 12.5 V from a 12 V 0.5 A adapter, which never sagged: an unloaded motor needs little current once it turns. Speed from Hall edge timing with a majority-vote filter, not a tachometer.

| Duty | Forward rpm | Reverse rpm |
|---|---|---|
| 10% | 405 | 376 |
| 20% | 924 | 891 |
| 30% | 1,561 | 1,446 |
| 40% | 2,312 | 2,091 |
| 50% | 3,067 | 2,728 |
| 60% | 3,632 | 3,442 |

Starts from standstill at 10% in both directions, every run. Reverse runs 5 to 10 percent slower than forward at the same duty: the reverse table is a separate pick from the twelve, and the Hall edges sit at different angles each way (TI's SDK keeps separate forward and reverse Hall tables for the same reason). Speed per volt at 60%: 7.4 V average gives 3,632 rpm, against 3,000 from the 2.45 V per 1,000 rpm constant if that figure is line-to-line rms; the label does not say which convention it uses, and a tachometer would settle whether the Hall-timed rpm reads high.

**Hall sector widths** (`j` while running, 50 electrical turns, three captures): states 1 to 6 take 14.9, 18.4, 16.9, 16.5, 18.4 and 14.7 percent of a turn forward and 15.0, 18.3, 16.6, 16.6, 18.3, 14.8 in reverse, against 16.7 ideal. The same pattern both ways means the sensors sit about 7 electrical degrees off, not that the drive leads or lags. Rows `hall_misalign_pct`.

## Schemes tried on this motor (2026-10-15 night), and what each is good and bad at

| Scheme | Command | Starts from standstill | Speed range here | Good | Bad |
|---|---|---|---|---|---|
| Six-step from the Halls | `g` | 20 of 20 at 10% and at 20% (one turn in about 300 ms); 0 of 20 at 5% | 405 to 3,632 rpm at 10 to 60% | Starts every time, any load the current allows, no tuning beyond the Hall table, 350 lines | Torque ripple (six steps a turn), needs the three Hall wires and a learned table, speed is open loop in duty |
| Open loop, no sensor, fixed duty | `O` (`H` target, `R` ramp time) | Follows the ramp to 750 rpm at 20% and 30%; loses the rotor at 15% | 750 rpm; lost at 1,500 rpm | No sensor at all, the rotor just follows the rotating field | Blind: a stall is invisible without the Halls; fixed duty cannot climb with back-EMF |
| Sine modulation on the Halls | `m`, then `g` or the speed loop | 20 of 20 at 10% (378 ms to one turn) and at 20% (335 ms), a little slower than six-step | 531 rpm at 20% amplitude, 894 at 30%; 1,330 at the 40% cap | Smooth: the three half bridges carry 50% plus a sine at the rotor angle interpolated from the last Hall edge and the speed, so the field turns continuously instead of jumping 60 degrees; reverse is symmetric (892 against 894 rpm at 30%) where six-step loses 5 to 10 percent | Needs a third more modulation than six-step for the same speed (line voltage is 0.87 of six-step's at the same number), and the angle between Hall edges is a guess from the last speed, so a fast speed change lags |
| Speed loop (PI on the Hall rpm) over either | `p`, `1` to `4`, `g` | 0 to 1,000 rpm in 490 ms (10 to 90), no real overshoot | holds 500 to 1,500 rpm within about 3% | Two gains, tuned in two tries: Kp 1 percent per 100 rpm, Ki 1 | Slow-down is a coast (no braking); the speed it sees is quantized by the Hall edges, so fine ripple cannot be read this way |
| Open loop with V/f, then hand over to the Halls | `V` slope, then `O` | Followed to 1,500 rpm (base 15% plus 20% per 100 Hz) and to 1,875 rpm, then ran on the Halls at 1,865 and 2,252 rpm | lost at 2,625 rpm target over 3 s, 7 of 7 | This is the start every sensorless scheme uses: align, ramp, then switch to the observer (here, the Halls stand in for it) | The ramp rate and the V/f slope are tuned by hand; too fast or too high a target and the rotor drops out with no warning |

The Halls are the judge in every row: the program counts their edges even when it is not using them. Rows `motor_start_ok`, `motor_start_ms`, `motor_ol_follow`.

**Equal speed, unloaded** (speed loop, two runs each; rows `motor_mod_at_speed`): six-step holds 1,000 rpm at 22% duty, sine at 33% amplitude; six-step holds 1,500 at 29%, sine cannot reach it under the 40% cap (1,330 rpm). Speed ripple from the Hall-timed rpm read 5 to 41 percent run to run for both methods, which is the estimator and the Hall misalignment, not the shaft; those rows are `waiting` on a tachometer. Step responses on six-step (rows `motor_step_*`): 0 to 1,000 rpm rise 490 ms, 1,000 to 1,500 rise 410 ms, 1,500 to 500 is a 470 ms coast.

**Phase current** (`i`): on the 2026-10-16 early run the three amplifier outputs read 18 to 23 mV at rest because J2 was absent. J2 is now fitted and the rest outputs are near VREF/2, but the board has not returned to continuous rotation after the phase leads were fixed. Current and no-load power at speed remain waiting (rows `motor_noload_w`). The assumed 1 V/A gain still needs checking against the EVM variant resistors.

## What went wrong first (so you can skip it)

1. **Hall power.** With the J13 shunt missing or the Hall ground open, every Hall line reads high (state 7). The fix is the jumper and the screws.
2. **Hall lines flicker on a boundary.** Holding a line vector parks the rotor exactly on a Hall edge, so the state flips with the slightest vibration: hundreds of changes a second, and a learn that reads the same state twice. The learn now parks on the phase axes (sector centres) and the Hall read is a 24-sample majority over 72 µs, longer than one PWM period.
3. **Phase-voltage sensing did not reliably identify PWM mode.** Earlier automatic checks were inconsistent. The current image therefore boots in fixed 6x mode with drive disabled.
4. **Reverse from the forward table.** Using the opposite vector of the forward table gave half the reverse speed; the learn now scores reverse tables separately.

5. **A stall after fitting J2.** A loose phase lead and an apparently floating ILIMIT connection both stopped the motor. A measured DAC reference on the ILIMIT input restored drive, and a new Hall table (found with a slow open-loop trial) restored forward rotation. Check the OUTA, OUTB and OUTC screw terminals and the J2 and J4 jumpers before suspecting the commutation table.

## First test (written 2026-10-15)

This sequence is from before the repair. Run it unloaded, with the shaft free.

1. Supply on, nothing typed: the status LEDs VM, AVDD, GVDD on the EVM light, NFAULT stays off, the mode line says 6x or 3x.
2. `v`: bus volts near the supply; Hall state 1 to 6 (0 or 7 means a Hall wire is off).
3. Turn the shaft by hand a little and press `v` again: the Hall state changes.
4. `l`: the shaft clicks round six steps; the table prints with six different Hall states.
5. `2` then `g`: the motor turns. Watch the rpm line.
6. `s`, `r`, `g`: the other way. `b` to feel the brake. `0` and `s` to end.

## If you see

- `no bus voltage on VM`: the supply is off or on the wrong terminals.
- `phase A never went high`: NFAULT LED on (press `c`), or the EVM is not seated.
- `learn failed`: a Hall wire is loose (state 0 or 7), or the rotor did not move at 6%: raise `LEARN_DUTY` or the supply voltage.
- Runs rough or stalls at once: wrong lead for this wiring, press `a`, then `g` again.
- `no Hall edge for 1.5 s`: shaft held, duty too low to start, or the table is wrong; learn again.
- `DRIVER FAULT`: overcurrent or undervoltage; lower the duty, check the supply can give the current, `c` to clear.
- Spins one day, dead the next with no fault: check the OUTA/OUTB/OUTC terminal screws and J4/J6 current-limit jumpers. The second cause was a floating current-limit reference, fixed with a measured DAC reference.
