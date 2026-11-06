# LP-AM13E230 with the kit DC motor through an L293D: ramp forward and reverse

Board page: [LP-AM13E230 README](../README.md).

The board's motor PWM unit (MCPWM4) drives the two inputs of an L293D H-bridge at 20 kHz. The program ramps the motor up forward, holds, ramps down, stops for a second, then does the same in reverse, forever. LED4 is on while the motor is driven. No soldering: the L293D from the Freenove kit sits in the breadboard.

**Should work:** built and flashed once; the L293D is not wired to a motor. The kit's stepper board was the one used (`../stepper-uln2003/`, experiment E145). This program is ready for the DC motor when the L293D goes in the breadboard.

## Build and flash

```bash
./build.sh --flash
```

Builds with TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 from `motor.syscfg` and `main.c` (the same flags as `../blink.sh`), then flashes over the onboard XDS110. Build 4 s, flash 4 s, 10 kB of flash.

## Wiring (USB unplugged first)

| From | To | Why |
|---|---|---|
| J3 pin 21 (5V) | breadboard + rail; L293D pins 16, 8, 1 | Logic supply, motor supply, enable for the first bridge |
| J3 pin 22 (GND) | breadboard - rail; L293D pins 4, 5, 12, 13 | Ground |
| J4 pin 40 (PB3, MCPWM4_1A) | L293D pin 2 (IN1) | First bridge input |
| J4 pin 38 (PB1, MCPWM4_2A) | L293D pin 7 (IN2) | Second bridge input |
| L293D pin 3 (OUT1), pin 6 (OUT2) | the motor's two wires | Either way round; the program drives both directions |

L293D pin 1 is beside the notch. Pins 1 to 8 run down the notch side, 9 to 16 up the other side. The L293D logic high is 2.3 V, so the board's 3.3 V signals work. The motor sees about 3.5 V (5 V minus the chip's drop), which is fine for the kit's 3 to 6 V motor.

## How the program drives the pins

Forward: IN2 forced low (software force on the action qualifier), IN1 carries the duty. Reverse: the other way. Stop: both forced low (coast). Duty is `CMPA = percent x 10,000 / 100`; the counter counts up from 0 to 9,999 at 200 MHz. Live values readable over the probe with `../tools/run_image.py motor.out 0 g_cycles:4 g_phase:4 g_pct:4 --no-flash`.

## If you see

- Motor never turns, LED4 cycles: check the L293D is powered from 5 V, not 3.3 V (pin 16 needs 4.5 V), and that pin 1 (enable) is high.
- Motor turns one way only: one of IN1 or IN2 is not wired, or two wires are on the wrong L293D side.
- Motor twitches but does not start: the kit motor needs about 40 percent before it moves; it ramps through that in about one second.
