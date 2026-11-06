# Assistant notes for porting code to or across the TI MSPM0 family (LP-MSPM0C1104, LP-MSPM0L1306, LP-MSPM0G3507, LP-MSPM0G3519, LP-MSPM0G5187)

DriverLib calls are the same across the family. SysConfig names, pins, interrupt lines and RAM are not, and RAM decides which layer fits. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [MSPM0 family FAQ](FAQ.md). **SysConfig** writes a chip's pin and clock code from a `.syscfg` file; **DriverLib** is TI's register-level C library; **TI Drivers** is TI's portable driver API; **NONMAIN** is the flash area that holds boot settings; **UNICOMM** is the combined serial block on the G5187. 1 kB = 1,024 bytes. Checked 2026-10-25 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.26.3 and 1.28.0.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The family

| Part | Flash, RAM | What bites a port | Source |
|---|---|---|---|
| MSPM0C1104 | 16 kB, 1 kB | SysTick does nothing (use TIMG8); one DMA channel, no UART trigger; no ROM bootloader | `mspm0c1104/AGENTS.md`, `site/data/ports.json` |
| MSPM0L1306 | 64 kB, 4 kB | Interrupt lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25, 31 only; no PB pins, SPI1, ADC1, AES, DAC12 or RTC | `mspm0l1306/FAQ.md`, `portability/matrix.csv` X15 |
| MSPM0G3507 | 128 kB, 32 kB | The reference most TI examples are written for | `mspm0g3507/AGENTS.md` |
| MSPM0G3519 | 512 kB, 128 kB | Zephyr's shared pin file is wrong for 136 of 443 functions; pulse nRST after a DSLite flash | `portability/arduino/zephyr/BOARD-mspm0g3519.md`, `portability/arduino/zephyr/FAQ.md` |
| MSPM0G5187 | 128 kB, 32 kB | UART is `UC0`; no MCAN or DAC12; Zephyr main has no board | `docs/guides/chip-capability-map.md`, `portability/everywhere/PORTING.md`, `portability/zephyr-mspm0/README.md` |

## Start here

| Step | Command or file |
|---|---|
| Will it move? | `python3 portability/everywhere/inventory.py` then `python3 portability/everywhere/feasibility.py`, run from `portability/everywhere/`; look the example up in `data/map.jsonl` (`portability/everywhere/PORTING.md` section 1). Treat Should work as an upper bound |
| Retarget one example | Copy the example, change the device, package and board lines in its `.syscfg`, drop each line SysConfig rejects, build, and write down every dropped line and moved pin |
| Back up | `python3 tools/benchmark/backup.py save <board> --probe <serial>` (main flash and NONMAIN read twice; NONMAIN never written back) (`mspm0l1306/AGENTS.md`) |
| Check the image | `python3 tools/preflight_image.py <board> <image.out>` |
| Check a blink | Read the LED pin's GPIO output register (DOUT) a few times through the XDS110 probe with OpenOCD; the bit changes when the pin toggles |
| Restore | `python3 tools/benchmark/backup.py restore <backup folder> --probe <serial>` |
| Record | a row per number in `data/results/<board>.csv`, then `python3 tools/results/check.py` |

## Rules for the assistant

1. **Carry the `.syscfg` with the app.** Swap the device, package and board lines, unlock pin assignments, and let SysConfig reject what the target lacks (`portability/everywhere/PORTING.md` section 2).
2. **List every dropped SysConfig line before flashing.** 13 of 96 G3507-to-L1306 builds lost a real setting without an error (`portability/matrix.csv` X15).
3. **Delete copied `ti_msp_dl_config.c` and `.h` files** from a ported project; with them deleted, 79 of 122 G3519 examples build for the G5187 (`portability/matrix.csv` X30).
4. **Check pin functions against SysConfig's device data**, not a shared pin file (`portability/arduino/zephyr/BOARD-mspm0g3519.md`).
5. **Check the interrupt line exists** on the L1306 before using it (`mspm0l1306/FAQ.md`).
6. **After any clock change, read `CPUCLK_FREQ`** in the generated `ti_msp_dl_config.h` (`mspm0l1306/AGENTS.md`).
7. **Do not run unknown ported images on the C1104 without a plan.** Three ported images locked it; recovery is a factory reset that erases main flash, so ask the board's owner first (`mspm0c1104/FAQ.md`).
8. **Name the G5187's UART `UC0`** in handlers (`docs/guides/chip-capability-map.md`).

## Which layer fits

| RAM | Part | What ran or fit | Source |
|---|---|---|---|
| 1 kB | C1104 | DriverLib: shared app 184 B of RAM; UART packet port 632 B | `portability/matrix.csv` X53, `site/data/ports.json` |
| 4 kB | L1306 | DriverLib; FreeRTOS one-task blink 0.98 kB with static allocation; Zephyr hello_world 3.7 kB; Rust blinky 112 B | `mspm0l1306/AGENTS.md`, `ports/rust-mspm0/README.md` |
| 32 kB | G3507, G5187 | TI Drivers on FreeRTOS on the G3507; MicroPython minimal 72.9 kB of flash on the G3507 (built only) | `portability/three-ways/README.md`, `ports/micropython/README.md` |
| 128 kB | G3519 | Arduino on Zephyr, MicroPython full (175.6 kB flash), Renesas FSP layer, Pico SDK layer, TI Drivers twins | `portability/families/mspm0/README.md` driver and function map |

## Moving to other families

MSPM33: `portability/everywhere/crossport_mspm0_mspm33.py`; I2C and ADC need a rewrite (UNICOMM, HSADC) (`portability/families/mspm33/AGENTS.md`). AM13: `portability/everywhere/crossport_mspm0_am13.py` (`portability/families/am13/AGENTS.md`). CC23xx and CC27xx: TI Drivers twins through `portability/shim/` (`portability/mspm0-to-wireless.md`).

## Next

The family guide is `portability/families/mspm0/README.md`; every port and test on record is `portability/families/mspm0/EVIDENCE.md`. Board bring-up is in each board folder's `AGENTS.md`.
