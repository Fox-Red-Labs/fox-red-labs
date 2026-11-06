<!-- experiment
id: E36
board: LP-AM13E230
date: 2026-10-10
status: works
reached: verified
line: Upstream Zephyr main runs on the LP-AM13E230 from an Intel Mac, flashed by OpenOCD built from source in 0.1 s of writing (12.6 kB): this repo's led-proof image toggled LED4 (PA5) and its counter rose about 4 per second, and hello_world reached main and then the idle loop; but Zephyr's own blinky does not build, because the board has no LED, GPIO or serial driver, and nothing prints.
stop: no-software
-->
# Does upstream Zephyr run on the TI LP-AM13E230 from a Mac, flashed by OpenOCD built from source?

**Short answer.** Yes, with limits. Zephyr's `main` branch has a board for the LP-AM13E230, and from an Intel Mac it was built, flashed with OpenOCD built from source, and watched running. This repo's `led-proof` app (it writes the GPIO registers by hand) blinked LED4 and its counter rose by 4 per second. `hello_world` booted the kernel, reached `main` and went idle, but prints nothing, because the chip has no serial driver in Zephyr. Zephyr's own `blinky` does not build: the board file has no LED and no GPIO driver. OpenOCD built in 3.5 minutes; programming 12.6 kB took 0.1 s.

Terms: [vocab](../vocab.md) (OpenOCD, devicetree, XDS110). Recipe and commands: [am13e230/zephyr/README.md](../../am13e230/zephyr/README.md). Errors and fixes: [LP-AM13E230 FAQ](../../am13e230/FAQ.md). 1 kB = 1,024 bytes.

**Question.** Can a builder follow Zephyr's own board page for the LP-AM13E230, from a Mac, to a built image and a flash command?

**Vendor position.** Zephyr's board page for `lp_am13e230` (`boards/ti/lp_am13e230/doc/index.rst`, `main` on 2026-10-10) calls it "a development board based on the AM13E230x SoC, a 200MHz ARM Cortex M33 real-time motor control MCU with edge AI, TMU and 512KB on-chip flash". On flashing it says: "Support for AM13E230x has been merged in upstream OpenOCD but not picked up by Zephyr's **sdk-ng**. Until the Zephyr SDK is updated to use a commit of OpenOCD that includes support for this SoC, the user has to clone and build upstream OpenOCD separately and point `west flash` to that directory." Its steps: clone `openocd-org/openocd`, `git submodule update --init`, `./bootstrap && ./configure --enable-internal-jimtcl --enable-xds110`, `make`, then `west flash --openocd <path>/src/openocd --openocd-search <path>/tcl`. The page does not name a toolchain, a Zephyr SDK version or any known limit. Its supported-features table is generated from the board's YAML, which lists none.

**What was tried.** 2026-10-10, Intel Mac with macOS 15.7.9, 4 GB of free disk. The Zephyr 4.4.2 checkout used for the other boards has no such board, and updating it would change the build their pages document, so a separate workspace was made: a shallow clone of Zephyr `main` (commit `11808e9ea`, VERSION 4.5.0) plus the three modules the board needs (`hal_ti`, `cmsis`, `cmsis_6`, fetched with `west update --narrow`), 830 MB in all, in a scratch folder outside this repo. Same toolchain route that worked on the NUCLEO-U083RC: Zephyr SDK 0.17.4's GCC 12.2.0 with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`. Builds of `samples/basic/blinky`, `samples/hello_world` and this repo's `am13e230/zephyr/led-proof`. Then upstream OpenOCD (`0f08cd377`) built from git with `--enable-xds110`. No board, probe or serial port was opened.

**How far it got.** Configure and compile pass for all three apps. Link passes for `hello_world` and `led-proof`; `blinky` fails to compile. Both linked images flashed with `west flash` through the new OpenOCD and ran (details in "Flash and run", below).

| App | Result | Flash | RAM |
|---|---|--:|--:|
| `samples/hello_world` | links, 32 s | 12.2 kB (2.33 percent of 512 kB) | 4.0 kB (4.07 percent of 96 kB) |
| `samples/basic/blinky` | compile error | | |
| `led-proof` (this repo's, direct register writes to PA5) | links, 25 s | 12.6 kB | 4.0 kB |

From the map: `hello_world` is 9.9 kB of code, 1.6 kB of read-only data, 256 bytes of vector table, 384 of interrupt table; RAM is 3.4 kB of stacks, 575 bytes of zeroed data and 32 of initialised data. OpenOCD: autoconf and automake installed with `brew` (23 s), `./bootstrap` 13 s, `./configure` 27 s, `make -j4` 79 s; the result prints "Open On-Chip Debugger 0.12.0+dev-g0f08cd3" and contains the `am13` flash driver and `target/ti/am13e230x.cfg`. The xPack OpenOCD 0.12.0-7 used for other boards (built 2025-10-04) has neither.

**Flash and run.** 2026-10-10, one probe, no wiring or board change.

- `SERIAL=AL130001 ./build.sh flash` (rebuild of all three apps, 37 s, then flash): OpenOCD found the XDS110 (firmware 3.0.0.43), the Cortex-M33 and the flash ("AM13: MAIN flash: 512 KB (256 sectors), 2 bank(s)"), auto-erased and wrote 12.6 kB in 0.106 s. The whole command took 2 min 12 s, nearly all of it the rebuild. No error. `west flash` printed no separate verify line.
- Run, `led-proof`: after `reset run`, `proof_count` (0x20000200) read 5, then 9 one second later, then 19 two seconds after that (about 4 per second, as a 250 ms toggle gives). Bit 5 of GPIO0 DOUT (0x400F1280) read 0, 0x20, 0x20, 0, 0 in five quick reads, so the pin moves. DSLite pin reads showed 0x20 and 0 alternating. A plain `west flash` with no reset leaves the program running: the counter was 14, then 18 one second later.
- Run, `hello_world` (12.2 kB written): it prints nowhere, so the core was read. With a hardware breakpoint on `main` (0x3d0) the core stopped there after a reset, so `main` is reached. Without the breakpoint, 1.5 s after reset the core was at 0x2592, inside `arch_cpu_idle`, with the link register in `idle`: the kernel booted, `main` ran and returned, and the chip sleeps in the idle thread.
- Restore: `./am13e230/blink.sh 250` (build 3.1 s, flash 3.6 s); pin reads afterwards alternated 0 and 0x20. The restore ran twice, the second after one extra flash of `led-proof`. The stepper wired to J4 lost its coil drive while the Zephyr images ran, as expected.
- Which tool proved it: OpenOCD flashed and read; DSLite read the pin independently.
- OpenOCD note: `mdw` returns its text instead of printing it, so `-c "init; ...; mdw ..."` prints nothing. Use one `-c` per command with `echo [mdw ...]`.

**The line.** Where it stops, in order:

1. `blinky`: `error: '__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here`. The board file `lp_am13e230.dts` defines memory and the pin-control node only: no `led0` alias, no GPIO node, no serial node, and no `zephyr,console` choice. Zephyr has no AM13 GPIO or UART driver at this commit (the pin-control node reuses the MSPM0 compatible string `ti,mspm0-pinctrl` and nothing enables a driver for it).
2. `hello_world` builds and would run, but its output has nowhere to go; the backchannel port `/dev/cu.usbmodemAL1300011` stays silent. A run can only be proven with the debugger.
3. The declared CPU clock is 32 MHz and there is no clock driver, so the chip stays on its reset clock; the board file's 32 MHz is what Zephyr's timing would assume, against the 200 MHz TI's examples run at. The clock the chip really starts on was not checked.
4. 77 warnings, all `implicit declaration of function '__rbit'`: Zephyr `main`'s `bit_rev.h` calls a function the CMSIS header for GCC does not define (it defines `__RBIT`). Nothing calls it, so the link is clean. `main` wants Zephyr SDK 1.0.1 (`SDK_VERSION`); 0.17.4 was used.

The cause of 1 is missing software, not this repo's toolchain: the board is a bring-up stub.

**Ask.** If you have run Zephyr on the LP-AM13E230, say which commit, which toolchain and how you saw it run (LED, debugger or serial).
