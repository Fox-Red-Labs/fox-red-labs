# LP-AM13E230 with the kit stepper through its ULN2003 board: one turn each way

Terms: see the [LP-AM13E230 FAQ](../FAQ.md). Half-step: one move in an 8-state coil sequence. ULN2003: a chip of seven switches on the driver board that carries the motor current, so the LaunchPad pins only send signals. Experiment write-up: [E145](../../docs/experiments/2026-10-09-am13e230-stepper-uln2003.md). Board page: [LP-AM13E230 README](../README.md).

The Freenove kit's 28BYJ-48 stepper on its ULN2003 driver board, driven by four plain output pins. The program turns the shaft one full turn, pauses a second, turns it back, forever. LED4 on the LaunchPad and the four LEDs on the driver board show it stepping. No soldering, no breadboard: six jumper wires.

Done 2026-10-09, shaft turning on the first flash (experiment E145). The turn count against the 4,096 figure was not measured.

## Build and flash

```bash
./build.sh --flash
```

Builds with TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 from `stepper.syscfg` and `main.c` (the same flags as `../blink.sh`), then flashes over the onboard XDS110. Build 3 s, flash 4 s, 8 kB of flash.

## Wiring (USB unplugged first)

| LaunchPad | ULN2003 board |
|---|---|
| J3 pin 21 (5V, top of the inner left row) | the pin marked + next to the jumper (silkscreen 5-12V) |
| J3 pin 22 (GND, just below it) | the pin marked - beside it |
| J4 pin 40 (PB3, top of the outer right row) | IN1 |
| J4 pin 38 (PB1) | IN2 |
| J4 pin 36 (PB2) | IN3 |
| J4 pin 34 (PB7) | IN4 |

The motor's white plug stays in its socket; the small jumper on the driver board stays on (it feeds the board's LEDs). IN5 to IN7 are unused. All four signal pins are on port B so one register write moves them together.

## How the program drives it

Half-step sequence, 8 states (one coil, two coils, next coil, and so on), 1.2 ms a step. 4,096 half-steps is one turn of the output shaft (64-step motor, about 1:64 gearbox). After each turn all coils go off, so the motor does not hold or heat between turns. Live values over the probe: `../tools/run_image.py stepper.out 0 g_turns:4 g_dir:4 g_step:4 --no-flash`.

## If you see

- Nothing moves, LED4 cycles and the driver LEDs chase: **Setup or handling.** The + and - power wires are off or swapped; the board needs 5 V on +.
- Buzzes but does not turn: **Setup or handling.** Two of IN1 to IN4 are swapped, or the step is too fast (suspected, not tested here); try 2,000 µs in `STEP_US`.
- Turns one way only: **Setup or handling.** The program reverses on its own, so suspect a loose IN4 (J4 pin 34), the last one wired (suspected, not tested here).

If it turns, try the DC motor version in `../motor-l293d/`.

## Microstepping (`microstep/`, experiment E146)

Same six wires. The four coils get sinusoidal duties from the PWM units (MCPWM4 outputs 1A, 2A, 3A and MCPWM1 output 2A on PB7) at 20 kHz, recomputed in a 20 kHz interrupt. Three builds: `./build.sh tmu --flash` (sine from the chip's trig unit through `fastrts_tmu.h`), `./build.sh sw` (C library sine), `./build.sh noisr` (interrupt off, for the main-loop baseline). Measured 2026-10-09: the handler takes 138 cycles with the TMU and 246 with the C library, 1.4 and 2.5 percent of the CPU at 20 kHz. Results are read from the `m` struct in RAM: `../tools/run_image.py microstep-tmu.out 13 m:40` (magic, best, worst, count, sum, loops, turns, phase, theta). Not watched by a person: whether the shaft turns smoothly under this drive through the ULN2003.
