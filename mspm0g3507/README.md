# LP-MSPM0G3507 LaunchPad: first blink with an AI assistant in 2 min 29 s, and what else it measures

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

Let Claude Code do it: paste the prompt in `START.md`, or run `./start.sh mspm0g3507 blink` from a clone. Either one has the assistant build TI's blink example for the LP-MSPM0G3507, flash it, and change the pattern to what you ask for. The folder is `mspm0g3507/`.

Measured on one borrowed LP-MSPM0G3507 (not a sealed box, software already installed): the blink was running 2 min 29 s after the board was in hand, with an agent driving CCStudio, about 1 minute of hands-on and 2 of waiting (`data/results/mspm0g3507.csv`, test `time_to_blink`). That is one run of the CCStudio route (`./start.sh ... --ccs`). The default terminal route has no time on this board. `START.md` and `start.sh` have not been timed on this board; `start.sh` was timed on an LP-MSPM0L1306 (1 min 15 s).

Terms: [vocab](../docs/vocab.md) and the [FAQ](FAQ.md). **CCStudio** is TI's IDE; **XDS110** is the debug probe on the board; **SDK** is TI's code library for the chip; **Claude Code** is Anthropic's coding assistant; **DSLite** is TI's command-line flasher; **Zephyr** is an open-source real-time operating system; **Rosetta 2** is Apple's translator for Intel apps on Apple Silicon; **NONMAIN** is the flash area that holds the chip's boot settings; **CoreMark** is a standard CPU speed test. 1 kB = 1,024 bytes.

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix). Folders: `arduino-core-test/` Arduino core tests, `display-test/` display tests, `lvgl-g3507/` LVGL on this board, `mcu-bench/` the shared benchmark suite, `src-g3507/` the blink example's `.c` and `.syscfg` for this board with TI's license headers kept (`src/` holds the LP-MSPM0C1104 version), `stm32-port-eval/` ST examples moved to this chip, `third-party-bench/` third-party benchmarks, `tools/` helper scripts for this board, `zephyr/` Zephyr builds. Results are rows in `data/results/mspm0g3507.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add mspm0g3507 tools data third-party/lvgl
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact | Read from |
|---|---|---|
| Chip | MSPM0G3507: Arm Cortex-M0+, 80 MHz, 128 kB flash, 32 kB RAM | `data/boards.csv` |
| Kit | LP-MSPM0G3507 LaunchPad | `data/boards.csv` |
| Debugger | XDS110 on the board; a Mac sees a probe and two serial ports with no driver (macOS 15, Intel); probe serial <probe serial> | `AGENTS.md` |
| LEDs | An RGB LED; TI's blink already sets up all three colors | `AGENTS.md` |
| Serial | UART0 on PA10 and PA11 through the probe, 115200 8N1 | `AGENTS.md` |
| Not on the chip | No floating-point unit and no neural accelerator | `AGENTS.md` |

## Ten-minute path

You need an LP-MSPM0G3507 LaunchPad, a USB data cable (a charge-only cable looks like a dead probe), a Mac, and a Claude subscription. TI's tools take about 30 minutes to install once and about 13 GB of disk: CCStudio about 10 GB and the MSPM0 SDK about 2.6 GB (`mac-setup/README.md`). Measured once, with the tools already installed: 2 min 29 s (see What was measured).

1. **Pick a route (browser or terminal).**
   - No terminal: paste the one prompt from `START.md` (at the repository root) into Claude Code. It fetches only this board's folder, then asks before installing anything or writing to the board.
   - Terminal, from the repository root:

     ```
     ./start.sh mspm0g3507 blink
     ```

     Add `--probe <serial>` if more than one board is plugged in, `--ccs` to have the assistant use CCStudio's servers, `--dry-run` to print what would run. `./start.sh` with no arguments prints its options.
2. **Install the tools once (browser).** CCStudio 21.0.1 and the MSPM0 SDK 2.11 from TI's download pages, as your user (not sudo). Versions and disk sizes are in `mac-setup/README.md`. All runs were on an Intel Mac, where TI's tools run natively. Apple Silicon is untested here; TI's tools are x86_64, so expect to need Rosetta 2:

   ```
   softwareupdate --install-rosetta --agree-to-license
   ```
3. **Plug in the board (bench, then terminal).** You should see the on-board XDS110 as a probe and two serial ports, with no driver. If the Mac sees nothing, try another cable: a charge-only cable looks the same (`FAQ.md`). This prints the probe serial:

   ```
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```
4. **Ask for what you want (Claude Code).** "Make the red LED flash twice, then pause." The assistant builds TI's `gpio_toggle_output` for this board, flashes it, and tells you what to look for.
5. **Check it (bench).** Look at the LED; that is the check that has been done on this board. Or read the pin through the debugger. Its pin list comes from TI's blink example and it has not been run on this board:

   ```
   python3 tools/check/blink.py --board mspm0g3507 --probe <serial>
   ```

## One-command build

The default route is `./start.sh mspm0g3507 blink` (step 1 above). It builds TI's blink example for this board, flashes it and changes the pattern to what you ask for. `START.md` and `start.sh` have not been timed on this board; `start.sh` was timed on an LP-MSPM0L1306 (1 min 15 s). The default terminal route has no time on this board; the one timed run used the CCStudio route (`./start.sh ... --ccs`).

The commands the assistant uses are in `AGENTS.md`, section "Start here" (the board's tools, probe serial, build, flash, check). To run the steps yourself:

- **TI's blink with CCStudio's tools:** copy `/Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0G3507/driverlib/gpio_toggle_output`, then:

  ```
  mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
  ```

  In CCStudio with Claude Code driving, build took 6 s and flash plus start 38 s.
- **Zephyr instead:** `mspm0g3507/zephyr/setup-intel-mac.sh` installs Zephyr 4.4.2, builds blinky and flashes it. 3 min 0 s and 3 min 6 s from clean on two runs; the first, unscripted run took about 9.5 minutes (`zephyr/README.md`). Intel Mac, no Homebrew needed.
- **Back up first.** There is no tested backup command for this board. Read the whole flash with DSLite (`/Applications/ti/uniflash_9.6.0/dslite.sh`) before the first write. Do not write NONMAIN.

It does not back up or restore the board, and no restore has been tested.

## What was measured

One borrowed LP-MSPM0G3507 (not a sealed box), Intel Mac, macOS 15.7.9.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Board in hand to a changed blink, CCStudio route | 2 min 29 s (about 1 min hands-on, 2 waiting) | 1 run; software already installed; agent in VS Code driving CCStudio | measured | `data/results/mspm0g3507.csv`, test `time_to_blink` (ccs-mcp) |
| Nothing installed to Zephyr blinking | about 9.5 min | 1 run, one prompt, includes finding the toolchain workaround | measured | `data/results/mspm0g3507.csv`, test `time_to_blink` (zephyr) |
| Zephyr from clean with the saved script | 3 min 0 s and 3 min 6 s | 2 clean runs of `zephyr/setup-intel-mac.sh` | measured | `data/results/mspm0g3507.csv`, test `time_to_blink` (zephyr-script) |
| Build of `gpio_toggle_output` | 6 s | CCStudio MCP build | measured | `data/results/mspm0g3507.csv`, test `blink_build` (ccs-mcp) |
| Flash and start | 38 s | CCStudio route | measured | `data/results/mspm0g3507.csv`, test `blink_flash` (ccs-mcp) |
| Zephyr blinky build | 62 s | `west build`, 1,102 build steps | measured | `data/results/mspm0g3507.csv`, test `blink_build` (zephyr) |
| Zephyr blinky flash | 6 s | `west flash` with xPack OpenOCD, 11.7 kB written | measured | `data/results/mspm0g3507.csv`, test `blink_flash` (west) |
| Zephyr blinky size | 11.7 kB flash, 3.9 kB RAM | Zephyr 4.4.2 | measured | `data/results/mspm0g3507.csv`, tests `blink_flash_bytes`, `blink_ram_bytes` |
| CoreMark | 183.61 (2.3 per MHz) | bare metal, TI Arm Clang 5.1.1, `-O3`, 80 MHz; repeat run 183.67 | measured | `data/results/mspm0g3507.csv`, test `coremark` (ticlang-bare-o3) |
| Probe on the board | yes, no driver install | probe <probe serial> | measured | `data/results/mspm0g3507.csv`, test `probe_included` |
| Chip: 80 MHz, 128 kB flash, 32 kB RAM | as listed | vendor figures | claim | `data/boards.csv` |

The rest of this board's benchmark rows (interrupt latency, GPIO toggle, math accelerator, AES, CRC, ADC rate, Embench) are in `data/results/mspm0g3507.csv`.

## If you see this

- If you see `fatal error: 'main.h' file not found` (ported STM32 project), add `-I"${PROJECT_ROOT}/Inc"`: the initializer does not put `Inc/` on the include path.
- If you see `error: unknown type name 'uint32_t'` in `Src/system_stm32g0xx.c`, add `#include <stdint.h>`: the port removed ST's header that pulled in `stdint.h`.
- If you see `make` in `Debug/` stop with `invalid option -- O`, use `ccs/utils/bin/gmake -B all` from CCStudio: Apple's `make` is too old for the generated makefile.

The rest, with causes, are in [FAQ.md](FAQ.md); then `AGENTS.md`, then `TRAPS.md` at the top of the repository.

## Limits

- The default terminal route (`./start.sh mspm0g3507 blink`) has no time on this board.
- `tools/check/blink.py` has not been run on this board.
- No backup or restore command is tested for this board.
- The Path A run (CCStudio with Claude Code inside it) is still to do.

## Where these facts come from

Checked 2026-10-05 and 2026-10-06 on an Intel MacBook (Core i7-8750H, 16 GB), macOS 15.7.9, on one borrowed LP-MSPM0G3507 (probe <probe serial>). Tools: CCStudio 21.0.1, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, Zephyr 4.4.2, DSLite from UniFlash 9.6.0.

Sources: `data/results/mspm0g3507.csv`, `AGENTS.md`, `FAQ.md`, `mac-setup/README.md`.

Public sources:

- CCStudio AI chapter: <https://software-dl.ti.com/ccs/esd/documents/users_guide/index_ai.html>
- CCStudio download: <https://www.ti.com/tool/download/CCSTUDIO>
- MSPM0 SDK download: <https://www.ti.com/tool/download/MSPM0-SDK>
- MSPM0 SDK on GitHub: <https://github.com/TexasInstruments/mspm0-sdk>
