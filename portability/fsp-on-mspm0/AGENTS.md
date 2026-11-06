# Assistant notes for building Renesas FSP examples on an MSPM0 with TI DriverLib

The Renesas application source is copied unchanged. A layer from this repo answers its `R_*` calls with DriverLib. The layer is real driver code, so its faults are driver faults. Each item below comes from runs on one LP-MSPM0G3519.

Terms: [docs/vocab.md](../../docs/vocab.md) and the [README](../../ports/renesas-ra-fsp/README.md) of the results in the Fox Red Labs repo. In short: **FSP** is Renesas's Flexible Software Package; **DriverLib** is TI's C library for the MSPM0 peripherals; **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **ELC** and **DTC** are Renesas's event link and data transfer controllers, mapped here to the event fabric and DMA; **FT6336U** is the touch controller on the Waveshare 3.5 inch panel (I2C address 0x38). 1 kB = 1,024 bytes. Checked 2026-10-25 on an Intel Mac, macOS 15.7.9, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0, against `ra-fsp-examples` commit `c74f31fde709a8b27571fee279ec584720fae4cd`.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `portability/` or `ports/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `portability/fsp-on-mspm0/README.md`.

## The layer

| Part | Fact |
|---|---|
| What it is | 577 lines from this repo that implement the FSP calls five examples use (`R_IIC_MASTER_*`, `R_SCI_UART_*`, `R_ADC_*`, `R_GPT_*`, `R_ELC_*`, `R_DTC_*`, `R_BSP_SoftwareDelay`, the RTT console) on DriverLib, bare metal (`ports/renesas-ra-fsp/README.md`) |
| Files | `include/hal_data.h`, `include/SEGGER_RTT/SEGGER_RTT.h`, `src/fsp_mspm0.c`, `src/fsp_mspm0_gpt.c`, `src/fsp_mspm0_adcgpt.c`, and per board `boards/<board>/board.syscfg` and `hal_data.c` (`portability/fsp-on-mspm0/README.md`) |
| Examples | `iic_master` (5 constants changed for the FT6336U), `sci_uart`, `adc` (single scan), `gpt`: Works. `adc_gpt_periodic_sampling`: partial. The timer-to-ADC link works and the two-register DMA transfer does not (`ports/renesas-ra-fsp/README.md`) |
| Board proven | LP-MSPM0G3519 only; the folder has no second `boards/` entry (`portability/fsp-on-mspm0/README.md`) |
| Not covered | FSP calls the five examples do not use; the unchanged `iic_master` against an ADXL345 (no such part on the bus) (`portability/fsp-on-mspm0/README.md`) |

## Start here

| Step | This port |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07 at `<sdk>`, TI Arm Clang 5.1.1.LTS at `<cgt>`, SysConfig 1.26.3 or 1.28.0 at `<sysconfig>` (`ports/renesas-ra-fsp/README.md`) |
| Source | Renesas `ra-fsp-examples` at the commit above, folder `example_projects/ek_ra6m5/<example>/<example>_ek_ra6m5_ep/e2studio/src`, into `<proj>/app/` (`third-party/renesas/<example>/SOURCE.md`) |
| SysConfig | `<sysconfig>/sysconfig_cli.sh --compiler ticlang --product <sdk>/.metadata/product.json --output build/gen fsp-on-mspm0/boards/mspm0g3519/board.syscfg` |
| Compile and link | The loop in "Take it with you", `portability/fsp-on-mspm0/README.md`; `-DFSP_EX_<example>` picks the example's objects; under a minute |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | Main flash (512 kB) and NONMAIN before the first write; DSLite `memory -r` (`ports/renesas-ra-fsp/README.md`) |
| Flash | UniFlash DSLite with `mspm0g3519/MSPM0G3519_<probe serial>.ccxml` on `build/<example>.out` |
| Check | Console at 115200 on `/dev/cu.usbmodem<probe serial>1`, port held open, then reset the board so the banner is not missed (`ports/renesas-ra-fsp/capture.py`) |
| Restore | The backup, written back and read back |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Unchanged `iic_master` prints `Error in reading Device ID`, then loops printing 65535 | The example returns success after a wrong ID and no ADXL345 is on the bus | Point the ID and data constants at the device on the bus (`patches/iic_master.patch`) | Device-ID read returns 0x64 (`ports/renesas-ra-fsp/README.md`) |
| First I2C write after boot fails although the device is there | The FT6336U holds the bus busy for about 65 ms after its reset line rises | Hold the touch reset low 5 ms, release, wait 400 ms before the first transfer | First write completes (`ports/renesas-ra-fsp/README.md`) |
| The I2C callback never sets the event the example waits for | The example clears its flag after the call returns, so a callback fired inside the call is lost | Start the transfer in the call and finish it in the interrupt | Event arrives (`ports/renesas-ra-fsp/README.md`) |
| Menu input does nothing, or `25` is read as `2` then `5` | UART receive interrupt enabled too late; bytes handed over as they arrive | Enable it at start-up with a 32-byte ring and line-buffer the console | `SEGGER_RTT_Read` returns the whole line (`ports/renesas-ra-fsp/README.md`) |
| Duty 0 gives a pin that is high all the time | A compare value equal to the period leaves the pin at its start level | Force the pin low for 0 percent and high for 100 percent with `DL_Timer_overrideCCPOut` | 0.00 and 100.00 percent high (`ports/renesas-ra-fsp/README.md`) |
| A timer-triggered ADC converts once and stops | In sequence mode the ADC clears its enable bit after one sequence | Repeat mode, last conversion set to wait for the next trigger | Intervals of 670 to 672 counts (`ports/renesas-ra-fsp/README.md`) |
| The ADC converts but DMA moves nothing | DMA passes results only when the DMA sample count is 1 | `DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1)` | Samples arrive (`ports/renesas-ra-fsp/README.md`) |
| DMA moves 256 samples, then nothing | The ADC clears its DMA enable bit when a transfer ends | Set `DMAEN` again each time a channel is armed | Next group fills (`ports/renesas-ra-fsp/README.md`) |
| Two DMA channels on one ADC trigger fill group A in 512 triggers and never finish group B | Cause not found | Not solved; give each register its own trigger, or edit the application | None (`ports/renesas-ra-fsp/README.md`) |
| `DL_I2C_drainControllerRXFIFO` undeclared | Not in this DriverLib | Loop `DL_I2C_receiveControllerData` | Compiles (`ports/renesas-ra-fsp/README.md`) |
| OpenOCD attached to read a register restarts the program | `init` resets the core on this board | Measure inside the firmware with `-DFSP_SHIM_MEASURE` | Program keeps running (`ports/renesas-ra-fsp/README.md`) |

## Rules for the assistant

1. **Do not edit the Renesas source to fit the chip.** Fix the layer or the board files; the one allowed edit is the five constants of `patches/iic_master.patch` (`ports/renesas-ra-fsp/README.md`).
2. **Keep SysConfig's instance names** `TP_I2C`, `UART_0`, `PWM_0`, `ADC12_0` and `LED`, because the layer refers to the generated `*_INST` macros (`portability/fsp-on-mspm0/README.md`).
3. **Count the layer as written code.** Its 577 lines are driver logic, not renames; say so when you size an estimate (`ports/renesas-ra-fsp/README.md`).
4. **Measure on the pin or in firmware, not by watching an LED.** No LED was watched in any run (`ports/renesas-ra-fsp/README.md`, "Not checked").
5. **Other vendors' example packs are licensed for their own chips**; port from behavior, not by copying (`ports/renesas-ra-fsp/README.md`).
