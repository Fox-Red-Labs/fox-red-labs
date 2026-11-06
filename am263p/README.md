# TI LP-AM263P LaunchPad development kit: from the box to a running program on a Mac

**Plug in two USB cables, build TI's I2C LED example with the SDK's makefile (about 6 seconds), and load it into RAM with DSLite.** The eight green LEDs next to the Ethernet ports chase each other, and the program's message appears on the board's serial port at 115200 baud, if you open that port before the load. The R5F core runs CoreMark at 1,464 per second at 400 MHz (3.66 per MHz); the other numbers are in [RESULTS-am263p.md](RESULTS-am263p.md).

Terms: 1 kB = 1,024 bytes. **R5F** is the chip's Arm Cortex-R5F real-time core (four of them). **DSLite** is the command-line loader inside CCStudio. **OCSRAM** is the on-chip RAM the SDK calls OCRAM (the data sheet lists 3 MB; a program that used above 2 MB did not start on this board, so 2 MB is what was usable, see [RESULTS-am263p.md](RESULTS-am263p.md)). **TMU** is the math unit that does sine and cosine in a few cycles. Problems and fixes: [FAQ](FAQ.md).

Checked 2026-10-09 on an Intel Mac (macOS 15.7.9), LP-AM263P Rev A (PROC171A), MCU+ SDK for AM263Px 26.01.00.08, CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, XDS110 probe firmware 3.0.0.29.

## The board

AM263P4 (a quad-core Cortex-R5F at up to 400 MHz), 32 MB of external flash, 3 MB of on-chip RAM listed in the data sheet (2 MB usable on this board, see Terms), 5 fast ADCs, 32 PWM modules, two gigabit Ethernet ports, and an XDS110 debug probe built in. There is no flash inside the chip: it boots from the external flash, or you load code into RAM from the debugger.

The kit ships without a USB-C power supply. The board is powered from its USB-C port (5 V at 3 A: a phone charger or a Thunderbolt port; a plain Type-A to Type-C cable does not carry enough). The micro-B cable only feeds the debug probe. With a weak power source only the 3.3 V test rail comes up and the red `SAFETY_ERROR` LED lights.

## Ten-minute path (build 5.9 s, load 4.4 to 7.6 s)

You need: the board, its micro-B cable, a 5 V / 3 A USB-C supply, CCStudio 21.0.1 (with TI Arm Clang and DSLite), SysConfig 1.28.0, and the AM263Px MCU+ SDK in `~/ti`.

1. Plug in both cables. The probe shows up as two serial ports, `/dev/cu.usbmodem<serial>1` and `...4`. The board's UART0 console is the first one (`...1`).
2. Read the chip, read-only: `am263p/tools/list_cores.sh <serial>` lists four R5F cores, the security core and two PRU cores. Read `0x50D80038` and bits 31 to 13 are `0x3088D`, which is the AM263P4.
3. Build TI's example:

```
cd ~/ti/mcu_plus_sdk_am263px_26_01_00_08/examples/drivers/i2c/i2c_led_blink/am263px-lp/r5fss0-0_nortos/ti-arm-clang
```

```
make i2c_led_blink.release.out DEVICE=am263px PROFILE=release TOOLS_PATH=$HOME/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0
```

   The SDK ships its libraries prebuilt. A full make also tries to build a `.mcelf` boot image and stops with `No module named 'construct'`; you do not need that image for a RAM load.
4. Open the serial port, then load. In one terminal window start a reader on the console port, then in another run DSLite with a connection file that carries your probe's serial number (`am263p/AM263Px_ZCZ_C_S26E0426.ccxml`; replace `S26E0426`, twice):

```
python3 -c "import serial; s=serial.Serial('/dev/cu.usbmodemS26E04261',115200,timeout=1); [print(s.readline().decode(errors='replace'),end='') for _ in range(60)]"
```

```
/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite load -c am263p/AM263Px_ZCZ_C_S26E0426.ccxml -t 120 i2c_led_blink.release.out
```

5. The load takes 5 to 8 seconds and ends with `Success`. The reader prints `I2C LED Blink Test Started ...`. The program leaves all eight green LEDs on, then turns them off one at a time and on again, ten times (about 16 seconds), then switches them off.

The finish line is a change you make: edit `delayMsec` in `examples/drivers/i2c/i2c_led_blink/i2c_led_blink.c`, rebuild, load again, and the chase runs faster or slower.

## What the LEDs and switches do

The eight LEDs are on an I2C expander (TPIC2810, address 0x61), not on chip pins, so there is no GPIO blink example for this board in the SDK. The boot switch SW1 stayed as shipped for every load here; by the photo (`img/top.jpg`) switches 1 and 2 sit on the ON side and 3 and 4 on the other, which is the OSPI boot setting. The SDK's EVM setup page shows the settings for each boot mode as pictures; RAM loads from the debugger worked without changing it.

## Before you load other examples

Do not load `ospi_flash_io`, `ospi_flash_xip`, `ospi_flash_diag` or any `sbl_*` example until you have read the board's 32 MB flash twice and compared the copies. The first three erase and write the block at flash offset `0x80000`; the boot image sits at `0x0` to `0x30000`. A backup that worked here: a copy of `ospi_flash_io` with every erase, write and test removed, loaded into RAM to set up the flash window, then `DSLite memory` reads of the 32 MB in eight 4 MB pieces, twice, and the two files compared.

## One-command build

There is no one-command route for this board. Use the ten-minute path above: one `make` line for the build, one `DSLite load` line for the run. The build took 5.9 s after `make clean`; the make stops at the `.mcelf` boot-image step, which a RAM load does not need (see "If you see this").

## What was measured

Every row is in `data/results/am263p.csv`; the full tables, with the cases that did not run, are in [RESULTS-am263p.md](RESULTS-am263p.md). All rows: LP-AM263P Rev A, core R5FSS0-0 at 400 MHz, both L1 caches on, programs loaded into RAM by DSLite unless the row says flash.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Time from `dslite load` to Success, first blink | 78 s | one run; 4.4 to 7.6 s on later loads | measured | `data/results/am263p.csv` (`time_to_blink`) |
| Build of `i2c_led_blink` | 5.9 s | SDK makefile after `make clean`, SysConfig run included | measured | `data/results/am263p.csv` (`blink_build`) |
| CoreMark, -O3, code in OCSRAM | 1,464.2 per second (3.661 per MHz) | unmodified EEMBC files, 3 runs | measured | `data/results/am263p.csv` (`coremark`, `ticlang-bare-o3-ocsram`) |
| CoreMark, -O3, code and stack in TCM | 1,497.8 per second (3.744 per MHz) | 3 runs | measured | `data/results/am263p.csv` (`ticlang-bare-o3-tcm`) |
| Dhrystone, -O3, no inlining | 1.497 DMIPS per MHz | 3 runs | measured | `data/results/am263p.csv` (`dhrystone`) |
| Sine plus cosine pair, TMU / TI mathlib / C library | 26.9 / 77.8 / 300.3 cycles | float functions | measured | `data/results/am263p.csv` (`math_sincos`) |
| One FOC step, TMU / TI mathlib / C library | 160.3 / 247.4 / 412.0 cycles (0.40 us with the TMU) | mean of 1,000 steps from OCSRAM | measured | `data/results/am263p.csv` (`foc_step`) |
| Current loop, trigger to new PWM value, TMU | 1.24 us | 50 kHz, code in OCSRAM, 3 runs | measured | `data/results/am263p.csv` (`control_loop`) |
| GPIO pin write | 10.0 cycles | register and SDK driver | measured | `data/results/am263p.csv` (`gpio_toggle`) |
| Software-pended interrupt to first handler line, IRQ / FIQ | 195 / 144 cycles | through TI's dispatcher | measured | `data/results/am263p.csv` (`irq_latency`) |
| FreeRTOS task switch, TI's example, semaphore / notification | 6.06 / 5.13 us | TI's page shows 0.763 and 0.558; cause not found | measured | `data/results/am263p.csv` (`rtos_task_switch`) |
| CoreMark from flash, TI's example | 1,394.5 per second with no extra cache; 1,464.5 with the 64 kB L2 cache | flash at 16.7 MHz (PHY tuning failed) | measured | `data/results/am263p.csv` (`coremark`, `ti-port-o3-xip`, `ti-port-o3-xip-l2-64k`) |
| CoreMark on all four cores at once | 5,854.0 per second in total | each core within 0.1 percent of its score alone | measured | `data/results/am263p.csv` (`ti-port-o3-ocsram-all4-sum`) |
| CoreMark with the cores in lockstep | 1,464.52 per second | TI's example, 3 runs | measured | `data/results/am263p.csv` (`ti-port-o3-ocsram-lockstep`) |
| Message round trip, TI's IPC, FreeRTOS to bare-metal | 3.49 us (one partner), 3.81 us (three) | 3 runs | measured | `data/results/am263p.csv` (`ipc_roundtrip_us`) |
| TI SDK examples that build from the terminal | 249 of 284 | MCU+ SDK 26.01.00.08 makefiles | measured | `data/results/am263p.csv` (`examples_built`) |
| TI SDK examples that printed a pass | 118 of 208 run | single-core examples loaded after a System Reset | measured | `data/results/am263p.csv` (`examples_ran_clean`) |

Not run: the ADC rate, the UART loop, interrupt latency from an external edge and power (they need wires or a meter), and a flash boot. Zephyr 4.4.2 boots on R5FSS0-0 with out-of-tree files in `zephyr/`; upstream Zephyr has no AM263P support. Photos of the board are in `img/`.

## If you see this

| What you see | Do this | Check |
|---|---|---|
| Empty console after `DSLite load` | Open the first serial port (`...1`, 115200) before the load | `I2C LED Blink Test Started ...` appears |
| DSLite error -615 on connect | Plug in the 5 V / 3 A USB-C supply, and delete the `SWD Mode` property from a connection file copied from a Cortex-M33 board | DSLite connects |
| `ModuleNotFoundError: No module named 'construct'` at the end of `make` | Ignore it; only the boot-image step failed and the `.out` is built | `i2c_led_blink.release.out` exists |

The rest, each with its cause: [FAQ](FAQ.md).

## Limits

The board never booted from external flash: the flash backup lost one block, so no boot image was written. The external-edge and wire-based rows listed under "Not run" have no result.

## Where these facts come from

Checked 2026-10-09 to 2026-10-11 on an Intel Mac (macOS 15.7.9) with an LP-AM263P Rev A (PROC171A), MCU+ SDK for AM263Px 26.01.00.08, CCStudio 21.0.1 (DSLite 21.0.0.4166), TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, XDS110 probe firmware 3.0.0.29, Zephyr 4.4.2 for the Zephyr rows. Data: `data/results/am263p.csv` and [RESULTS-am263p.md](RESULTS-am263p.md). For an assistant: [AGENTS.md](AGENTS.md). Problems and fixes: [FAQ](FAQ.md).
