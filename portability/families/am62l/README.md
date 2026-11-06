# Porting code to the AM62L family (TMDS62LEVM): MCU app on Linux, containers, Zephyr on a second core

The AM62L is a Linux-class part, so code lands at user space, not at a driver layer: the 81-line app used on 11 boards runs unchanged as a Linux program with 66 glue lines and 5 config lines (matrix id X53), Home Assistant, Mosquitto, Node-RED and Pi-hole run from containers in TI's own SDK image (`am62l-evm/linux-apps/README.md`), and Zephyr runs on core 1 beside Linux with a 56-byte echo of 4.0 to 5.7 µs (`am62l-evm/MIGRATION.md`). Gaps: the files record only two Cortex-A53 cores as user cores (the debugger also lists SMS_CM4_0_TIFS, TI's security firmware core), so there is no MCU-class core to port TI Drivers or DriverLib code to; no compiler on the image; no `apt`; Zephyr in the on-chip SRAM does not work beside Linux; there is no Arduino, MicroPython or ESPHome target.

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **U-Boot** is the boot loader that loads Linux; **TF-A** is Arm's secure-world firmware and **TIFS** TI's security firmware; **PSCI** is the Arm call that starts and stops CPU cores; **AMP** is asymmetric multiprocessing, a different operating system on each core; **Jailhouse** is a partitioning hypervisor; **DSLite** is TI's command-line debug server; **LSE** is the ARMv8.1 atomic instruction set. 1 kB = 1,024 bytes (the numbers in `am62l-evm/MIGRATION.md` use 1,000). The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the part and its neighbors, a ten-minute path, the one-command build, the function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (one part held)

| Part | Kit | Core and clock | Memory | On the board | Source |
|---|---|---|---|---|---|
| AM62L32 | TMDS62LEVM | two Cortex-A53 up to 1.25 GHz | 160 kB on-chip SRAM, 2 GB LPDDR4, 32 GB eMMC, micro-SD, 512 Mbit OSPI NOR at 0x60000000 | USB-C PD power into J17 (more than 15 W); console on an FT4232HL at J7 (port A is UART0, 115200 8N1); on-board XDS110 at J8; user LED LD8 on GPIO0_123, button SW5 on GPIO0_90 | `am62l-evm/AGENTS.md` |

Board folder: [`am62l-evm/`](../../../am62l-evm/AGENTS.md) (`README.md`, `AGENTS.md`, `FAQ.md`). No other AM62L part is held. No file here records an M33 or other MCU-class user core on this part.

### Part-to-part map: the AM62L32 against the MCU parts the portable app runs on

A dash means the files here do not record it.

| | AM62L32 | MSPM33C321A | CC2755P10 |
|---|---|---|---|
| Core, clock | 2 x Cortex-A53, 1.25 GHz | Cortex-M33, 160 MHz | Cortex-M33, 96 MHz |
| Memory | 2 GB LPDDR4, 160 kB SRAM | 1 MB flash, 256 kB RAM | 1,024 kB flash, 162 kB RAM |
| Layer the portable app uses | Linux sysfs and a tty, 66 glue lines | DriverLib with HSADC glue, 37 lines | TI Drivers with FreeRTOS, 48 lines |
| ADC | IIO `in_voltage0_raw`, TI-am335x-adc, 12 bit, 0.439 mV per count | HSADC | battery channel |
| App binary | `67,680 B` stripped, built in 1.2 s | `2,816 B` flash | 20.2 kB flash |
| CoreMark | 4,254 to 4,256 at 1.25 GHz (3.40 per MHz, GCC 15.3 -O2) | - | 426.31 (4.44 per MHz) |
| Compiler on the board | none; build in a Debian trixie arm64 container | - | - |
| First step | boot switches, full power cycle, serial rate | none | none |

Sources: `portability/three-ways/linux/README.md`, `am62l-evm/AGENTS.md`, `portability/families/mspm33/README.md`, `portability/families/radio/README.md`, matrix id X53.

## Ten-minute path

Run the shared app on a board that already runs Linux (booting is separate: UART boot to U-Boot takes about 7 minutes, then Linux logs in about 30 s after the kernel starts; steps in `am62l-evm/README.md`). The commands are from `portability/three-ways/linux/README.md`.

1. Terminal. Hold the console open with a program that sets the rate itself, never `stty` then `cat`:

   ```bash
   python3 -I am62l-evm/tools/capture.py /dev/cu.usbserial-<id>0 115200 console.log
   ```
2. Terminal. Copy `portability/three-ways/common/` and `portability/three-ways/linux/` to `/root/frl/tw` on the board. The image has no compiler.
3. Board shell. Build in the Debian trixie arm64 container:

   ```sh
   sh linux/build.sh /root/frl/tw
   ```

   You should see `three_ways_static`, `67,680 bytes` stripped, in about 1.2 s.
4. Board shell. Run it for 14 s:

   ```sh
   FRL_UART=/dev/ttyS0 timeout 14 /root/frl/tw/linux/build/three_ways_static
   ```

   You should see `t=<n> adc=<raw> avg=<avg>` once a second on the console.
5. Board shell. Give the console its output processing back and the LED its trigger back:

   ```sh
   stty -F /dev/ttyS0 opost
   echo heartbeat > /sys/class/leds/am62-sk:green:heartbeat/trigger
   ```
6. Mac. `python3 portability/three-ways/serial_check.py /dev/cu.usbserial-<id>0 16 <log file>` prints PASS (14 lines, gaps 0.991 to 1.008 s, 2026-10-25).

The OSPI NOR was "not written in any run" (`am62l-evm/AGENTS.md`). The SDK image runs from a USB stick or the micro-SD card.

## One-command build

```sh
sh linux/build.sh /root/frl/tw
```

On the board, inside a container: `common/app.c` plus `glue_linux.c` (66 code lines) and `config_am62l.h` (5 code lines), `67,680 B` stripped, 1.2 s (`am62l-evm/AGENTS.md`). It does not touch the boot media.

## Function map

Columns: Linux user space (native), the portable MCU app, Zephyr, and software run from containers. Not run cells are quiet text. This part has no TI Drivers, DriverLib, Arduino or MicroPython layer.

| Function | Linux user space | Portable MCU app | Zephyr | Containers and Python |
|---|---|---|---|---|
| GPIO | Works: libgpiod tools 2.2.4; LD8 and SW5 are on `gpiochip0`, owned by `leds-gpio` and `gpio-keys`, released with `am62l-evm/linux-apps/gpio_claim.sh free` | Works: LED class sysfs `am62-sk:green:heartbeat/brightness` after the trigger is set to `none`; not seen by eye | - | Works: gpiozero and Blinka toggle LD8 after three workarounds |
| UART | Works: `/dev/ttyS0`, UART0 at 0x02800000, 115200 | Works: 14 lines, gaps 0.991 to 1.008 s | Works: 16550 driver on UART0 for hello world; the core 1 image prints to a 4 kB ring at 0xEFFFF000 | - |
| ADC | Works: IIO `in_voltage0_raw`, 12 bit | Works: `adc_read` from sysfs | - | - |
| I2C, SPI, PWM | _not recorded_ in the files read | - | - | - |
| Display | Works: no GPU; LVGL 9.3 at 1080p 54.1 fps on fbdev, 18.0 on DRM; Slint software renderer 41.9 fps | - | - | - |
| Real time | Works: stock PREEMPT kernel, cyclictest worst case 442 µs idle, 1,074 µs loaded, one unexplained 93,699 µs | - | - | - |
| Second core | Works: `echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl` starts Zephyr on core 1 | - | Works: image linked at 0xE0000000, entry 0xE00008BC | - |
| Partitioning | Works: `jailhouse enable` 0.27 s; `uart-demo`, `gic-demo` and a second Linux as inmates | - | - | - |
| Containers | Works: Docker 29.3.0 in TI's SDK image | - | - | Works: Home Assistant 557 MiB and 67.7 s to first answer, Node-RED 110 MiB, Pi-hole 24.8 MiB, Mosquitto 2.6 MiB |
| Power | Works: eight INA228 rails, 567.6 mW idle, within 5 mW of that with every app running | - | - | - |

Sources: matrix id X53; `am62l-evm/AGENTS.md`; `am62l-evm/README.md`; `am62l-evm/MIGRATION.md`; `am62l-evm/linux-apps/README.md`; `portability/three-ways/linux/README.md`; `am62l-evm/zephyr/README.md`.

## How to port into this family

1. **Pick the layer: Linux user space.** Sysfs and a tty carry the portable app; nothing written to TI Drivers or DriverLib lands here (`portability/three-ways/linux/README.md`).
2. **Plan for no compiler and no package manager.** Build in a Debian trixie arm64 container on the board with `--network host`; `apt` does not exist and `opkg update` cannot reach its feed outside TI's network; add software with `docker run` or `pip3 install --break-system-packages` (`am62l-evm/AGENTS.md`).
3. **Free the LED and button from the kernel drivers** before a program drives them; LD8 reads back 0 whatever is written, so keep state in the program (`am62l-evm/linux-apps/README.md`).
4. **Clear `OPOST` on the tty** when a program writes lines, and restore it after, or lines end in `\r\r\n` (`portability/three-ways/linux/README.md`).
5. **Give Python GPIO a board description.** gpiozero needs one, lgpio needs swig 4.3.1 and a source-built C library, Blinka needs `blinka_am62l.py` (`am62l-evm/FAQ.md`).
6. **Check for LSE atomics when a program dies with `Illegal instruction`.** The A53 lacks ARMv8.1 LSE atomics; read the address from `dmesg` and preload a trap library such as the one built from `am62l-evm/steam/lse_trap/` (`am62l-evm/AGENTS.md`).
7. **For Zephyr beside Linux, use core 1 in DDR.** Boot Linux with `maxcpus=1 mem=1536M cpuidle.off=1`, link Zephyr at 0xE0000000, build it non-secure (`core1-ns.conf`) and with `am62l-evm/zephyr/patches/gic-skip-distributor.patch` (`am62l-evm/AGENTS.md`).
8. **Do not put Zephyr in the on-chip SRAM beside Linux.** Linux owns 0x70800000 to 0x7080FFFF and TF-A code sits at 0x707F0000 (`am62l-evm/AGENTS.md`).
9. **Set the boot switches, then do a full power cycle.** SW7 does not re-read them (`am62l-evm/AGENTS.md`).
10. **Plan the RPMsg layer.** The Linux-to-Zephyr link is a custom RPMsg-framed ring pair, not OpenAMP and not remoteproc; the name service and endpoints are not implemented (`am62l-evm/MIGRATION.md`).

## Moving inside the family

Only one AM62L part is held. Moves between its two A53 cores are:

| Move | Route | Measured | Files |
|---|---|---|---|
| Linux on core 0, Zephyr on core 1, no hypervisor | PSCI `CPU_ON` from U-Boot (`am62l-evm/tools/psci_stub.py cpu_on 0x1 0xE00008BC`) | Zephyr ring read from Linux | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| Linux starts Zephyr and the two exchange messages | kernel module and 1 MB DDR window | echo 4.0 to 5.7 µs for 56 bytes | `am62l-evm/MIGRATION.md` |
| Linux plus inmates under Jailhouse | `jailhouse cell create`, `load`, `start` | `uart-demo` prints `Hello 1 from cell!`; `gic-demo` prints `Timer fired, jitter: N ns` | `docs/experiments/2026-10-22-am62l-jailhouse.md` |

## Moving to and from the other families

| To or from | Route | Measured | Matrix ids and files |
|---|---|---|---|
| MSPM0L1306 app to AM62L | Shared `app.c`, Linux sysfs glue | 11 boards tried, `app.c` byte-identical; AM62L glue 66 lines, config 5; serial check passed | X53 |
| MCU coprocessor firmware to Zephyr on core 1 | Steps in `am62l-evm/MIGRATION.md` | 56-byte echo 4.0 to 5.7 µs | `am62l-evm/MIGRATION.md` |
| Raspberry Pi software | Containers and Python GPIO | four containers and gpiozero, Blinka ran | `am62l-evm/linux-apps/README.md` |
| MCU guides | | | [MSPM0](../mspm0/README.md), [MSPM33](../mspm33/README.md), [Radio](../radio/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Shared app as a Linux program | 66 glue lines, 5 config lines, `67,680 B`, 1.2 s build | TI Linux SDK 12.01, kernel 6.18.38 | measured | `portability/matrix.csv` X53 |
| UART boot to the U-Boot prompt | about 7 minutes | switches SW3.4 ON, 3.3 ON, 3.2 ON, 3.1 OFF | measured | `am62l-evm/AGENTS.md` |
| Linux-to-Zephyr echo | 4.0 to 5.7 µs, 56 bytes | both cores at 1.25 GHz | measured | `am62l-evm/MIGRATION.md` |
| CoreMark | 4,254 to 4,256 | 1.25 GHz, GCC 15.3 -O2 | measured | `data/results/am62l-evm.csv` |
| `jailhouse enable` | 0.27 s | TI's Jailhouse image | measured | `am62l-evm/AGENTS.md` |
| Home Assistant Container | 557 MiB, 67.7 s to the onboarding API | one run | measured | `am62l-evm/linux-apps/README.md` |
| Rails, idle | 567.6 mW | eight INA228 rails, 30 s mean | measured | `am62l-evm/linux-apps/README.md` |
| Zephyr hello world, SRAM map | 124 KB of 144 KB used; floor 104 kB; the 80 kB MAIN SRAM map does not link | 8 translation tables | measured | `am62l-evm/zephyr/README.md` |
| LVGL 1080p scene | 54.1 fps on fbdev | median of 3, 30 s each | measured | `am62l-evm/AGENTS.md` |

## If you see this

- If the console prints `Starting kernel` and nothing else after `booti`, `bootargs` was never set: reset, then `setenv bootargs` first (`am62l-evm/AGENTS.md`).
- If a program dies with `Illegal instruction`, look for an LSE instruction at the `dmesg` address and preload a trap library (`am62l-evm/steam/lse_trap/`, `am62l-evm/AGENTS.md`).
- If every UART stays quiet with the shipped switches and no SD card, the boot ROM is waiting: use UART boot (SW3.4 ON, 3.3 ON, 3.2 ON, 3.1 OFF, power cycle) (`am62l-evm/README.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

TI Linux SDK 12.01.00.05.03 (kernel 6.18.38), Zephyr at commit a915b2f, CCStudio 21.0.1 (DSLite 21.0.0.4166), XDS110 firmware 3.0.0.32, Docker 29.3, Python 3.14, on an Apple M4 Mac; runs dated 2026-10-18 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/am62l-evm.csv`, `data/support-matrix.json`. The generated record: `portability/families/am62l/EVIDENCE.md`. Checked 2026-10-25.
