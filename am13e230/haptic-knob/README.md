# Haptic smart knob (LP-AM13E230 + DRV8376EVM + Anaheim BLY17)

Status: staged, never run. The image builds and passes `tools/preflight_image.py` (SAFE); the detent arithmetic is host-tested (`foc-drv8376/tests/test_haptic_core.c`, 6 detent counts, 10 strengths, 401 positions). The motor has not been energised for this mode.

## What it does and the honest limit

The BLY17's Hall sensors give 24 positions a turn (15 degrees each). There is no usable position inside a Hall step and none at standstill finer than that. So the knob is a coarse spring: detents every 24 / N steps (N = 2, 3, 4, 6, 8 or 12), no torque at a detent, more torque the farther out, capped. At 12 detents (two Hall steps apart) it feels like clicks, not a smooth notch. N = 24 cannot work (no error to push against).

Cheapest fix for a smooth feel: a magnetic angle sensor (AS5600, 12-bit, I2C) with a diametric magnet on the shaft end, or a small quadrature encoder module on the LaunchPad kit's encoder header. That needs a way to mount a magnet on the motor shaft and a new reader in the firmware; not staged.

## Safety limits (all enforced on the MCU)

- Starts only from key `H` in the guarded commissioning image, which boots with DRVOFF high and all PWM low. Preflight needs the measured current-limit reference 1670 to 1730 mV, bus 9 to 15 V, sense channels at idle, no fault.
- Torque duty 5 to 14 (the six-step runs allow 15). Pulses over 250 mV sensed are skipped; over 400 mV aborts; every hardware trip stays live.
- The run ends by itself at 20 s. `s` stops sooner. Invalid Hall state, three skipped transitions, nFAULT or a trip also end it. No automatic retry.
- No flood tests: the page sends a setting only after you stop moving a control (250 ms) and never repeats.
- Hands, hair and cables clear of the shaft; 12 V plug within reach; stay at the bench.

## Steps

1. Wiring is the existing six-step wiring (docs/arrivals/img/am13e230-bldc-drv8376.png and am13e230/bldc-drv8376/README.md, "Hardware"). Do not change it.
2. `am13e230/haptic-knob/run.sh` builds, runs the preflight, flashes the commissioning image and serves the page.
3. In Chrome open http://localhost:8000/, connect the LaunchPad serial port (the `...AL1300011` one), tick both boxes, press Start.
4. Turn the shaft by hand. Move Detents and Strength; read the angle on the dial.
5. Headless alternative: `python3 am13e230/haptic-knob/knob_cli.py --detents 12 --strength 5` (log in `last-run.log`).

Expected: clicks about every 30 degrees at 12 detents, the dial following the shaft, `haptic done reason=0` at 20 s (reason 8 means you pressed stop). Anything else: read `reason` in `foc-drv8376/COMMISSIONING.md`, leave the 12 V off, and do not repeat blindly.

Untested and likely to need tuning: the spring may buzz or hunt at high strength; try strength 3 first.
