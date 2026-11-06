# Build check: LP-MSPM0G3519 board series for Zephyr

Terms: [docs/vocab.md](../../../../docs/vocab.md). 1 kB = 1,024 bytes. Built 2026-10-25 on an Intel Mac (macOS 15.7), compile only. Nothing in this file ran on a board.

Zephyr `main` at `bc5d59d1b` (`v4.5.0-rc3-10`) with the three patches in `patches/` applied; `hal_ti` `53c80140`, GCC 12.2.0 (Zephyr SDK 0.17.4, cross-compile variant). Same CMake form as `../flash-driver/BUILD.md`, with the app directory changed:

```text
cmake -S $ZEPHYR_BASE/samples/basic/blinky -B out -GNinja -DBOARD=lp_mspm0g3519 -DZEPHYR_MODULES="$MODS" && ninja -C out
```

In a normal workspace: `west build -b lp_mspm0g3519 samples/basic/blinky` (and the other two).

| Target | Flash | RAM | Warnings |
|---|---|---|---|
| `samples/basic/blinky` | 12.3 kB (4.80 % of 256 kB) | 4.0 kB | 0 |
| `samples/hello_world` | 10.7 kB (4.17 %) | 3.9 kB | 0 |
| `samples/drivers/adc/adc_dt` | 15.2 kB (5.95 %) | 4.1 kB | 0 |

Checked in the generated `zephyr.dts` of the `adc_dt` and `blinky` builds: `adc0` is `okay` with `ti,clk-range = <6>`, `ti,clk-divider = <8>`, `max-result-reg = <12>`, `pinctrl-0 = <&analog_pa25>`; `canfd0_cantx_pa12` and `canfd0_canrx_pa13` carry function 12 (pinmux `0x21000c`, `0x22000c`); `CONFIG_ADC_MSPM0_ADC12=y` in the `adc_dt` build.

## With the I2C pull request applied

`optional-after-117311/` holds the one commit that adds the `i2c2` node. It was checked on top of this series plus the four commits of #117311 (`../../patches/0001-i2c-mspm0-pr117311.patch`, which needs `git am -3`: its board-file commit conflicts with this series' `&adc0` block and takes a one-hunk merge). `samples/basic/blinky` for `lp_mspm0g3519` builds with `i2c2` at `0x400f4000`, `okay`, 12.3 kB flash, 0 warnings. Blinky does not enable `CONFIG_I2C`, so this checks the devicetree, not the driver. No I2C traffic was run.

## Style check

`scripts/checkpatch.pl --no-tree` on each of the three patches: 0 warnings, and only the deliberate `MISSING_SIGN_OFF` error. `scripts/ci/check_compliance.py` was not run.

## Before opening

1. Fork, branch from current `main`, `git am` the three patches, set the author and add `Signed-off-by` (see `../flash-driver/BUILD.md`).
2. Rebase against #117311, #119889 and #122731 if any has merged; see REVIEW-NOTES.md.
3. Decide with the maintainers where the full pin-file fix goes (`hal_ti`, another repository). `evidence/g3519-pinctrl-audit.py` regenerates the comparison from SysConfig 1.28.0's `MSPM0G351X.json`.
