# Shims: headers that let one chip family's TI example build on another

Terms: [docs/vocab.md](../../docs/vocab.md). `mspm0_on_f3/`, `f3_on_mspm0/` and `mspm0_on_am13e230/` each hold a force-included header and stub headers that cover the named gaps between TI driver APIs. Every line in a shim is one gap, and a shim never edits the application. The build results are in `../matrix.csv`. Needs TI's SDKs; builds nothing by itself.

## The ten TI Drivers twin cells (2026-10-20)

Ten driver examples ship in both the MSPM0 SDK 2.11 (LP-MSPM0G3507, FreeRTOS) and the F3 SDK 9.21 (LP-EM-CC2340R5, FreeRTOS). Each cell builds the other family's app source, copied byte for byte, inside the target family's own project (its board config, FreeRTOS and linker file). Build only; nothing was flashed or connected.

| Cell (MSPM0 / F3 name) | MSPM0 app on CC2340R5 | F3 app on G3507 | App lines MSPM0 / F3 | Identical lines | Shim lines used, to F3 (+ pin flags) | Shim lines used, to MSPM0 | Board overlay lines |
|---|---|---|---|---|---|---|---|
| adc_singlechannel / adcsinglechannel | yes | yes | 148 / 142 | 96 | 1 (+1) | 0 | 4 |
| gpio_interrupt / gpiointerrupt | yes | yes | 52 / 63 | 36 | 0 (+4) | 2 | 0 |
| i2c_controller / i2ccontroller | yes | yes | 210 / 204 | 155 | 1 (+1) | 2 | 0 |
| i2c_target / i2ctarget | yes | yes | 374 / 317 | 234 | 2 (+2) | 3 | 3 |
| i2c_tmp / i2ctmp | yes | yes | 180 / 190 | 138 | 2 (+2) | 1 | 0 |
| spi_controller / spicontroller | yes | yes | 208 / 214 | 153 | 2 (+3) | 3 | 0 |
| spi_peripheral / spiperipheral | yes | yes | 232 / 246 | 171 | 2 (+3) | 3 | 0 |
| uart_callback / uart2callback | yes | yes | 107 / 88 | 34 | 15 (+0) | 14 | 0 |
| uart_echo / uart2echo | yes | yes | 55 / 63 | 23 | 12 (+0) | 13 | 0 |
| empty_freertos / empty | yes | yes | 32 / 36 | 21 | 1 (+1) | 1 | 0 |
Totals: 10 of 10 build in each direction (TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0). Before this pass 5 of 10 built each way. The counts above, plus the lines only in the MSPM0 or only in the F3 source, are in `data/lines.csv`.

How to read the counts:

- "Identical lines" is how many non-blank lines the two vendor sources already share (LCS of trimmed lines, license block skipped). The two examples are different programs written for the same API, so the SDKs already agree on about two thirds of the lines (1,061 of 1,563 F3 lines and of 1,598 MSPM0 lines).
- Shim lines used is the number of `#define` lines in the shim header whose name the app mentions. The whole headers are 39 lines (MSPM0 to F3, 47 with the one-line stub header and the two overlays) and 38 lines (F3 to MSPM0, 40 with its two stub headers). Pin flags are the `-D CONFIG_*_IOMUX=0` flags the build script adds, one per pin-mux name the MSPM0 app passes to `GPIO_setConfig` (F3 takes the mux from its board table).
- Board overlay lines: SysConfig text appended to an F3 example that lacks something the MSPM0 app uses. `mspm0_on_f3/led0.syscfg` (4 lines) adds LED 0 (the ADC cell); `display.syscfg` (3 lines) adds the UART display (the I2C target cell). The MSPM0 side has no SysConfig in its SDK (only generated code), so the F3 to MSPM0 direction uses `-DFR_NO_GPIO_DRIVER` for the two UART cells, whose MSPM0 board files have no TI Drivers GPIO table: the LED calls become the DriverLib calls their own MSPM0 examples use (LED 0, GPIOA pin 0 only).

The gaps closed in this pass, each one line in a header: UART names (`CONFIG_UART_0`, `CONFIG_UART_BAUD_RATE`, `CONFIG_UART_BUFFER_LENGTH`, `CONFIG_UART2_0`), LED names (`CONFIG_LED_ON`, `CONFIG_GPIO_LED_0_ON`, `CONFIG_GPIO_LED_ON`, F3's second LED), I2C target names (`CONFIG_I2C_TARGET_0` and `CONFIG_I2C_0`, and `I2C.h` for the types the MSPM0 error handler uses), and the DriverLib LED call two MSPM0 UART examples make.

Limits:

- Built, not run. The first 5 MSPM0 apps ran on a CC2340R5 on 2026-10-06 (`../matrix.csv`); the other 5, and all 10 F3 apps on the G3507, have not run. A shim that builds can still change behavior: the LED 1 alias, the dropped `GPIO_CFG_HIZ_*` flags and the DriverLib LED mapping are the lines to check on a board.
- The FreeRTOS library for the MSPM0 projects comes from `../crossbuild/build_mspm0_freertos.sh`; 2 of its 41 files fail to compile and the 10 images still link.
- MSPM33 is not in this table: the MSPM33 SDK 1.04 ships no TI Drivers layer, so this app source has nothing to land on there. The DriverLib route is in `../matrix.csv`. Not run here for that reason.

Rebuild (build step only; edits nothing under `/Applications/ti`):

```bash
../crossbuild/build_mspm0_freertos.sh /tmp/frlib
./build_twins.sh m0_to_f3 /tmp/w /tmp/w/a.jsonl
./build_twins.sh f3_to_m0 /tmp/w /tmp/w/b.jsonl /tmp/frlib/freertos.lib
python3 count_lines.py --csv
```

## The ten twins run on the LP-MSPM0G3519 (2026-10-25)

The F3 to MSPM0 direction was rebuilt for the LP-MSPM0G3519 (`MSPM0_BOARD=LP_MSPM0G3519 ./build_twins.sh f3_to_m0 ...`, FreeRTOS library from `../crossbuild/build_mspm0_freertos.sh <dir> LP_MSPM0G3519`) and every image was flashed, started with an nRST pulse and watched on the backchannel and the LED pin. All 10 build; the board was backed up first and restored. The rows are in `data/results/mspm0g3519.csv`.

| Cell | Outcome | Cause class |
|---|---|---|
| empty | Works: LED 0 blinks once a second | |
| uart2echo | Works: prompt and echo of typed text | |
| uart2callback | Works: same | |
| adcsinglechannel | Runs, prints its start line only; TI's own example for this board does the same | toolchain |
| gpiointerrupt | Runs; button path not observed | no-hardware (a finger on S2 or S1) |
| i2ccontroller | Runs, waits for a target; LED 0 never driven | no-hardware, plus port-tool (PA0 left unconfigured) |
| i2ctarget | Runs, no controller; LED 0 never driven | no-hardware, plus port-tool |
| i2ctmp | I2C opens; no sensor answers at 0x48 or 0x49 | no-hardware |
| spicontroller | Waits for the peripheral's ready pin | no-hardware |
| spiperipheral | Waits for the controller | no-hardware |

Tally: 3 pass, 1 toolchain, 6 no-hardware (two of them also port-tool). None is does-not-fit (images use 13.4 to 29.7 kB of 512 kB flash and 5.9 to 7.7 kB of 128 kB RAM) or not-on-this-chip.
