# Upstream Zephyr on the TI LP-AM13E230: build on an Intel Mac and flash with OpenOCD

Zephyr's `main` branch has a board for this LaunchPad kit and it builds from a Mac terminal, but the board file is a bring-up stub: no GPIO, serial or pin driver, no LED in the devicetree. `hello_world` builds (11.9 kB of flash, 4,000 of RAM) and prints nowhere; Zephyr's `blinky` does not build. This repo's `led-proof` app writes the registers by hand and blinks LED4 (red, PA5). Flashing needs OpenOCD built from source, which also builds on a Mac. **Built, flashed and run 2026-10-10: `led-proof` blinks LED4 and its counter rises about 4 per second; `hello_world` reaches `main` and idles.**

Errors: [FAQ](../FAQ.md). 1 kB = 1,024 bytes. Checked 2026-10-10 on an Intel Mac (macOS 15.7.9), Zephyr `main` at commit `11808e9ea` (VERSION 4.5.0, 2026-10-10), GCC 12.2.0 from Zephyr SDK 0.17.4, OpenOCD `0f08cd377` (2026-10-10).

## Build it (no board needed)

You need Zephyr 4.4.2's Python environment in `~/zephyrproject/.venv` and the cross GCC in `~/zephyr-toolchain` (the install the [NUCLEO-U083RC page](../../nucleo-u083rc/README.md) uses). Then:

```bash
./build.sh
```

The first run clones Zephyr `main` (shallow, 650 MB) into `$RUN` (the script's work folder, outside the repository; set `RUN` to move it) and fetches three modules, 90 s; each build after that is 25 to 32 s. It leaves the shared `~/zephyrproject` (Zephyr 4.4.2, which the other boards' pages use) untouched. The board is not in 4.4.2: `ls ~/zephyrproject/zephyr/boards/ti` has no `lp_am13e230`.

What the script does for one app: `west build -p always -b lp_am13e230 <app> -d <dir>` with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile` and `CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-`. Without those two variables the build stops at "Could not find a package configuration file provided by Zephyr-sdk", as on every Zephyr page here.

| App | Result | Flash | RAM | Notes |
|---|---|--:|--:|---|
| `samples/hello_world` | builds, 77 warnings | 11.9 kB (2.33 percent of 512 kB) | 3.9 kB (4.07 percent of 96 kB) | No console is chosen in the board file, so it prints nothing |
| `samples/basic/blinky` | does not build | | | `__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD undeclared`: no `led0` alias, no GPIO driver |
| `am13e230/zephyr/led-proof` (this repo's) | builds | 12.3 kB | 3.9 kB | Writes GPIO and IOMUX registers directly, toggles PA5 every 250 ms |

The 77 warnings are all `implicit declaration of function '__rbit'` from Zephyr's `include/zephyr/arch/arm/bit_rev.h`. The CMSIS header for GCC defines `__RBIT`, not `__rbit`. Nothing calls the function, so the link is clean.

## Flash it (needs the board; tested 2026-10-10)

1. **Build OpenOCD from source.** The OpenOCD in the Zephyr SDK and the xPack 0.12.0-7 build used for other boards in this repo (2025-10-04) have no AM13E230 flash driver or target file; the board's doc says to build upstream. On this Mac it took 3.5 minutes, 144 MB. Run `export RUN=<work folder>` first, using the folder `build.sh` created:

```bash
brew install autoconf automake
git clone --depth 1 https://github.com/openocd-org/openocd $RUN/openocd
cd $RUN/openocd && git submodule update --init --depth 1
./bootstrap && PKG_CONFIG_PATH=/usr/local/opt/libusb/lib/pkgconfig ./configure --enable-internal-jimtcl --enable-xds110 --disable-doxygen-html && make -j4
```

`brew install` pulled only those two formulae (autoconf 2.73 from a bottle, automake 1.19 built in 14 s). libusb and libtool were already installed. Result: `src/openocd`, "Open On-Chip Debugger 0.12.0+dev-g0f08cd3", which contains the `am13` flash driver and `tcl/target/ti/am13e230x.cfg`. Already built at that path.

2. **Make sure nothing else holds the probe.** DSLite, CCStudio and OpenOCD cannot use the XDS110 at the same time. Check `ps aux | grep -i dslite`.

3. **Flash.** Probe serial `AL130001`:

```text
SERIAL=AL130001 ./build.sh flash
```

which ends in `west flash -d build-led --runner openocd --openocd <run>/openocd/src/openocd --openocd-search <run>/openocd/tcl --cmd-pre-init "adapter serial AL130001"`. The board's `board.cmake` passes `--use-elf` and the board's `support/openocd.cfg` sets `adapter speed 10000` (10 MHz) for the XDS110. Tested result: the XDS110, the core and the flash (512 kB, 2 banks) were found at once, the image was erased and written (12.6 kB, 0.106 s) with no error, and the program was already running afterwards without a reset. `west flash` printed no verify line. For `hello_world` use `west flash -d build-hello ...` with the same options (12.2 kB).

## Prove it runs (no serial output exists)

The backchannel port `/dev/cu.usbmodemAL1300011` (115200 baud) stays silent with these images: upstream has no UART driver for the chip. Use the pin and the counter instead. `proof_count` is at `0x20000200` in this build (`arm-zephyr-eabi-nm build-led/zephyr/zephyr.elf | grep proof_count`; recheck after any change). The output register `GPIO0 DOUT31_0` is at `0x400F1280`; LED4 is bit 5 and is lit when the bit is 0. Give OpenOCD one `-c` per command and wrap each read in `echo`; a read put inside one `-c "a; b; mdw ..."` string prints nothing.

```text
OCD=$RUN/openocd
$OCD/src/openocd -s $OCD/tcl -c "adapter serial AL130001" -f interface/xds110.cfg -f target/ti/am13e230x.cfg \
  -c "adapter speed 10000" -c init -c "reset run" -c "sleep 1000" -c "echo [mdw 0x20000200]" -c "echo [mdw 0x400F1280]" \
  -c "sleep 1100" -c "echo [mdw 0x20000200]" -c "echo [mdw 0x400F1280]" -c shutdown
```

Pass: the counter rises by about 4 per second and bit 5 of the second register changes over several reads. Fail signs: `Error: ... Examination failed`, a counter stuck at 0 (the app never reached `main`), or a counter that rises but a pin that never moves (the register map is wrong). Looking at LED4 is the second proof; `am13e230/tools/led_check.sh AL130001` (DSLite) is the third.

**Tested 2026-10-10** (probe <probe serial>, 14:23 to 14:29, no one at the bench):

| Image | Flash | Proof |
|---|---|---|
| `led-proof` | OpenOCD, 0.106 s for 12.6 kB | counter 5, then 9 after 1 s, then 19 after 2 more; PA5 changed between OpenOCD reads and between DSLite reads; the counter also rose (14, 18) after a flash with no reset |
| `hello_world` | OpenOCD, 12.2 kB | prints nowhere. A hardware breakpoint on `main` (0x3d0) after `reset halt` and `resume` stopped the core there. With no breakpoint, 1.5 s after reset the core sat at 0x2592 in `arch_cpu_idle` with the link register in `idle`: kernel started, `main` returned, chip idle |

Hello_world proof commands: `-c "reset halt" -c "bp 0x3d0 2 hw" -c resume -c "sleep 500" -c "echo [reg pc]"` (find the address with `nm | grep ' main$'`).

## Restore

`./am13e230/blink.sh 250` (build 3.1 s, flash 3.6 s via DSLite), then `am13e230/tools/led_check.sh AL130001` shows PA5 changing. Tested twice on 2026-10-10: restored and verified by pin reads. OpenOCD's `program` writes only main flash and never touches the NONMAIN boot region. A stepper wired to J4 loses its coil drive while another image is in flash.

## What Zephyr supports for this board today

From the board file and the SoC files at the commit above: Cortex-M33 core with SysTick, the MPU, the interrupt controller (48 lines) and the flash map (512 kB at 0, 96 kB of RAM at 0x20000000, two more 32 kB RAM regions). The CPU clock is declared as 32 MHz, not 200 MHz, and no clock-control driver exists, so the chip runs on its reset clock. The pin controller node reuses the MSPM0 compatible string (`ti,mspm0-pinctrl`) and nothing enables a driver for it. Nothing else: no GPIO, UART, timer, ADC, MCPWM, TMU or NPU support. The board doc's list of supported features is generated from the board's YAML, which names none.
