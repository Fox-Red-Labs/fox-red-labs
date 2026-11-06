#!/usr/bin/env python3
import json, os
BASE = os.path.expanduser("<repo>/cc2340r5/matrix")
d = json.load(open(os.path.join(BASE, "memory-breakdown.json")))
B = {b["id"]: b for b in d["builds"]}
APP = "Your app (the example's own code)"
KEYS = ["RTOS", "Radio stack", "TI drivers and device library", "C library and compiler runtime", APP,
        "Heap and stacks", "Other", "Boot configuration (CCFG)"]


def n(x):
    return f"{x:,}"


def pct(a, b):
    return f"{100 * a / b:.0f}%"


NOTES = {
    "empty_nortos": "TI's `empty` example without an RTOS. The RAM is almost all reserved space: an 8,192-byte C heap (`.sysmem`) that nothing uses and the 1,024-byte startup stack. TI's no-RTOS layer is counted under drivers.",
    "empty_rtos": "The same example on FreeRTOS. FreeRTOS reserves an 8,192-byte heap (`ucHeap`), 1,024 bytes of idle and timer task stacks and a 2,048-byte startup stack.",
    "basic_ble": "The FreeRTOS heap alone is 19,792 bytes; the Bluetooth stack and its tasks borrow from it while running, so most of the Heap and stacks row is really Bluetooth memory. The software elliptic-curve library used for pairing (`ecc.a`, 7,134 bytes of flash) is in the drivers row.",
    "data_stream": "Same layout as `basic_ble`: the same 19,792-byte FreeRTOS heap and the same Bluetooth libraries, with a different app.",
    "rfPacketTx": "No protocol stack: the Radio stack row is TI's radio control layer (RCL, 9,334 bytes of flash), the radio firmware patches (LRF, 8,544) and the radio settings SysConfig generates.",
    "onoff_switch_zed": "Logging is on: the log buffers (`ti_log_config`, 4,645 bytes of RAM) are in the drivers row. The Zigbee buffer pools that the example file declares (5,808 bytes) are counted as Radio stack, not app.",
    "onoff_light": "The largest build: 799 bytes of RAM left. A router keeps tables for its neighbors and children, so the Zigbee pools declared in the example file grow to 9,036 bytes (counted as Radio stack). Logging is on, as in the switch.",
    "ota_on_v1": "Our OTA client build with the receiver always on. Its flash area is the 284,880-byte slot after the MCUboot bootloader; MCUboot is a separate image and is not in these numbers. No logging, so the drivers row is smaller than the switch's. Zigbee pools from the example file: 5,744 bytes (Radio stack).",
    "coremark_O3": "No RTOS and no radio, so the app (the benchmark) dominates flash. The C library row is mostly `printf` (4,624 bytes of code). RAM: an 8,192-byte C heap and a 4,096-byte stack. This is the rebuilt image (37,444 bytes), not the first measured one (37,656).",
    "zephyr_blinky": "Built with GCC 12.2, not TI Arm Clang. Zephyr's own GPIO, UART and timer drivers are in the drivers row next to TI's device library. Stacks: 2,048 bytes for interrupts, 1,024 for `main`, 256 for the idle thread. The C library row is mostly picolibc's `printf` (2,031 bytes).",
    "zephyr_ti_peripheral": "Built with GCC 12.2; flashed but does not run yet ([../zephyr-ti/README.md](../zephyr-ti/README.md)). Heap and stacks: TI's Bluetooth controller heap (6,272 bytes), Zephyr's system heap (3,072), TI task stacks (3,008), interrupt stack (2,048), system work queue (2,048), `main` (1,024), logging thread (768), idle (320) and a 128-byte pool. Bluetooth host buffer pools stay in Radio stack.",
}


def table(b):
    rows = ["| Component | Flash (bytes) | RAM (bytes) |", "|---|--:|--:|"]
    for k in KEYS:
        f = b["flash"][k]
        r = b["ram"].get(k)
        rows.append(f"| {k} | {n(f)} | {'not RAM' if r is None else n(r)} |")
    c = b["check"]
    rows.append(f"| **Used, linker total** | **{n(c['flash_used_in_map'])}** plus CCFG | **{n(c['ram_used_in_map'])}** |")
    return "\n".join(rows)


summary = ["| Build | Flash used | Radio stack share | RAM used | Heap and stacks | RAM left |", "|---|--:|--:|--:|--:|--:|"]
for b in d["builds"]:
    c = b["check"]
    f = c["flash_used_in_map"]; r = c["ram_used_in_map"]
    rs = b["flash"]["Radio stack"]
    summary.append(f"| [{b['label']}](#{b['id'].lower()}) | {n(f)} | {pct(rs, f) if rs else 'none'} | {n(r)} | {n(b['ram']['Heap and stacks'])} | {n(36864 - r)} |")

sections = []
for b in d["builds"]:
    src = b["source"]
    src_txt = f"`{src}`" if src.startswith("~") else f"`{src}` (in `cc2340r5`)"
    sections.append(f"<a id=\"{b['id'].lower()}\"></a>\n### {b['label']}\n\n{b['sdk']}. Map: {src_txt}.\n\n{table(b)}\n\n{NOTES[b['id']]}\n")

page = f"""# Where do the flash and RAM go in each LP-EM-CC2340R5 build?

Terms: RTOS, heap, `.bss`, linker, CCFG, NVS, OTA, DMM and SysConfig are defined in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

The radio stack takes most of the flash: 71% to 88% in every Bluetooth and Zigbee build. RAM goes mostly to heap and stacks: 21,520 of 31,689 bytes in Bluetooth `basic_ble`, where the Bluetooth stack borrows its working memory from the FreeRTOS heap. Zigbee is different: its stack keeps its own tables and buffers, 13,830 to 20,295 bytes, more than the heap and stacks (8,892). The example's own code is small: 1% to 4% of flash in every radio build.

Split on 2026-10-06 from each build's linker map (builds of 2026-10-05): LP-EM-CC2340R5 board (CC2340R52 chip, 512 KB flash, 36 KB RAM), SimpleLink Low Power F3 SDK 9.21.00.36 with TI Arm Clang 5.1.1, plus two Zephyr builds with GCC 12.2. 1 KB = 1,024 bytes. The same numbers, machine-readable: [memory-breakdown.json](memory-breakdown.json). Totals per build: [memory.md](memory.md).

Contents: [all builds at a glance](#all-builds-at-a-glance), [how the split was made](#how-the-split-was-made), [one table per build](#one-table-per-build), [caveats](#caveats).

## All builds at a glance

Bytes. "Radio stack share" is the radio stack's part of flash used. "RAM left" is what the linker did not place, out of 36,864.

{chr(10).join(summary)}

## How the split was made

Every component adds up exactly to the linker's own totals (`MEMORY CONFIGURATION`, used column, FLASH and SRAM) for all 11 builds; nothing was left unexplained.

1. **Source.** TI maps list every object file and library with its code, read-only data and read-write data (`MODULE SUMMARY`). That table is the source for each object's size. The per-section list (`SECTION ALLOCATION MAP`) says where each piece landed, so bytes outside FLASH and SRAM (the Bluetooth NVS area, the Zigbee NV area) are left out. Zephyr maps (GNU format) have no summary, so every input section was added up by address.
2. **Components.** Each object goes in one bucket by where it came from:
   - **RTOS:** the FreeRTOS kernel, or the Zephyr kernel and its services (logging, `printk`, device model, power management).
   - **Radio stack:** the Bluetooth libraries and the Bluetooth files the SDK compiles into the project, the Zigbee (ZBOSS) libraries, the radio control layer (RCL), the radio firmware patches (LRF), the DMM stub and the generated radio settings.
   - **TI drivers and device library:** `drivers_*.a`, `driverlib`, the secure (crypto) drivers and the crypto libraries they call, display, log, the generated driver settings, TI's NV storage code. In Zephyr builds, Zephyr's own drivers too.
   - **C library and compiler runtime:** `libc`, `libsysbm` and `libclang_rt.builtins` (TI), picolibc or minimal libc and `libgcc` (Zephyr).
   - **Your app:** objects compiled from the example's own folder.
   - **Heap and stacks:** the FreeRTOS heap array `ucHeap`, the C heap `.sysmem`, the startup stack `.stack`, the FreeRTOS idle and timer task stacks; in Zephyr, the thread stacks and heap buffers in `noinit`.
   - **Boot configuration (CCFG):** the 2,048-byte boot settings area at 0x4E020000. It is its own memory region, so it is listed apart and is not part of "Flash used".
   - **Other:** the linker's startup tables and alignment padding.
3. **Check.** For each build, flash components (without CCFG) were checked to equal FLASH used and RAM components to equal SRAM used, to the byte. Other matched the padding plus startup tables exactly in every build.

## One table per build

{chr(10).join(sections)}
## Caveats

- **The heap is counted as Heap and stacks, even though the radio stack uses it.** In the TI Bluetooth builds the FreeRTOS heap is 19,792 bytes and the Bluetooth stack and the tasks it starts borrow from it at run time. The same goes for the 7,100-byte heap in the Zigbee builds and the 6,272-byte Bluetooth controller heap in TI's Zephyr fork. Read "Heap and stacks" as memory set aside up front, not memory the app owns.
- **FreeRTOS includes TI's glue.** SysConfig compiles the FreeRTOS kernel, TI's adapter layer (driver porting layer and POSIX), the startup code and the power policy into one file (`ti_freertos_config.c`). All of it is counted as RTOS. Without an RTOS, TI's no-RTOS layer is counted under drivers.
- **Bluetooth framework files count as Radio stack.** The Bluetooth examples compile SDK files into the project (the app framework `BLEAppUtil`, the UART menu, iCall, the device information service). They come from the SDK's Bluetooth folder, not the example folder, so they are counted as Radio stack: 9,672 bytes of flash and 1,304 of RAM in `basic_ble`, 9,806 and 1,088 in `data_stream`. Counting them as app instead would move that much.
- **Zigbee memory pools moved from app to Radio stack.** The Zigbee examples include `zb_mem_config_lprf3.h` in their main file, which declares the stack's buffer pools and tables there (names starting `gc_`). They were moved to Radio stack: 5,808 bytes in the switch, 9,036 in the router, 5,744 in the OTA client.
- **Zigbee link-time optimization.** The Zigbee libraries are optimized at link time, and the linker makes one object with a generated name (for example `TImlg4LqN0r`) holding shared code fragments and merged data: 5,712 bytes of code and 2,853 of RAM in the switch. Its named contents are Zigbee functions, so it is counted as Radio stack. **Unknown** whether a small part came from other code.
- **Crypto is counted under drivers.** TI's software elliptic-curve library (`ecc.a`), Zephyr's TinyCrypt and Mbed TLS files, and in the fork a link-time-optimized object holding the same elliptic-curve code (10,968 bytes) sit in the drivers row, next to the secure drivers that call them. In these builds their only user is Bluetooth pairing.
- **Logging.** TI's log library and log buffers are in the drivers row; Zephyr's logging subsystem is in the RTOS row.
- **Other is not the same on both toolchains.** In TI builds Other holds the startup tables (`.cinit`): the starting values of variables, compressed, copied to RAM at boot. They cannot be traced back to an object. GCC copies starting values uncompressed, so in Zephyr builds they are counted with their owners and Other is only padding.
- **Zephyr string merging.** GNU maps list text strings before the linker merges duplicates, so a few input sections overlap (205 bytes in the fork). Each byte was counted once, for the first object at that address.
- **Reserved flash is not counted.** The Bluetooth builds reserve 12,288 bytes for NVS and the Zigbee builds 4,096 or 8,192 for network settings. Those areas are separate memory regions, outside "Flash used".
- **Which OTA build.** The OTA row is the receiver-always-on rebuild (`ota_on_v1`), 222,004 bytes of flash. [memory.md](memory.md) lists the first OTA client build (`ota_v1`), 222,012 bytes; RAM is the same.
- **The maps are not committed.** They are build outputs. The two Zephyr maps live in the Zephyr workspaces outside this repo.
"""
out = os.path.join(BASE, "memory-breakdown.md")
open(out, "w").write(page)
print("wrote", out, len(page.split()), "words")
