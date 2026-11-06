# Port drivers and code to the MSPM33 family (LP-MSPM33C321A) and between it and MSPM0

MSPM0 DriverLib code runs on the MSPM33C321A with the app files untouched for GPIO, SysTick, timers, PWM, SPI, UART and CRC, when you carry the example's `.syscfg` and swap the device lines; I2C and the ADC need a rewrite, because the MSPM33 has the UNICOMM and HSADC blocks (matrix X7, X29, X51). 43 of 83 MSPM0 examples build that way, and 40 ran on the LP-MSPM33C321A with 39 clean fault registers (X29). The MSPM33 SDK ships no TI Drivers layer (X6).

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **DriverLib** is TI's register-level C library; **SysConfig** writes a chip's pin and clock code from a `.syscfg` file; **UNICOMM** is the combined serial block whose I2C calls are `DL_I2CC_` and `DL_I2CT_`; **HSADC** is the high-speed ADC; **TrustZone** is the Arm security split into secure and non-secure images; **QSPI** is the quad SPI flash interface. 1 kB = 1,024 bytes. **X29** and the other X numbers are row ids in `portability/matrix.csv`. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the part and its neighbors, a ten-minute path, the one-command build, the driver and function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (one part held)

| Part | Kit | Core and clock | Flash | RAM | On the board | Source |
|---|---|---|---|---|---|---|
| MSPM33C321A | LP-MSPM33C321A | Cortex-M33 up to 160 MHz, FPU, TrustZone | 1 MB | 256 kB | 0.91 inch OLED, QSPI NOR flash, RGB LED, two buttons, 80-pin BoosterPack headers, XDS110 with EnergyTrace | `mspm33c321a/AGENTS.md`, `mspm33c321a/README.md` |

Board folder: [`mspm33c321a/`](../../../mspm33c321a/AGENTS.md) (`README.md`, `AGENTS.md`, `FAQ.md`). No other MSPM33 part is held.

### Part-to-part map: the MSPM33C321A against its nearest neighbors

The map compares the one MSPM33 part held with the MSPM0 and AM13 parts code most often moves from. A dash means nothing is on record.

| | MSPM33C321A | MSPM0G3519 | AM13E230 |
|---|---|---|---|
| Core, clock | Cortex-M33, 160 MHz, FPU | Cortex-M0+, 80 MHz, no FPU | Cortex-M33, 200 MHz, FPU, TMU, NPU |
| Flash, RAM | 1 MB, 256 kB | 512 kB, 128 kB | 512 kB, 128 kB |
| I2C | UNICOMM (`DL_I2CC_`, `DL_I2CT_`); `DL_I2C_ClockConfig` unknown | I2C block (`DL_I2C_CONTROLLER_`, `DL_I2C_`) | redirect macros for MSPM0 I2C names |
| ADC | HSADC, about 5.0 million samples a second measured; 1.4 V internal reference | ADC12, two 4 Msps ADCs | sequencer and SOC ADC, three 12-bit ADCs |
| DMA | an MSPM33 DMA instance is required per channel in SysConfig | MSPM0 DMA | - |
| Interrupt groups | `DL_INTERRUPT_GROUP_1` is not in its DriverLib | present | answered through the alias header |
| GPIO ports | LEDs on three ports (blue PA2, red PC26, green PC27), so SysConfig writes one `_PORT` per LED | LEDs share port B, one `GPIO_LEDS_PORT` | LED4 on PA5 |
| GPIO input | an output pin reads 0 until `DL_GPIO_initDigitalInputFeatures` | - | - |
| Clock tree | 32 MHz default; 160 MHz after `PLL_QDIV` 10 and `EXTDIVMCLK` 1, 2 flash wait states, caches on | 80 MHz, `FLASHWAIT` 2 | 200 MHz |
| Flash base for code | 0x10000000 | 0x0 | 0x0 |
| Boot configuration | 0x80101800, never written | NONMAIN | NONMAIN 0x60100000 |
| Low power | MSPM0 standby sequences bus fault here (CFSR 0x200, HFSR 0x40) | - | STANDBY0 with only a watchdog wake never woke |
| Extra blocks | AES-256 (41.7 times tiny-AES-c at 1 kB), CRC, QSPI; no math accelerator | math accelerator, AES, TRNG, CAN-FD | motor PWM, PGAs, fast comparators |

Sources: `mspm33c321a/AGENTS.md`, `mspm33c321a/FAQ.md`, `docs/guides/chip-capability-map.md` (HSADC joystick, QSPI, clock tree), `portability/everywhere/PORTING.md` sections 6, 7.1 and 7.2, `docs/experiments/2026-10-24-mspm33-mspm0-driverlib-both-ways.md` (GPIO ports), `portability/three-ways/README.md` (1.4 V reference), `mspm0g3519/AGENTS.md`, `am13e230/AGENTS.md`, matrix X28, X29 and X31.

## Path: carry one MSPM0G3519 example to the LP-MSPM33C321A

Read the result over the probe. The commands are the ones the 2026-10-24 runs used, put in order; the path as a whole was not timed.

1. Terminal, no board. Build the pair with the porter, which edits the `.syscfg` as in `portability/everywhere/PORTING.md` section 7.1 (device `MSPM33C321A`, package `LQFP-100(PFA)`, board `/ti/boards/LP_MSPM33C321A`, pin lines and `SYSCTL` clock-tree lines commented out):

   ```bash
   python3 portability/everywhere/crossport_pair.py m0_to_m33 <work dir> <results.jsonl> gpio_toggle_output
   ```

   You should see one JSON line in `<results.jsonl>` with `ok`, the dropped SysConfig lines and the first error if any.
2. Terminal, board plugged in. Back up main flash, data flash and the boot configuration as `mspm33c321a/AGENTS.md` "Start here" says (each range read twice and compared; the boot configuration is read only).
3. Terminal. Check the image: `python3 tools/preflight_image.py mspm33c321a <image.out>`.
4. Terminal. Flash and read: `python3 portability/everywhere/bench_pair_run.py m33c321a <image.out> <test> <results.jsonl>` flashes the image through the probe lock and reads the fault registers, the GPIO outputs and the test's RAM variables without halting the core; DSLite ends with `Failed: Target is not halted` after the image is written and running, which is expected (`mspm33c321a/AGENTS.md`).
5. Restore the backup and read it back.

## One-command build

```bash
portability/three-ways/driverlib/build_m33.sh
```

It builds the shared 81-line application with the MSPM33 DriverLib and the HSADC glue (`glue_driverlib_hsadc.c`, 37 lines) and the `m33c321a.syscfg` file: `2,816` B of flash, 272 B of RAM, 9.9 s on this Mac. It does not flash (`portability/three-ways/README.md`). `portability/three-ways/build_all.sh` builds it together with the MSPM0 and AM13 versions.

## Driver and function map

Columns: the MSPM33 SDK's own DriverLib examples, MSPM0 DriverLib code carried over, and the other layers. A dash means nothing is on record.

| Function | MSPM33 DriverLib (SDK examples) | MSPM0 DriverLib code carried over | TI Drivers | Zephyr (main) | MicroPython | Arduino, FSP and Pico layers |
|---|---|---|---|---|---|---|
| GPIO | Works: blink, pin read over the probe ([board](../../../mspm33c321a/AGENTS.md)) | Works: `gpio_toggle_output` as is (X51) | Not for this chip: no TI Drivers in the SDK (X6) | Could work: `hello_world` builds; blinky does not (no `led0`); the GPIO driver does not compile for shields | A build on Zephyr main stopped at a missing `alloca.h` ([micropython-zephyr](../../toolchains/micropython-zephyr/README.md)) | - |
| UART | Works: STM32 UART DMA port, same six checks at 115200 ([STM32 port](../../../docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md)) | Works with internal loopback (`UART1.enableInternalLoopback = true;`) (X51) | Not for this chip | - | - | - |
| I2C | Works: the Education BoosterPack light sensor answers at 0x44 on PB13 and PB12 ([FAQ](../../../mspm33c321a/FAQ.md)) | Does not build; UNICOMM needs a rewrite (X51) | Not for this chip | - | - | - |
| SPI | Works: Education pack panel by DMA, 19.5 ms a frame at 16 MHz ([chip map](../../../docs/guides/chip-capability-map.md)) | Works: `spi_controller_internal_loopback_poll` as is (X51) | Not for this chip | No SPI node for shields | - | - |
| ADC | Works: HSADC about 5.0 million samples a second with the DMA fix; joystick through the sequencer | ADC12 code does not build; the three-ways `adc_read` was rewritten for the HSADC and ran ([three-ways](../../three-ways/README.md)) | Not for this chip | - | - | - |
| PWM and timers | Ran in the SDK sweep (177 of 178 left the debugger connected); behavior not checked one by one | Works: `systick_periodic_timer`, `timx_timer_mode_periodic_sleep`, `timx_timer_mode_pwm_edge_sleep` as is (X51) | Not for this chip | - | - | - |
| DMA | Works: SPI with DMA; UART RX and TX DMA with block-done in the STM32 port | SysConfig wants an MSPM33 DMA instance for every DMA channel (8 examples stop) (X29) | Not for this chip | - | - | - |
| Interrupts | Works: FreeRTOS from-ISR calls at `NVIC_SetPriority(irq, 0x40u >> (8 - __NVIC_PRIO_BITS))` | `DL_INTERRUPT_GROUP_1` aliases missing (4 examples stop) (X29) | Not for this chip | - | - | - |
| Flash | Works: QSPI NOR dump, guarded writes with `verify_bad` 0, page-cache reads ([chip map](../../../docs/guides/chip-capability-map.md)) | Every flash twin builds (X29); flash writes not run | Not for this chip | - | - | - |
| Clocks | Works: 160 MHz after `PLL_QDIV` 10 and `EXTDIVMCLK` 1 | Works with the MSPM0 `SYSCTL` clock-tree lines commented out (`portability/everywhere/PORTING.md` 7.1) | Not for this chip | - | - | - |
| Low power | - | MSPM0 standby code bus faults (`sysctl_lfxt_standby`, `timx_timer_mode_periodic_standby`) (X28, X29) | Not for this chip | - | - | - |
| RTOS | Works: FreeRTOS, 397 tasks with the heap raised to about 248 kB (`data/results/mspm33c321a.csv`) | bare metal | Not for this chip | Not in 4.4; main builds, not flashed | - | - |

Sources: matrix X6, X28, X29, X51; `mspm33c321a/AGENTS.md`; `mspm33c321a/FAQ.md`; `site/data/ports.json` (ports `zephyr-shields`, `stm32-usart-dma-rx-tx`); `data/support-matrix.json` (Zephyr, FreeRTOS, MicroPython cells); `portability/everywhere/PORTING.md` section 7.1.

## How to port into this family

1. **Pick the layer.** DriverLib is the only TI driver layer in the MSPM33 SDK 1.04 (X6). Code written to TI Drivers has nothing to land on; move it to DriverLib or plain C. Plain C moves without changes: ML-KEM-768 and ML-DSA-44 passed all 6 known-answer steps here with only new board glue (X39).
2. **Start from the MSPM33 example of the same name, or its `empty` example.** Build with the target SDK's makefile and `MSPM33_SDK_INSTALL_DIR`, `SYSCONFIG_TOOL`, `TICLANG_ARMCOMPILER` set and `GCC_ARMCOMPILER=` empty (`portability/everywhere/PORTING.md` section 7.1).
3. **Put board changes in a board folder.** The shared app pattern is `portability/three-ways/driverlib/m33c321a.syscfg` beside a shared glue file; the bench tests read the board's row in [`portability/boards/boards.csv`](../../boards/boards.csv).
4. **Check pins in the generated `ti_msp_dl_config.h`.** With pin lines commented out, SysConfig picks pins, and the app's LED is not the board's RGB LED unless you assign it (`portability/everywhere/PORTING.md` section 7.1). For the Education BoosterPack, I2C is PB13 and PB12, not the PB0 and PB1 several SDK READMEs list (`mspm33c321a/FAQ.md`).
5. **Check the link address.** Code links at 0x10000000; an image linked at 0x20 fails verify and leaves flash erased (`mspm33c321a/AGENTS.md`).
6. **Run the checks.** `python3 tools/preflight_image.py mspm33c321a <image>`, and `grep -E '^ +(BCR_CONFIG|BSL_CONFIG) ' <image>.map` must show 0 bytes used (`mspm33c321a/AGENTS.md`).
7. **Back up and restore.** Main flash at 0x10000000, data flash at 0x80000000, the boot configuration at 0x80101800 read only; the QSPI flash holding the demo video cannot be read by the debugger, so do not run TI's `qspi_flash_readback` examples (`mspm33c321a/AGENTS.md`).

## Moving inside the family

Only one MSPM33 part is held, so no move between MSPM33 parts has run. Code moved onto this part from MSPM0 and from other families is in the next section; every row is in [EVIDENCE.md](EVIDENCE.md).

## Moving to and from the other families

| To or from | Route | Measured | Rows and files |
|---|---|---|---|
| MSPM0 to MSPM33, app dropped into the MSPM33 project | App `.c` files only, MSPM33 SysConfig kept | 82 tried, 20 build; 17 ran clean, 1 hard fault in a low-power sequence | X28 |
| MSPM0 to MSPM33, configuration port | `portability/everywhere/crossport_mspm0_mspm33.py` carries the `.syscfg` | 83 tried, 43 build with the app untouched; 40 ran, 39 clean | X29 |
| MSPM0G3519 to MSPM33C321A, both ways on silicon | `crossport_pair.py`, `bench_pair_run.py` | 12 tried each way, 8 built untouched; 7 checked here by register and RAM reads, 9 of 9 checked on the G3519 (GPIO needs a 5-line app edit there) | X51, X52, [`PORTING.md` section 7](../../everywhere/PORTING.md) |
| One app on MSPM0, MSPM33 and AM13 | Shared `app.c`; the MSPM33 rewrites `adc_read` for the HSADC | 37 glue lines, 41 config lines, `2,816` B of flash; serial check passed | X53 |
| CC1354P10 to MSPM33C321A, plain C | Board harness only | 6 of 6 known-answer steps; ML-DSA signing needs 51 kB of stack | X39 |
| TI's CC1407P host driver | Plain C, 7 files, in the `empty` project | first SPI frame equal to the PC reference; no radio fitted | X45, `portability/cc1407p-host/README.md` |
| MSPM0 guide | | | [MSPM0 family guide](../mspm0/README.md) |
| AM13 guide | | | [AM13 family guide](../am13/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| MSPM0 examples built with their SysConfig carried | 43 of 83 | MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1 | measured | `portability/matrix.csv` X29 |
| Those run on the LP-MSPM33C321A | 40 ran, 39 clean | backed up and restored by the runner | measured | X29 |
| MSPM33 SDK examples | 252 of 258 build; 178 flashed and run | MSPM33 SDK 1.04.00.00 | measured | `mspm33c321a/README.md` |
| HSADC rate with the DMA fix | about 5.0 million samples a second | `hsadc_max_freq_dma`, single-transfer DMA | measured | `mspm33c321a/AGENTS.md` |
| Shared app with DriverLib and HSADC glue | `2,816` B flash, 272 B RAM | 37 glue lines | measured | `portability/three-ways/README.md` |
| ML-KEM-768 key pair, encapsulate, decapsulate | 8.1, 9.2, 11.3 ms | 160 MHz | measured | X39 |

## If you see this

- If the build stops at `DL_I2C_ClockConfig`, `ADC12_0_INST_INT_IRQN` or `DL_INTERRUPT_GROUP_1` on a carried MSPM0 example, the MSPM33 has UNICOMM I2C, the HSADC and no interrupt-group alias: rewrite those parts; GPIO, timer, SPI, UART and CRC move as is (`mspm33c321a/AGENTS.md`).
- If a carried UART example never finishes, it expects a TX to RX wire: add `UART1.enableInternalLoopback = true;` to the `.syscfg` (`portability/everywhere/PORTING.md` section 7.1).
- If a carried MSPM0 low-power example stops with a bus fault (CFSR 0x200, HFSR 0x40), the MSPM0 standby sequence does not fit this chip: write the sequence from an MSPM33 example (`portability/everywhere/PORTING.md` section 6).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

MSPM33 SDK 1.04.00.00, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, DSLite from CCStudio 21.0.1, probe firmware 3.0.0.43, on an Intel Mac with macOS 15.7.9; runs dated 2026-10-10 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/mspm33c321a.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/mspm33/EVIDENCE.md`. Checked 2026-10-25.
