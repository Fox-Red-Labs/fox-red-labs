# Thread (OpenThread) on the LP-CC1354P10-1

Terms: [docs/vocab.md](../../docs/vocab.md) and the [CC1354P10 FAQ](../FAQ.md). 1 kB = 1,024 bytes. FTD is a full Thread device, MTD a minimal one, RCP a radio co-processor.

**Runs: one board formed its own Thread network and became leader** (2026-10-06, about 6 to 10 s after `thread start`; PAN 0xf561, channel 13, network name OpenThread-f561, mesh-local and link-local IPv6 addresses up). Built as below. TI's OpenThread port (`TexasInstruments/ot-ti`, release `thread-v1.4-ti-2.1`, commit `eaac0de`) built every CC1354P10 image on 2026-10-06 in 4 minutes with no failures, against the installed **F2 SDK 8.33**, although `ot-ti` pins F2 SDK 7.40 as its submodule. The CLI runs at **921600 baud** (`ot-ti/src/uart.c`), not 115200: at 115200 it answers in garbage, which is also what the CC2340R5 Thread test saw. macOS termios stops at 230400, so `tools/ot_cli.py` sets the speed with the `IOSSIOSPEED` ioctl.

| Image | Flash | RAM (including reserved heap and stacks) |
|---|--:|--:|
| `ot-cli-ftd` (full Thread device, command line) | 397.9 kB | 92.7 kB |
| `ot-cli-mtd` (minimal Thread device) | 335.4 kB | 86.7 kB |
| `ot-rcp` (radio co-processor for a border-router host) | 86.6 kB | 65.9 kB |
| `ot-cli-radio` (radio test) | 75.4 kB | 64.6 kB |

Flash is the end of the last flash section in each ELF file; the 18 kB of non-volatile storage is not counted. RAM is the stack end minus the RAM base. All fit the CC1354P10's 1 MB of flash and 256 kB of RAM. The figures are rows in `data/results/cc1354p10.csv`. On the CC2340R5 (36 kB RAM) every node build fails to link (`cc2340r5/thread/`). The images carry a valid CCFG (`IMAGE_VALID_CONF` = 0), so they will boot.

## How it was built

Toolchain from the CC2340R5 Thread work (`cc2340r5/thread/README.md`: GCC 12.2 from the Zephyr SDK, CMake below 4, SysConfig 1.26.3), then:

    cmake -GNinja -DTI_SIMPLELINK_BOARD=LP_EM_CC1354P10_1 -DOT_PLATFORM=external \
      -DCMAKE_TOOLCHAIN_FILE=src/arm-none-eabi-m33.cmake -DTI_SIMPLELINK_SDK_DIR=<sdk path> ~/ot-ti -B <build dir>
    cmake --build <build dir>

No edits to `ot-ti` and no SDK download. One trap: `ot-ti`'s CMake reads `<SDK>/../../../src/nvocmp.c`, which only exists when the SDK is the submodule inside `ot-ti`. CMake resolves that path as text, so pointing `TI_SIMPLELINK_SDK_DIR` at a symlink three levels down (`<dir>/a/b/sdk` -> the installed SDK, plus `<dir>/src` -> `~/ot-ti/src`) satisfies it.

## Run it

`run_thread_leader.sh <ot-cli-ftd.out>` loads the image with CCStudio scripting (no power cycle), forms a new network alone with the OpenThread CLI (`tools/ot_cli.py`) and reads back state, PAN ID, channel and addresses. A two-node test with a CC2340R5 as the second device passed: see [two TI radios on one Thread network](../../board-to-board/thread-two-radios-2026-10-06/README.md). The RCP needs a border-router host: see [the CC1354P10 as a border-router host](../../border-router/host-cc1354/README.md).

## Thread border router: what works, and what does not exist on TI (checked 2026-10-06)

**Measured: the CC1354P10 works as the radio of a border router.** TI's `ot-rcp` image (radio co-processor, built above) was loaded on the board, and OpenThread's host program (`ot-cli`, POSIX build of the same OpenThread commit, built on this Intel Mac in 15 s) drove it over the backchannel UART: `spinel+hdlc+uart:///dev/cu.usbmodemLS45013A1?uart-baudrate=921600`. The host reported the RCP as `OPENTHREAD/1.4.0.0; CC13XX_CC26XX thread-v1.4-ti-1.0-ea-1.0`, formed a new network (PAN 0xb00d, channel 24) and became leader.

That is the radio half of every Linux Thread border router: OpenThread's `ot-br-posix` (the border-router software, Linux only) talks to an RCP exactly this way. TI documents the same split: `ot-ti` pins an `ot-br-posix` commit, and its "border router setup guide" (`docs/thread-users-guide/ti-openthread-borderrotuer-setup-guide.md`, filename misspelled) links to a TI SimpleLink Academy lab built on a Raspberry Pi and a TI RCP, from the 2023 SDK. The CC1354P10 is not in `ot-ti`'s device table (its sibling CC2674P10 is), and every TI Thread role is marked "Evaluation Only".

**Not measured:** a full border router with this board as its radio. Needs `ot-br-posix` on a Linux host (a Raspberry Pi, or a container on another computer) with this board as its RCP, then route a Thread device's IPv6 traffic to the home network.

**Single-chip border router:** NXP's RW612 puts Wi-Fi 6, Bluetooth LE and an 802.15.4 radio on one chip and runs OpenThread's border routing on it without Linux (NXP's description; not checked here). TI has no chip with both Wi-Fi and 802.15.4: the CC35xx has Wi-Fi 6 and Bluetooth LE only, and the CC13xx/CC26xx/CC27xx have 802.15.4 and no Wi-Fi. A TI border router is therefore always two chips. The embedded (no Linux) version would be a TI Wi-Fi part running OpenThread's border-routing code as the host, with a CC27xx or CC13xx RCP over UART or SPI; that is the shape of Espressif's two-chip Thread border router (ESP32-S3 host plus ESP32-H2 RCP, `esp-thread-br`; not checked here). No TI port of that host side was found.
