# Dim an LED or move a servo

The LED dims up and down, or a hobby servo swings.

The page: [site/project-pwm-dim.html](../../site/project-pwm-dim.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Could work. TI's PWM examples run, and their output was not measured. A PWM program ported from ST's example did not match its duty: one channel read high in 88 of 600 debugger samples, 14.7% (plus or minus 2.8%) against 12.5% duty. One channel was measured, with no servo.
- **l1306**: Could work. TI's PWM examples run; the output was not measured.
- **l2228**: Could work. TI's timer dead-band PWM example runs: counter moving, compare values 1000 and 499 of 2000 and the pin muxed to the timer, read through the debugger; the output was not measured. Source: `data/results/mspm0l2228.csv`.
