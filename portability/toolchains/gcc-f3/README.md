# GCC builds of the F3 SDK examples for the CC2340R5 and CC2755P10

Terms: [docs/vocab.md](../../../docs/vocab.md). The SimpleLink Low Power F3 SDK examples build with GCC for every example that ships a GCC makefile: 144 of 144 built with GCC, unchanged, for both LaunchPad kits (nortos and FreeRTOS). Radio stack examples (BLE, Zigbee, proprietary RF, DMM) ship no GCC makefile at all, and the BLE and Zigbee libraries ship only for TI Arm Clang (and IAR for BLE).

Built only. Not run on silicon.

## Headline

| Board | Core | Examples in SDK | Ship a gcc makefile | Built with GCC unchanged | Built with TI Arm Clang | Median text, GCC / ticlang |
|---|---|--:|--:|--:|--:|--:|
| LP_EM_CC2340R5 | Cortex-M0+ | 108 | 64 (27 nortos, 37 FreeRTOS) | 64 of 64 | 107 of 108 | 1.31 |
| LP_EM_CC2755P10 | Cortex-M33 | 106 | 80 (33 nortos, 47 FreeRTOS) | 80 of 80 | 105 of 106 | 1.26 |
| Both | | 214 | 144 | 144 of 144 | 212 of 214 | |

"Examples" counts every folder that has a `gcc/` or a `ticlang/` makefile folder, one per RTOS variant (`nortos/drivers/empty` and `rtos/drivers/empty/freertos` are two). Of 461 gcc projects across all F3 boards, 144 are on these two boards.

- "Unchanged" means the shipped `gcc/makefile`, linker script and sources were not edited. Only make variables were set on the command line.
- GCC warnings: 1 across 144 builds, and it is a SysConfig message (`mcuboot/simpleboot`: "noAuthentication ... not recommended for production use"), not a compiler warning.
- Build time per example, one at a time inside a 4-way run: median 29 s (CC2340R5) and 31 s (CC2755P10) with GCC. Most of it is SysConfig.
- The two TI Arm Clang failures are examples that have no GCC makefile either, so they do not touch the GCC result: `CC2340R5 rtos/ble/basic_ble_multistep_oad` (`make: *** [prebuild] Error 126`, the prebuild script is not runnable) and `CC2755P10 rtos/dmm/ble_hci_ser_mux` (`fatal error: 'host_test_app.h' file not found`).
- Spot check of the produced ELF (empty FreeRTOS example): CC2340R5 `Tag_CPU_arch: v6S-M`, Thumb-1. CC2755P10 `Tag_CPU_arch: v8-M.mainline`, `Tag_FP_arch: FPv5/FP-D16`. The SDK's own `crc_tool patch-image` post-build step ran on every GCC image and succeeded (a build counts only if make returned 0 and a `.out` exists).

## What does not have a GCC makefile (not a GCC failure)

| Group | CC2340R5 | CC2755P10 |
|---|---|---|
| ble | 0 of 12 | 0 of 11 |
| zigbee | 0 of 17 | 0 of 11 |
| prop_rf | 0 of 13 | (none in this SDK for this board) |
| dmm | | 0 of 2 |
| drivers | 60 of 62 | 71 of 73 |
| mcuboot, hsm, secureboot, demos | all | all |

The other drivers gap is `fatsd` (nortos and FreeRTOS), on both boards. Everything else in the drivers, demos, mcuboot, hsm and secureboot groups has a GCC makefile and built. The TF-M examples (`tfm_empty`, `tfm_psa*`) on the CC2755P10 built too, using the SDK's prebuilt `tfm_s` images.

## Radio stacks have no GCC libraries

The BLE and Zigbee stacks cannot build with GCC from this SDK, because the prebuilt libraries are not shipped for GCC.

- BLE, CC2340R5: `source/ti/ble/lib/CC2340R5/{OneLib,StackWrapper,ControllerLib}/lib/{ticlang,iar}/m0p/*.a`. There is no `lib/gcc/` folder. Same for CC2755P10: `source/ti/ble/lib/CC27XXX10/{OneLib,StackWrapper,ControllerLib}/lib/{ticlang,iar}/m33f/*.a`. (Six `.a` files per board, two toolchains each, none for GCC.)
- Zigbee: `source/third_party/zigbee/libraries/zboss_stable/zb_r22_*/lib/ticlang/{m0p,m33f}/` holds 168 ZBOSS libraries (84 variants, one per M0+ and M33F). Zero for GCC, zero for IAR. Examples: `zb_r22_core_zed`, `zb_r22_core_zr`, `zb_r22_zcl_zed`, `zb_r22_ti_platform_zed`.
- Thread: no Thread example ships in this SDK's `examples/` for these boards.
- What does ship for GCC in `lib/gcc/`: the RF patch libraries (`source/ti/devices/*/rf_patches/lib/gcc/`, for example `rfe_ble5_cc23x0r5.a`, `pbe_ble5_cc23x0r5.a`), the radio control library (`source/ti/drivers/rcl/lib/gcc/{m0p,m33f}/rcl_cc23x0r5.a`, `rcl_cc27xxx10.a`), driverlib, drivers, display, log, hsmddk and DMM logging. So the proprietary RF examples (`prop_rf`, CC2340R5) have their GCC libraries; they lack a GCC makefile only. Whether they would build with a hand-made GCC makefile is not tested here.
- Not tested: whether a hand-made GCC makefile could link the BLE stack against the ticlang `.a` files. It cannot be assumed; the archives hold TI Arm Clang objects and runtime helper calls.

## Failures

None on the GCC side: 0 compile errors, 0 link errors, 0 missing files across 144 builds. The only non-builds are the 44 CC2340R5 and 26 CC2755P10 folders that ship no `gcc/` makefile, grouped above.

## Size comparison (144 examples built by both)

Both measured with Berkeley `size` (text, data, bss) on the linked `.out`.

| Board | n | text GCC / ticlang median | quartiles | min, max | GCC larger |
|---|--:|--:|---|---|--:|
| CC2340R5 | 64 | 1.31 | 1.18 to 1.38 | 1.01 to 1.66 | 64 of 64 |
| CC2755P10 | 80 | 1.26 | 1.22 to 1.30 | 1.03 to 1.55 | 80 of 80 |

- By variant: CC2340R5 nortos 1.16 (27), FreeRTOS 1.34 (37). CC2755P10 nortos 1.22 (33), FreeRTOS 1.30 (47).
- Median extra `.text` with GCC: 5.7 kB (CC2340R5), 6.4 kB (CC2755P10).
- Examples: CC2340R5 `rtos/drivers/empty/freertos` 17.9 kB GCC vs 13.7 kB ticlang; `nortos/drivers/empty` 8838 vs 7781. CC2755P10 `rtos/drivers/empty/freertos` 22024 vs 16624; `nortos/drivers/empty` 12578 vs 10233. Largest ratio `rtos/drivers/i2copt3001_cpp/freertos` (1.66 on CC2340R5, 1.55 on CC2755P10).
- This is not a like-for-like compiler comparison. The shipped makefiles use GCC `-O3` in every FreeRTOS project and most nortos ones, and TI Arm Clang `-Oz` (76 nortos ticlang projects) or `-Os`. The ratio mixes compiler and optimization level.
- `.data` and `.bss` are not comparable. GCC reports about 240 to 270 bytes of `.data` (median) where ticlang's `tiarmsize` reports 0, because the two linker files lay out the initialized RAM sections differently. `.bss` is within about 1 percent. Compare `.text` only.

## Method

1. `run.sh` makes a scratch SDK in `/private/tmp/gcc-f3-work`: `source`, `kernel`, `.metadata`, `tools`, `tfm_s` and `imports.mak` are symlinks into `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS`; the `examples/rtos/<board>` and `examples/nortos/<board>` trees are copied. Nothing is written into `/Applications/ti`.
2. It lists every example folder with a `gcc/` or `ticlang/` directory. For each, `build_one.sh` runs the shipped makefile in `gcc/` and then in `ticlang/` (`make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=<scratch> SYSCONFIG_TOOL=<sysconfig_cli.sh> GCC_ARMCOMPILER=<shim> TICLANG_ARMCOMPILER=<ticlang>`). The makefiles call SysConfig, build, run the SDK's `crc_tool` and make a `.hex`.
3. A build counts only if make returned 0 and a `.out` exists. Sizes come from `arm-zephyr-eabi-size` and `tiarmsize`. Objects, `.out`, `.hex`, `.map` and generated files are deleted after the size is recorded.
4. One JSON line per example (built, rc, secs, text, data, bss, warnings, cause, first_error). `analyze.py` prints the tables and failure groups.

Rerun: `JOBS=4 ./run.sh` (both boards, about 40 minutes), or `./run.sh LP_EM_CC2340R5`, or `ONLY=drivers/empty ./run.sh LP_EM_CC2755P10`. Then `python3 -I analyze.py`. Needs about 100 MB of scratch.

## Tool versions

| Tool | Version |
|---|---|
| GCC | arm-zephyr-eabi-gcc 12.2.0 (Zephyr SDK 0.17.4), newlib nano, multilibs `thumb/v6-m/nofp` and `thumb/v8-m.main+fp/hard` present |
| TI Arm Clang | 5.1.1.LTS (`ti-cgt-armllvm_5.1.1.LTS` in CCS 20.2.1) |
| SysConfig | 1.26.3 (the version the SDK release notes list) |
| F3 SDK | 9.21.00.36 LTS |

The SDK names Arm GNU Toolchain 12.3.Rel1-0 (`imports.mak`: `GCC_ARMCOMPILER ?= /Applications/arm-none-eabi-gcc/12.3.Rel1-0`) and TI Arm Clang 4.0.4.LTS. Neither is installed. The Zephyr SDK GCC 12.2 was used, one minor release older than the documented 12.3, and ticlang 5.1.1 was used, newer than the documented 4.0.4. So the result says "builds with GCC 12.2", not "builds with the exact documented GCC". The SDK says earlier or newer compilers are not supported.

One shim: the makefiles call `$(GCC_ARMCOMPILER)/bin/arm-none-eabi-gcc` and objcopy, and use `$(GCC_ARMCOMPILER)/arm-none-eabi/{include,lib}`. `run.sh` writes small wrapper scripts named `arm-none-eabi-{gcc,size,objdump,objcopy,ar,nm}` that exec the Zephyr tools, plus a symlink `arm-none-eabi` to the Zephyr target folder. Same compiler, only the prefix differs.

## What this does not show

- Not run on silicon. No board was touched. Next rung: flash a sample of the GCC images on the CC2340R5 and CC2755P10 LaunchPad kits (empty, gpiointerrupt, uart2echo, the FreeRTOS demo, an AES driver) and compare pins and serial output against the ticlang build, as was done for the LP-MSPM0L1306 in `portability/toolchains/gcc-mspm0/`.
- Not the radio path: no BLE, Zigbee, Thread or DMM image builds with GCC from this SDK as shipped (section above). Next rung: try the `prop_rf` examples with a GCC makefile adapted from a driver example.
- Not the exact documented compilers (GCC 12.3.Rel1, ticlang 4.0.4).
- Sizes use each makefile's own flags, so the ratio mixes compiler and optimization level.
- Only two boards of the six F3 boards in the SDK.
