# Build check: MSPM0 flash driver series for Zephyr

Terms: [docs/vocab.md](../../../../docs/vocab.md). 1 kB = 1,024 bytes. Built 2026-10-25 on an Intel Mac (macOS 15.7), compile only. Nothing in this file ran on a board.

| Piece | Version |
|---|---|
| Zephyr | `main` at `bc5d59d1b` (`v4.5.0-rc3-10`, 4.6 working draft), series applied on top (4 commits) |
| `hal_ti` | `53c80140` (the revision `west.yml` pins), `cmsis_6` and `picolibc` from an existing workspace |
| Compiler | Zephyr SDK 0.17.4 GCC 12.2.0 through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile` |

## Apply

```text
git clone --depth 50 https://github.com/zephyrproject-rtos/zephyr zephyr && cd zephyr
git checkout bc5d59d1b          # or any newer main; rebase the series if it does not apply
git am patches/*.patch          # the author line in each patch is a placeholder; see "Before opening"
```

## Build

`west build` needs a west workspace. These were built with plain CMake against a bare checkout, giving the three modules by path (the same thing `west` does):

```text
export ZEPHYR_BASE=<zephyr checkout> ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=<sdk>/arm-zephyr-eabi/bin/arm-zephyr-eabi-
MODS="<ws>/modules/hal/ti;<ws>/modules/hal/cmsis_6;<ws>/modules/lib/picolibc"
cmake -S $ZEPHYR_BASE/tests/drivers/flash/common -B out -GNinja -DBOARD=lp_mspm0g3519 -DZEPHYR_MODULES="$MODS"
ninja -C out
```

In a normal workspace: `west build -b lp_mspm0g3519 tests/drivers/flash/common` and `twister -T tests/drivers/flash -p lp_mspm0g3519 --build-only`. Those two were not run.

## Result

| Target | Flash | RAM | Warnings |
|---|---|---|---|
| `tests/drivers/flash/common`, `lp_mspm0g3519` | 30.6 kB of 256 kB (11.95 %) | 9.9 kB of 64 kB (15.39 %) | 0 |
| `tests/drivers/flash/common`, `lp_mspm0g3507` | 30.5 kB of 128 kB (23.83 %) | 9.8 kB of 32 kB | 0 |
| `tests/drivers/flash/common`, `lp_mspm0l2228` | 30.1 kB of 256 kB (11.77 %) | 9.7 kB of 32 kB | 0 |
| `samples/drivers/flash_shell`, `lp_mspm0g3519` | 69.2 kB of 256 kB (27.05 %) | 42.9 kB of 64 kB (67.08 %) | 0 |

`CONFIG_FLASH_SOC_TI_MSPM0=y`, `CONFIG_DT_HAS_TI_MSPM0_FLASH_CONTROLLER_ENABLED=y` and `CONFIG_USE_MSPM0_DL_FLASHCTL=y` are set in each. The 64 kB RAM figure for the G3519 is what Zephyr's devicetree gives it today (the chip has 128 kB). `flash_shell` is the generic flash shell sample; `samples/drivers/soc_flash_nrf` is Nordic-specific and was not used.

## Style check

`perl scripts/checkpatch.pl --no-tree <patch>` on each patch: 0 warnings. The one error per patch is `MISSING_SIGN_OFF`, which is left to the human submitter on purpose. `scripts/ci/check_compliance.py` was not run.

## Before opening

1. Fork `zephyrproject-rtos/zephyr`, branch from current `main`, `git am` the four patches (fix the author with `git rebase --exec 'git commit --amend --reset-author --no-edit -s' <base>`, which also adds `Signed-off-by`).
2. Add `Co-developed-by:` and `Signed-off-by:` for the authors of #91500 with their agreement, or ask them first.
3. Re-run the build above on the rebased branch, then `twister` and `check_compliance.py`.
4. Optionally run `tests/drivers/flash/common` on an LP-MSPM0G3519 with `west flash` before claiming the test passes; the series says only that it builds.
