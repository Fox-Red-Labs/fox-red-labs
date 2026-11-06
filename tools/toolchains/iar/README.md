# IAR Build Tools on TI's MSPM0, MSPM33, AM13 and SimpleLink F3 SDKs

Terms: [docs/vocab.md](../../../docs/vocab.md).

IAR C/C++ Compiler 10.10.2 for Arm (Linux, evaluation license) builds TI's IAR projects on a Mac mini build host in the image
`frl-iar:10.10.2`. Do not rebuild the image on the build host. Results are in `data/toolchains/iar/<sdk>.csv`; every build that
is not `pass` carries `cause_class` and `cause`. CoreMark scores are in the CoreMark section below.

| File | Job |
|---|---|
| `iar_harness.py` | list and build an SDK's `.ewp` projects in the container, write the CSV |
| `sdk_fixes.py` | project-file fixes applied to the build host's SDK copy before a build (originals kept as `.orig`) |
| `crc_tool_linux.py`, `imgtool_linux.sh`, `pyscript_linux.sh` | Linux stand-ins the harness mounts over TI's macOS-only post-build tools |
| `coremark_iar.sh`, `cm_build_remote.sh`, `cm_run.py` | CoreMark build (MacBook runs `cm_build_remote.sh`), flash and read-out on LP-MSPM33C321A |
| `classify_csv.py` | adds `cause_class` and `cause` to the CSVs |

## Status 2026-10-24

| SDK | Builds | Pass | Not pass |
|---|---|---|---|
| MSPM0 2.11.00.07 | 1,362 | 1,343 | 19 `toolchain`: 18 LP-MSPM0G5187 TinyUSB examples (IAR Pe852, `void *` arithmetic in `dcd_musb.c` lines 288 and 319 and `hcd_musb.c` line 491; a cast compiles), 1 `secondary_bsl_dfu` (the SDK ships no `tusb_config.h`; TI Arm Clang stops at the same include) |
| MSPM33 1.04.00.00 | 167 | 167 | none |
| AM13E230 26.01.00.03 | 332 | 330 | 2 `toolchain`: the stale `3sh_hall_foc_drv8323rh` project file names sources that are not in the SDK; its sibling `3sh_hall_sen_foc_drv8323rh` builds |
| SimpleLink F3 9.21.00.36 | 631 | 595 | 32 `toolchain`: link but the post-build `sbtool` is a macOS binary with no source in the SDK; 4 `toolchain`: the MS-OAD pre-build starts its own IarBuild with a `c:/ti/sysconfig_1.26.3` path |

`pass` on F3 means linked and post-built: the CRC step ran and the `.out` is flash-ready. No F3 image was flashed here. No example from these SDKs was run on a board except CoreMark on the LP-MSPM33C321A.

### Fixes that moved the counts

- **AM13 (330 of 332 had failed with Error[Ta108])**: the SDK's `.ewp` files carry no FPU (`FPU2` 0) while the project arguments pass `--aapcs=vfp`. The chip has the unit: data sheet SPRSPC3A (Feb 2026, rev. A Aug 2026), Features: "Floating Point Unit (FPU), Custom Datapath Extension (CDE), Memory Protection Unit (MPU) and Micro Trace Buffer (MTB)" and "DSP Extension"; the SDK's `soc.h` sets `__FPU_PRESENT`; TI's Clang builds use `-mfpu=fpv5-sp-d16`. `sdk_fixes.py` writes the MSPM33 SDK's values (VFPv5 single precision). Vendor facts, no view: IAR's device file `AM13E23019.i79` lists `fpu=VFPv5_SP` and `SIMD=false` (the data sheet lists a DSP extension); `iarbuild` takes the cpu and FPU from the `.ewp`, not from that file, so the builds use `Cortex-M33.no_dsp.no_se` with `--fpu=VFPv5_sp`.
- **MSPM0 CUSTOM_BOARD MSP32C031 and MSP32G031 (3 builds, Ta105)**: IAR's device support does not list the MSP32 parts, so the cpu fell back to ARM7TDMI. `sdk_fixes.py` names `MSPM0C1105` (the projects link that DriverLib, a Cortex-M0+). Built only.
- **F3 post-build tools**: `crc_tool` (CRC-32 over each `*_begin`/`*_end` symbol pair, little-endian at end+1) is reimplemented in `crc_tool_linux.py` from TI's README and source. It matched TI's own `crc_tool` on 12 of 12 test images (load segments byte for byte; the ELF files differ because LIEF rewrites the container), and on every rebuilt image: TI's own `crc_tool`, run on the build host over the 591 patched ELFs, changed no byte of their load segments (591 of 591 identical). `imgtool` runs MCUboot's `imgtool.py` from the SDK with Linux wheels of its dependencies in `/Volumes/ti/pydeps-linux` on the build host; `commands_runner` and the `msoad_*` scripts run their shipped Python source.
- **Container network**: Docker's default bridge on the build host had no route out on 2026-10-24 (IAR's license check then timed out, 16 builds failed with LMSC3014). The harness now runs the container with `--network host`.

## CoreMark gap (LP-MSPM33C321A)

Same silicon and same sources, so the two compilers should score close. With one extra IAR flag they do: the remaining gap is under 8 percent against TI Arm Clang `-O3`.

| Check | Result |
|---|---|
| Clock and flash wait states, read on the chip for both builds | identical: `FRDCNTL_M33` 0x02000201 at 32 MHz and 0x02000200 at 160 MHz, `FRD_INTF_CTRL_M33` 0x2 and 0x6 |
| Optimization on every file | `-Ohs` on all eight compile units (assembly listings, `LISTS=1`) |
| Placement | code in flash at 0x10000000, data and stack in SRAM at 0x30000000 |
| cpu, FPU, iterations, timer | `--cpu=Cortex-M33 --fpu=VFPv5_sp`; 1,600 and 8k iterations (12 s and 16 s); DWT cycle counter, as for Clang |
| Cause | `-Ohs` alone keeps IAR's size constraints on. Adding `--no_size_constraints` (code 13.4 kB to 17.1 kB at 32 MHz) is the fix |

| Build | 32 MHz | 160 MHz |
|---|---|---|
| IAR `-Ohs`, defaults (before) | 87.23 | 363.29 |
| IAR `-Ohs --no_size_constraints` (after, 3 runs each, identical to 0.01) | 129.30 | 502.40 |
| TI Arm Clang `-O3` / `-O2` | 140.28 / 96.86 | 544.05 / 376.94 |

The remaining gap to Clang `-O3` is 7.8 to 7.9 percent. One flag at a time on the 129.30 build (32 MHz, 1 run each): `--no_loop_align` 128.28, `--no_scheduling` 124.19, `--no_code_motion` 122.85, `--no_unroll` 98.65, `--no_inline` 95.66, and `--mfc` (six files as one unit) 123.20 with code growing to 29 kB. None beats the plain build. `-Oh` and `-Ohz` images are byte-identical in size with and without the flag, so their rows stand.

DSLite refuses the IAR `.out` (the 276-byte vector table is its own load block; the programmer writes 128-bit words). `coremark_iar.sh` writes an Intel HEX padded with 0xFF and `cm_run.py` flashes that.

Run: `python3 tools/frl/lock.py exec --serial M3010001 -- python3 tools/toolchains/iar/cm_run.py <folder with coremark.hex>`.
