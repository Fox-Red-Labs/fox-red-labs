# Assistant notes for porting code to the TI MSPM33 family (LP-MSPM33C321A)

MSPM0 DriverLib calls for GPIO, SysTick, timers, PWM, SPI, UART and CRC work here unchanged once the `.syscfg` is retargeted. I2C (UNICOMM) and the ADC (HSADC) do not, and there is no TI Drivers layer. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [MSPM33 family FAQ](FAQ.md). **DriverLib** is TI's register-level C library; **SysConfig** writes pin and clock code from a `.syscfg` file; **UNICOMM** is the combined serial block; **HSADC** is the high-speed ADC. 1 kB = 1,024 bytes. Checked 2026-10-25 with MSPM33 SDK 1.04.00.00, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0, on one LP-MSPM33C321A.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The part

| Fact | Value | Source |
|---|---|---|
| Chip | MSPM33C321A, Cortex-M33 up to 160 MHz, FPU, TrustZone, 1 MB flash, 256 kB RAM | `mspm33c321a/AGENTS.md` |
| Code address | Flash at 0x10000000; boot configuration at 0x80101800, never written | `mspm33c321a/AGENTS.md` |
| LEDs | blue PA2, red PC26, green PC27, three ports | `mspm33c321a/AGENTS.md` |
| TI driver layers | DriverLib only; no TI Drivers | `portability/matrix.csv` X6 |
| Blocks that differ from MSPM0 | I2C is UNICOMM, the ADC is HSADC, no `DL_INTERRUPT_GROUP_1` alias, one SysConfig DMA instance per channel | `portability/everywhere/PORTING.md` 7.1, `portability/matrix.csv` X29 |

## Start here

| Step | Command or file |
|---|---|
| Port one MSPM0G3519 example | `python3 portability/everywhere/crossport_pair.py m0_to_m33 <work dir> <results.jsonl> <example>` |
| Port many | `python3 portability/everywhere/crossport_mspm0_mspm33.py <work dir> <results.jsonl> [FROM board] [category ...]` |
| Back up | the DSLite reads in `mspm33c321a/AGENTS.md` "Start here", each range twice |
| Check the image | `python3 tools/preflight_image.py mspm33c321a <image.out>` |
| Flash and read | `python3 portability/everywhere/bench_pair_run.py m33c321a <image.out> <test> <results.jsonl>` |

## Rules for the assistant

1. **Carry the source `.syscfg` and swap its first three comment lines** to `MSPM33C321A`, `LQFP-100(PFA)` and `/ti/boards/LP_MSPM33C321A`; comment out `.$assign` pin lines and the `SYSCTL` clock-tree lines; add a `ProjectConfig` block if missing (`portability/everywhere/PORTING.md` 7.1).
2. **Do not rename I2C calls; rewrite them** for UNICOMM (`portability/everywhere/PORTING.md` 7.2).
3. **Rewrite ADC code for the HSADC**; the shared three-ways app did it in one function (`portability/three-ways/README.md`).
4. **Use internal loopback for UART tests without a wire:** `UART1.enableInternalLoopback = true;` (`portability/everywhere/PORTING.md` 7.1).
5. **Do not carry MSPM0 standby code.** It bus faulted twice here (`portability/matrix.csv` X28, X29).
6. **Check the link address is 0x10000000** before flashing (`mspm33c321a/AGENTS.md`).
7. **For FreeRTOS from an interrupt, set the priority with `NVIC_SetPriority(irq, 0x40u >> (8 - __NVIC_PRIO_BITS))`** (`mspm33c321a/AGENTS.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| MSPM0 DriverLib examples, configuration port | 43 of 83 build, 40 ran, 39 clean | `portability/matrix.csv` X29 |
| MSPM0G3519 examples, both ways | 8 of 12 built untouched, 7 checked here | X51 |
| Shared three-ways app | serial check passed; `2,816` B of flash | X53 |
| ML-KEM-768 and ML-DSA-44 from the CC1354P10 | 6 of 6 known-answer steps | X39 |

## Next

The family guide is `portability/families/mspm33/README.md`; every port and test on record is `portability/families/mspm33/EVIDENCE.md`; the board bring-up is `mspm33c321a/AGENTS.md`.
