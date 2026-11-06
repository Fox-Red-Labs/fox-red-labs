# Portable motor-control core for M0 to M33 with an NPU: design

A lean C core for motors that builds for any chip with a PWM timer and an ADC, with a cost row per feature and chip. Status: design, 2026-10-19. The first slice of `core/` (number format, configuration, kernel, Hall angle with the edge tracker, guard, cost stamps) and a host test exist; see `README.md`. Boards, the hardware layer and the profiles are not ported. The reference is the LP-AM13E230 with the DRV8376EVM and a BLY171S on a 20 V pack, running a guarded Hall FOC image (`am13e230/foc-drv8376/`).

Terms: [docs/vocab.md](../../docs/vocab.md). Contents: 1 What it is, 2 Layers, 3 Hardware layer, 4 Number formats, 5 Profiles and tiers, 6 Guard, 7 Host protocol, 8 Control chain, 9 NPU layers, 10 Cost matrix, 11 Porting order, 12 Checks.

## 1. What it is

A lean C core for three-phase and two-phase motors that builds for any chip with a PWM timer and an ADC. One code base covers every motor market by configuration: appliance pumps and drums, compressors, fans and blowers, vacuums, blenders, trolling motors and thrusters, toys, steppers, servos and actuators, power tools. A build picks a **profile** (what the application needs) and a **tier** (what the chip can carry). Every feature has a measured cost row per chip: flash, RAM, control-ISR cycles (mean and worst) and the highest PWM rate it holds.

What it is not: a motor modelling tool (engineering predictions run on a trusted solver: OpenModelica for the machine and drive, vendor SPICE models for the power stage; the bench validates both), a GUI, or a wrapper around a vendor's motor library. Vendor driver libraries sit below the hardware layer only.

## 2. Layers

```
portability/motor/
  core/                    plain C99, no vendor include, host-tested
    frl_num.h              number format: frl_real, FRL_MUL, FRL_SINCOS, FRL_SQRT, FRL_DIV (one switch per build)
    frl_cfg.h              FRL_TIER, FRL_PROFILE, FRL_F_* flags, checked combinations
    kernel.h               clarke, park, ipark, PI (conditional integration), svm/min-max, ramp; static inline
    sense.h                offsets, scaling, three-shunt reconstruction, sample-window rules, saturation flags
    angle_hall.h           Hall decode, table (6 or 24 entries), edge tracker, direction from edge order
    angle_obs.h            back-EMF observers (each its own block), PLL, hand-over
    sixstep.h              Hall and back-EMF commutation, brushed H-bridge, stepper microstep
    loops.h                current loop, speed loop (gain schedule, feedforward), position loop
    start.h                align, I-f ramp, Hall start, flying start, initial position detection
    stop.h                 coast, bounded brake, ramp-down limited by bus voltage
    ident.h                staged identification at idle: R, L, flux, J and friction, Hall table
    guard.h / guard.c      one safety concept: refusals, trips, caps, watchdogs (section 6)
    table.h                one published variable table, version and layout hash (section 7)
    cost.h                 FRL_COST_BEGIN/END: cycle stamps, mean and max, no division in the ISR
    npu.h                  asynchronous model hook for the slow loops (section 9)
  boards/<id>/
    hal.h                  static inline verbs (section 3), pins, ADC and PWM setup notes
    board.syscfg | .ioc    vendor configuration as a text file
    build.sh               one command, flags per feature set
  tests/
    host_test.c            core against a float reference and a simple plant, both number formats
    cost/                  flag-on and flag-off builds per board, map diffs
```

Rules, from `docs/repo-layout.md` and `portability/driver-layering.md`: the core never includes a vendor header; a board folder holds only what that board changes; configuration stays in the vendor tool's text file.

## 3. The hardware layer

Static inline functions in `boards/<id>/hal.h`, so the cost matrix measures the chip, not a call layer. The verbs:

| Verb | What it does |
|---|---|
| `hal_pwm_set(a, b, c)` | three duty values in timer counts, shadow-loaded at the period |
| `hal_pwm_off()` / `hal_pwm_low()` | all switches off (coast) / low sides on (brake), one write |
| `hal_pwm_period(n)` | PWM period in counts |
| `hal_adc_trigger_at(n)` | sample point in the period |
| `hal_isen(i[3])` / `hal_vsen(v[4])` | raw phase-current counts / phase and bus voltage counts |
| `hal_drv_enable(bool)` / `hal_drv_fault()` | gate driver on or off / its fault pin |
| `hal_trip_arm(lo, hi)` / `hal_trip_status()` | hardware over-current window (comparators or a timer fault input), both sides where the chip allows |
| `hal_ilimit_set(code)` / `hal_ilimit_read()` | driver current-limit reference and its read-back, where the board has one |
| `hal_hall_read()` / `hal_hall_capture()` | three Hall bits / last edge timestamp in timer counts |
| `hal_cycles()` | free-running cycle counter (DWT on M33/M4, a 16-bit timer on M0+) |
| `hal_link_*` | host transport (UART, USB CDC, CAN) |

The AM13 Hall program touches hardware in 89 of 799 lines (11 percent), so the split is cheap.

## 4. Number formats

`frl_real` is `float` on a core with an FPU or a trig unit, and Q15 or IQ24 (`int32_t`) elsewhere, set once per build. Angles are per-unit turns everywhere (0 to 1), so integer builds wrap for free and sine needs no multiply by 2 pi. `FRL_SINCOS` expands to the trig unit, a math accelerator, a 256-entry interpolated table or libm; those four choices are why the same kernel measures 117, 531, 918 and 1.9k cycles across the boards on hand (`data/results/*.csv`, test `foc_step`). Float on a core without an FPU is not offered: 6.4k to 14k cycles for the same step. The host test runs both formats against a float reference and fails on a stated error bound.

## 5. Profiles and tiers

A **profile** is an application's needs; a **tier** is the control the chip carries. A build is a profile at a tier, and `frl_cfg.h` refuses combinations that cannot work (position control without a position sensor, sensorless start under load without a start path).

### Profiles

| # | Profile | Markets | Core features |
|---|---|---|---|
| 1 | Six-step sensorless, one direction | small fans and pumps, drain pumps, PC fans | back-EMF commutation, align and ramp start, speed PI near 1 kHz, lock detect, tach out, PWM or analog command |
| 2 | Sensored, both directions, fast start | power tools, e-bikes, doors and gates, thrusters, robot wheels; 2b brushed H-bridge (toys, openers, windows) | Hall decode with fault check, direction from the rotor, current limit, stall rule, bounded brake, trigger map |
| 3 | Quiet sine or FOC, medium speed | ceiling fans, hoods, dishwasher and pool pumps, medical blowers | observer, I-f start, d/q current loops, 16 to 25 kHz PWM, dry-run from power, outer pressure or flow loop |
| 4 | High-speed sensorless | vacuum and blower fans | high electrical frequency, 40 kHz class PWM, field weakening, start under 200 ms, blocked-outlet protection |
| 5 | Fast-response ESC | drones, RC | digital throttle with telemetry, 24 to 128 kHz PWM, demagnetization handling, active braking |
| 6 | Heavy variable-load FOC | washer drums, compressors, large pumps | sensorless from low speed, start under load, field weakening, MTPA for interior magnets, cyclic-load compensation, soft stop, load and unbalance estimates |
| 7 | Position | servos, actuators, seats, windows | cascaded position, speed and current loops; encoder, potentiometer or ripple counting; anti-pinch reverse |
| 8 | Stepper | printers, CNC | microstepping, current scaling, stall detect in its valid speed window, homing |

### Tiers

| Tier | Control | Smallest chip class on hand | Runs where |
|---|---|---|---|
| T0 | open loop: stepper microstep, brushed duty, V/f | M0+ 24 MHz, no complementary PWM | every board |
| T1 | six-step: Hall, then back-EMF | M0+ 32 MHz | every board with PWM |
| T2 | sine Hall FOC, fixed point or float | M0+ 80 MHz with a math accelerator (10 to 15 kHz), M33 (20 kHz) | AM13, M33C321A, F2800137, AM263P, MCXA153 (Q15), Pico 2; G3519/G3507 at 10 to 15 kHz |
| T3 | sensorless FOC with identification | M33 with FPU | AM13, M33C321A, F2800137, AM263P; G3519 at 10 to 15 kHz |
| T4 | field weakening, MTPA, advanced starts (at the speed-loop rate) | whatever carries T2 or T3 | same as T2/T3 |
| T5 | learned layers on an NPU beside the slow loops | M0+ or M33 with an NPU | AM13, G5187 |

Tiers add code, not rate: a drive can run T2 at 20 kHz and its T4 tasks at 2 kHz. A feature that misses its budget on a chip gets a `does_not_fit` result row, never a silent degraded build.

### Feature flags

`FRL_F_GUARD, FRL_F_COST, FRL_F_SIXSTEP_HALL, FRL_F_SIXSTEP_BEMF, FRL_F_BRUSHED, FRL_F_STEPPER, FRL_F_FOC, FRL_F_HALL_TRACK, FRL_F_HALL_TABLE24, FRL_F_OBS_SMO, FRL_F_OBS_FLUX, FRL_F_IDENT, FRL_F_FF_SPEED, FRL_F_GAIN_SCHED, FRL_F_FW, FRL_F_MTPA, FRL_F_IPD, FRL_F_FLYING, FRL_F_BRAKE, FRL_F_DTC, FRL_F_CAPTURE, FRL_F_TABLE, FRL_F_NPU`. `FRL_TIER` sets a default group; a profile adds or removes flags. Every flag is `#if`, never a runtime branch in the ISR, and removes code and table entries along with it.

### The control ISR

Generated from the flags as one straight function, blocks in a fixed order: sample, guard checks, estimate angle, transform, regulate, modulate, write, capture, cost stamp. No function pointers in the ISR. Slow work (speed loop at 1 to 2 kHz, field weakening, identification steps, host protocol, model calls) runs in a background tick. Hot data lives in one struct in the chip's fastest RAM.

## 6. One safety guard

The concept proven on the AM13 bench (`am13e230/foc-drv8376/frl_guard.h`), made portable:

- **Refusals at a start:** a driver fault, the bus outside its window, an uncalibrated angle source, standstill phase currents off zero, a configuration the host may not run.
- **Stops during a run:** run cap, host silence (a changing heartbeat, not any byte), driver fault, phase current over the latched limit (driver off from inside the ISR), bus excursion, background stall (N ISRs without a tick), clock disagreement (cycle counter against ISR count), non-finite values, a stall rule, I²t.
- **Limits:** every host-writable limit clamped on write to a ceiling the firmware owns, latched at a run start; out-of-range writes revert.
- **Hardware layers, fastest first:** the driver's own limits (fixed over-current, thermal, cycle-by-cycle current limit at a verified reference), comparator window trips on both sides of zero on every phase the chip allows, then the ISR check. The ISR trip ceiling sits inside the current sense's linear range, so a clipped reading still trips.
- **Fault reaction table:** per fault, coast or low-side brake, latch or retry, and the reason code the host sees.
- **Per board:** where the trip hardware is and how one write turns the driver off. A board with no comparator path (L1306) records the gap; it does not pretend.

The guard is itself a cost row, measured with the flag on and off before any other feature.

## 7. One host protocol

One published variable table, generated from a single variable list where each entry names the flag that owns it, so a build without an observer has no observer entries and its host map is derived, not edited. A version number at a fixed index and a layout hash; a host refuses writes unless its map matches the board (the rule the motor link tool enforces today). Transport is a board file. The same table feeds the Motor Lab console, the capture readout and the batch tools; profiles add entries, never rename them.

## 8. The control chain, feature by feature

| Stage | Platform behavior | Measured on the AM13 bench |
|---|---|---|
| Signal chain and current sense | offsets at boot with the driver off; refusal when standstill currents drift; saturation flag at the rail; sample window sized from the driver's straps, not the slowest default | a 7 µs default window caps duty at 28 to 72 percent (about 2,470 rpm at 19.3 V instead of the bus limit) |
| PWM and ADC timing | center-aligned PWM, sample in the low-side window, current loop at the PWM rate or every second period | 20 kHz, 6.7 µs worst Hall FOC ISR on the AM13 |
| Current loop | PI with conditional integration, gains from identified R and L (Kp = ωc·L, Ki = ωc·R), back-EMF and cross-coupling feedforward, dead-time compensation after the step is measured | the reference firmware's Ki puts the zero at R/4L, a slow current tail |
| Speed loop | bandwidth scheduled with the Hall edge rate, acceleration and friction feedforward, integral zero near fc/5, Iq slew limit | a fixed 60 Hz crossover on a 120 Hz edge rate limit-cycled; Kp 0.025 halved torque noise |
| Angle and speed | Hall: calibrated table, edge tracker with sign from edge order, speed advance for debounce and PWM delay; model-based prediction between edges from Iq·Kt/J; observers for sensorless | the calibrated table's pole-pair spread is 4 degrees, so a 24-entry table is not the lever on this motor |
| Direction | from the rotor (edge order or observer sign) while turning; a zero command keeps the last direction | the reference firmware took direction from the command sign; a ramp stop to 0 flipped it at once |
| Identification | staged at idle: R from DC holds at two currents, L from a voltage step through the 20 kHz capture, flux from a coast or a spin, J and friction from a known-torque acceleration and a coast; Hall table and offset; gains derived | Ke measured 3.44 V/krpm line-to-line peak; the firmware's flux constant was 23 percent high |
| Startup | Hall start from the standstill sector; align and I-f ramp for sensorless; flying start from edge timestamps; initial position detection only where saliency is measured | Hall start reached 300 rpm in 0.06 to 0.16 s, 23 of 23 |
| Braking and bus | separate motoring and regenerating current limits; regenerating Iq limited as the bus nears the window top; bounded brake state | negative Iq is unlimited in the reference firmware |
| Protection | section 6 | trip at 1.5 A was above the 1.25 A sense range |
| Field weakening | voltage-magnitude loop lowering Id above base speed, at the speed-loop rate | limited on this driver by the 1.25 A sense range (about 30 percent more speed at most) |
| NPU layers | section 9 | smallest NPU call 88.7 µs, above the 50 µs control period |

## 9. NPU layers

Never inside the current loop. A model runs asynchronously beside the speed loop (an estimate the loop reads when ready, models up to about 2k parameters at 2 kHz) or in a supervisor at 1 to 100 Hz: load and unbalance classification, stall and dry-run, bearing health, virtual temperature. The CPU front end (windowing, FFT) is counted with the model. Each model is compared with a formula baseline (a band-energy threshold, Kt·Iq, a fitted thermal network) and kept only if it wins. The first measurement is the control ISR's worst time and jitter with the smallest model looping beside it, motor off.

## 10. The cost matrix

Rows are features, columns are boards; each cell holds flash bytes, RAM bytes, ISR cycles added (mean, worst), and the board's highest PWM-synchronous rate for the tier.

- **Flash and RAM:** the same board built with the flag off and on, sections diffed from the map (the map-size script in `tools/results/`); harness and stack excluded.
- **Cycles:** cycle stamps at ISR entry and exit (DWT on M33/M4, a free-running timer on M0+, the CPU timer on C28x, the PMU on R5F), interrupt entry excluded and said so; a spare pin toggled for a scope.
- **Rate:** the `fom.h` sweep with the real ISR as the body, then in situ with the motor turning where a motor is fitted.
- Expectation written before every run; three runs; rows in `data/results/<board>.csv`. A cell that needs gear not on hand is a `waiting` row with its reason.

Fill order: host build of the core (both formats) and map sizes for every board's toolchain; the AM13 with the motor off (shell, guard, capture, NPU hook); then the M33C321A and G3519 with synthetic inputs; F2800137 and MCXA153; the G5187 and L1306 as floor checks.

## 11. Porting order

1. Core host test passes in float and fixed point against the reference.
2. AM13: the core's T2 Hall FOC replaces the reference firmware's control path behind the same guard and table; ABBA against the guarded image on the same runs (start, steps, stop, scatter).
3. M33C321A and G3519 on the same EVM where the header pins allow.
4. Profiles 1, 2 and 8 on the smallest boards (six-step and stepper need no FOC).
5. Second motor (QIWO 57BL) through identification only, gains derived, no hand tuning.

## 12. How the platform is checked

Predictions before every bench run, with a one-line physics derivation and a naive baseline; unattended batches with the guard bounding every run; results folders and one index line per results folder. Plant predictions are written so they can be rerun on the OpenModelica model when it lands. Supplier documents are cited for facts only; techniques are named by what they do.
