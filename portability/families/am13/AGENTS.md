# AM13 family porting notes for AI coding assistants (LP-AM13E230)

MSPM0 DriverLib app code runs here unchanged through the AM13 DriverLib's redirects once the `.syscfg` is translated and an alias header is force-included, that the STM32G4 HAL layer covers only GPIO and UART, and that the TMU must be switched on before any code uses it. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [AM13 family FAQ](FAQ.md). **DriverLib** is TI's register-level C library; **SysConfig** writes pin and clock code from a `.syscfg` file; **MCPWM** is the motor PWM unit; **TMU** is the trigonometric math unit; **NPU** is the neural processing unit. 1 kB = 1,024 bytes. Checked 2026-10-25 with AM13E2x SDK 26.01.00.03.STS, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0, on one revision E2 LP-AM13E230.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The part

| Fact | Value | Source |
|---|---|---|
| Chip | AM13E23019, Cortex-M33 at 200 MHz, FPU, TMU, NPU, 512 kB flash, 128 kB RAM | `am13e230/AGENTS.md` |
| Motor and analog | five MCPWM modules, three 12-bit ADCs at 7.81 million samples a second, PGAs, fast comparators | `am13e230/AGENTS.md` |
| Revision | `MCU178E2` on the back is E2 (what ran); E1 has different pins | `am13e230/AGENTS.md` |
| LED, serial | LED4 red on PA5; backchannel UART at 115200 | `am13e230/AGENTS.md` |
| TI driver layers | AM13 DriverLib (with MSPM0 name redirects) and an STM32G4 HAL layer; no TI Drivers | `portability/matrix.csv` X8, X9, `portability/README.md` |

## Start here

| Step | Command or file |
|---|---|
| Port MSPM0 examples | `python3 portability/everywhere/crossport_mspm0_am13.py <work dir> <results.jsonl> [--run] [--only <regex>] [FROM board]` |
| Back up | main flash 0x00000000, 0x20000 words; NONMAIN 0x60100000, 8 kB, read only; each read twice with `DSLite memory -c <ccxml> -r <addr>,<words> -o <file>` (`am13e230/AGENTS.md`) |
| Check the image | `python3 tools/preflight_image.py am13e230 <image.out>` |
| Flash and start | `DSLite flash -c <ccxml> -e -f -v -r 1 -u -t 60 <image>.out` |
| Check a blink | `am13e230/tools/led_check.sh <serial>` |
| Build the shared app | `portability/three-ways/driverlib/build_am13.sh` |

## Rules for the assistant

1. **Translate the `.syscfg` module by module** (`/ti/driverlib/GPIO` to `/drivers/gpio`), and read every `// PORT-DROPPED:` line (`portability/everywhere/PORTING.md` section 2).
2. **Force-include the generated `board_pins.h`** for interrupt names, the group `_PORT` macro and the `RX` alias; renames only, no logic (`portability/everywhere/PORTING.md` section 3).
3. **For timer PWM, copy the MSPM0 generated init** and drop `isTimerWithFourCC`; AM13 SysConfig has no timer-PWM module (`portability/matrix.csv` X8).
4. **Turn the TMU on in `_system_pre_init`** before any code that uses it (`am13e230/AGENTS.md`).
5. **Do not trust `HAL_Delay` in the STM32G4 layer**; it counts CPU cycles (`portability/matrix.csv` X9).
6. **Every sleep image keeps a GPIO wake source** and an awake window at boot (`am13e230/AGENTS.md` trap 9).
7. **Never halt the core while a motor runs** (`am13e230/foc-drv8376/README.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| Three MSPM0 programs (GPIO, UART, timer PWM) | ran, 53 app lines unchanged | `portability/matrix.csv` X8 |
| MSPM0G3519 examples, configuration port | 20 of 43 build, 14 ran clean | X31 |
| ST's GPIO and UART examples | ran with 3 and 7 lines added | X9 |
| A C2000-style current-loop step | 1.33 µs trigger to new PWM | X49 |
| Shared three-ways app | serial check passed; `9,376` B of flash | X53 |

## Next

The family guide is `portability/families/am13/README.md`; every port and test on record is `portability/families/am13/EVIDENCE.md`; the board bring-up is `am13e230/AGENTS.md`.
