# FSP calls on MSPM0 DriverLib

Terms: [vocab](../../docs/vocab.md). **FSP** is Renesas's Flexible Software Package; **DriverLib** is TI's C library for the MSPM0 peripherals.

Maps the Renesas FSP calls that five `ra-fsp-examples` use (`R_IIC_MASTER_*`, `R_SCI_UART_*`, `R_GPT_*`, `R_ADC_*`, `R_ELC_*`, `R_DTC_*`, `R_BSP_SoftwareDelay`, `R_IOPORT_Open`, the SEGGER RTT console) onto TI DriverLib, bare metal, for the LP-MSPM0G3519. It is real driver code, not a rename shim: count it as written lines. Measurements, the examples and what broke: `ports/renesas-ra-fsp/README.md`.

- `include/hal_data.h` (141 lines): the FSP types and prototypes the examples use. Names follow the public interface; the code is this repo's. `R_ADC0->ADDR[n]` is ADC0's result word `n`, so the DTC source addresses in the Renesas example work as written.
- `src/fsp_mspm0.c` (228 layer lines, 14 more in measurement builds): the I2C, UART and single-scan ADC calls, the I2C2 and UART0 interrupt handlers, the line-buffered console, `main()`.
- `src/fsp_mspm0_gpt.c` (97 layer lines, 37 measurement): `R_GPT_*` for periodic and one-shot on TIMG12 (channel 0), PWM on TIMA1 CC1 (channel 1) and TIMG0 (channel 2). Built for `sci_uart`, `gpt` and `adc_gpt_periodic_sampling`.
- `src/fsp_mspm0_adcgpt.c` (103 layer lines, 71 measurement and experiment): `R_ELC_*` as event-fabric channel 1 (TIMG0 zero event to ADC0), `R_DTC_*` as DMA channels 0 to 3 on the ADC DMA trigger, and the group-scan ADC calls. Built for `adc_gpt_periodic_sampling` only (`-DFSP_ADC_GROUP`).
- `boards/mspm0g3519/`: `board.syscfg` (64), `adc_gpt_periodic_sampling.syscfg` (79, picked by `build.sh` for that example) and the hand-written stand-in for the configurator's `hal_data.c` (46; the objects of each example sit under `#if defined(FSP_EX_<example>)`).
- `build.sh <example>`: copies `third-party/renesas/<example>/src`, applies `ports/renesas-ra-fsp/patches/<example>.patch` if there is one, runs SysConfig on the board's `.syscfg`, compiles the app, the layer files that example needs, the generated config and TI's startup file, links, prints the size. Measurement builds: `FSP_SHIM_CFLAGS=-DFSP_SHIM_MEASURE`.

A second board needs its own `boards/<board>/` folder and the instance names in the layer (`TP_I2C_INST`, `UART_0_INST`, `PWM_0_INST`, `ADC12_0_INST`, `LED_PORT`) from its SysConfig file, plus the timer, event channel and DMA numbers in the two new files.

## Take it with you

Copy these files into a folder of your own, keeping the folder names. The layer is code from this repo; the application is Renesas's, unchanged.

| File | What it is |
|---|---|
| `include/hal_data.h` | The FSP types and prototypes the examples use, on TI DriverLib |
| `include/SEGGER_RTT/SEGGER_RTT.h` | 8-line stand-in for the SEGGER RTT header; console goes to UART0 |
| `src/fsp_mspm0.c` | I2C, UART and single-scan ADC calls, the interrupt handlers, the console, `main()` |
| `src/fsp_mspm0_gpt.c` | `R_GPT_*` on TIMG12, TIMA1 and TIMG0; needed by `sci_uart`, `gpt` and `adc_gpt_periodic_sampling` |
| `src/fsp_mspm0_adcgpt.c` | `R_ELC_*` and `R_DTC_*` on the event fabric and DMA; `adc_gpt_periodic_sampling` only, built with `-DFSP_ADC_GROUP` |
| `boards/mspm0g3519/board.syscfg` | The pin and peripheral setup for SysConfig; `adc_gpt_periodic_sampling.syscfg` replaces it for that example |
| `boards/mspm0g3519/hal_data.c` | Hand-written stand-in for the configurator's `hal_data.c`; each example's objects sit under `#if defined(FSP_EX_<example>)` |
| `app/*.c`, `app/*.h` | The Renesas example source, unchanged: from `github.com/renesas/ra-fsp-examples` at commit `c74f31fde709a8b27571fee279ec584720fae4cd`, folder `example_projects/ek_ra6m5/<example>/<example>_ek_ra6m5_ep/e2studio/src` (BSD-3-Clause) |
| `iic_master.patch` (`ports/renesas-ra-fsp/patches/`) | Only for `iic_master` retargeted to the FT6336U: `patch -p1` inside `app/` |

**What it needs.**

- MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.26.3 or 1.28.0.
- Defines: `-DFSP_EX_<example>` (the folder name, for example `-DFSP_EX_sci_uart`) picks the objects in `hal_data.c`; `-DFSP_ADC_GROUP` for the ADC-with-timer example. Add `-DFSP_SHIM_MEASURE` only for the measurement image.
- Include paths: the SysConfig output folder, `include/`, `app/`, the SDK's `source` and `source/third_party/CMSIS/Core/Include`, and the SysConfig-written `device.opt`. No force-include flag; `hal_data.h` is included by the Renesas files themselves.
- Compiler flags: `-Os -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb` (the G3519 has a Cortex-M0+). TI's `startup_mspm0g351x_ticlang.c` from the SDK is the last object.
- Wiring for the I2C example: touch controller on PC2 (SCL) and PC3 (SDA), its reset on PB11; console on PA10 and PA11 (the probe's serial port).

**A different board in the same family** (another MSPM0 with SysConfig): copy `boards/mspm0g3519/` to `boards/<board>/`. In `board.syscfg` change the `--device` and `--package` lines at the top and the pin assignments (`assignedPin`, `peripheral.sdaPin.$assign`). Keep the instance names `TP_I2C`, `UART_0`, `PWM_0`, `ADC12_0` and the `LED` port: the layer uses `TP_I2C_INST`, `UART_0_INST`, `PWM_0_INST`, `ADC12_0_INST` and `LED_PORT`. If the new chip's timers or DMA differ, change the timer table (`s_hw` in `fsp_mspm0_gpt.c`), the event channel and DMA channel numbers in `fsp_mspm0_adcgpt.c`, and the startup file name in the build command.

**Build from a fresh folder** (`<proj>` holds `app/`, `fsp-on-mspm0/` with `include/`, `src/` and `boards/`; `<sdk>`, `<cgt>` and `<sysconfig>` are the three tool folders):

```bash
cd <proj> && mkdir -p build/gen
<sysconfig>/sysconfig_cli.sh --compiler ticlang --product <sdk>/.metadata/product.json --output build/gen fsp-on-mspm0/boards/mspm0g3519/board.syscfg
CF=(-Ibuild/gen -Ifsp-on-mspm0/include -Iapp @build/gen/device.opt -I<sdk>/source/third_party/CMSIS/Core/Include -I<sdk>/source
    -DFSP_EX_sci_uart -Os -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -Wno-unused-variable -gdwarf-3)
for f in app/*.c fsp-on-mspm0/src/fsp_mspm0.c fsp-on-mspm0/src/fsp_mspm0_gpt.c fsp-on-mspm0/boards/mspm0g3519/hal_data.c \
         build/gen/ti_msp_dl_config.c <sdk>/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c; do
  <cgt>/bin/tiarmclang "${CF[@]}" -c "$f" -o "build/$(basename "${f%.c}").o"
done
<cgt>/bin/tiarmclang -mcpu=cortex-m0plus -march=thumbv6m -mthumb -Wl,-u,_c_int00 build/*.o -Wl,-lbuild/gen/device.cmd.genlibs \
  -L<sdk>/source build/gen/device_linker.cmd -Wl,--rom_model -Wl,--warn_sections -L<cgt>/lib -llibc.a -o build/sci_uart.out
```

Built from a copy of only these files on 2026-10-25 (`sci_uart`, SysConfig 1.28.0, copy outside the repo, zero errors): 13.1 kB of text and data and 567 B of bss, the same as the results table. Only `sci_uart` was built this way; for another example add its `app/` source and its objects as `build.sh` lists them.

## Set up once

Variables, defaults and sizes are in the "Set up once" table of `ports/renesas-ra-fsp/README.md`: `SDK` (MSPM0 SDK 2.11.00.07), `CGT` (TI Arm Clang 5.1.1.LTS), `SYSCFG` (SysConfig 1.26.3 or 1.28.0) and `OUT`. `build.sh` stops with one line naming the variable when a path is missing.

## Not checked

Only the LP-MSPM0G3519 has a `boards/` folder. `R_DTC_*` mapped as one DMA channel per transfer element does not reproduce the RA behavior: with two or more channels on the one ADC DMA trigger the first group fills at half rate and the second does not complete (`ports/renesas-ra-fsp/README.md`, "What went wrong"). FSP calls the five examples do not use are absent.

## License and source

The layer is this repo's code and carries no Renesas source; it uses FSP's public function names. The Renesas examples it serves are BSD-3-Clause (`third-party/renesas/LICENSE`). SEGGER RTT is replaced by an 8-line header, not copied.

