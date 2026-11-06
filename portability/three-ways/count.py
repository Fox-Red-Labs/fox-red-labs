#!/usr/bin/env python3
"""Three ways: count code lines per layer, read flash and RAM from each build, check no image writes NONMAIN.
Code lines = no blank lines, no comment-only lines (the rule in portability/driver-layering.md).
Usage: python3 count.py   (after the build scripts). Prints the table and writes results.json."""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))

def code_lines(path):
    s = open(os.path.join(HERE, path)).read()
    if path.endswith((".conf",)) or os.path.basename(path) == "CMakeLists.txt":
        lines = [l.split("#", 1)[0] for l in s.splitlines()]
    else:
        s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
        lines = [re.sub(r"//.*", "", l) for l in s.splitlines()]
    return sum(1 for l in lines if l.strip())

SHARED = ["common/app.c", "common/app.h"]
WAYS = [  # (way, board, glue files, config files, build folder, log)
    ("DriverLib", "g3507", ["driverlib/glue_driverlib.c"], ["driverlib/g3507.syscfg"], "driverlib/build-g3507", "logs/driverlib-g3507.log"),
    ("TI Drivers + FreeRTOS", "g3507", ["tidrivers/glue_tidrivers.c", "tidrivers/main_freertos.c"],
     [f"tidrivers/g3507/{f}" for f in ("ti_drivers_config.c", "ti_drivers_config.h", "ti_msp_dl_config.c", "ti_msp_dl_config.h")],
     "tidrivers/build-g3507-freertos", "logs/tidrivers-g3507-freertos.log"),
    ("TI Drivers, no RTOS", "g3507", ["tidrivers/glue_tidrivers.c", "tidrivers/main_nortos.c"],
     [f"tidrivers/g3507/{f}" for f in ("ti_drivers_config.c", "ti_drivers_config.h", "ti_msp_dl_config.c", "ti_msp_dl_config.h")],
     "tidrivers/build-g3507-nortos", "logs/tidrivers-g3507-nortos.log"),
    ("Zephyr 4.4.2", "g3507", ["zephyr/src/glue_zephyr.c"], ["zephyr/prj.conf", "zephyr/boards/lp_mspm0g3507.overlay"],
     "zephyr/build-g3507", "logs/zephyr-g3507.log"),
    ("DriverLib", "l1306", ["driverlib/glue_driverlib.c"], ["driverlib/l1306.syscfg"], "driverlib/build-l1306", "logs/driverlib-l1306.log"),
    ("DriverLib", "g5187", ["driverlib/glue_driverlib.c"], ["driverlib/g5187.syscfg"], "driverlib/build-g5187", "logs/driverlib-g5187.log"),
    ("DriverLib (HSADC glue)", "m33c321a", ["driverlib/glue_driverlib_hsadc.c"], ["driverlib/m33c321a.syscfg"], "driverlib/build-m33c321a", "logs/driverlib-m33c321a.log"),
    ("DriverLib (AM13 glue)", "am13e230", ["driverlib/glue_am13.c"], ["driverlib/am13e230.syscfg"], "driverlib/build-am13e230", "logs/driverlib-am13e230.log"),
    ("TI Drivers, no RTOS", "l1306", ["tidrivers/glue_tidrivers.c", "tidrivers/main_nortos.c"],
     [f"tidrivers/l1306/{f}" for f in ("ti_drivers_config.c", "ti_drivers_config.h", "ti_msp_dl_config.c", "ti_msp_dl_config.h")],
     "tidrivers/build-l1306-nortos", "logs/tidrivers-l1306-nortos.log"),
    ("Zephyr 4.4.2", "l1306", ["zephyr/src/glue_zephyr.c"],
     ["zephyr/prj.conf", "zephyr/boards/lp_mspm0l1306.overlay", "zephyr/boards/lp_mspm0l1306.conf"],
     "zephyr/build-l1306", "logs/zephyr-l1306.log"),
]

def sizes(build, log):
    logt = open(os.path.join(HERE, log)).read()
    secs = float(re.findall(r"== clean build[^0-9]*([0-9.]+) s", logt)[-1])
    mp = [os.path.join(HERE, build, f) for f in ("three_ways.map", "zephyr/zephyr.map")]
    if "zephyr" in build:
        fl = int(re.search(r"FLASH:\s+(\d+) B", logt).group(1)); ram = int(re.search(r"RAM:\s+(\d+) B", logt).group(1))
        hexf = os.path.join(HERE, build, "zephyr/zephyr.hex")
    else:
        m = open(mp[0]).read().split("SEGMENT ALLOCATION MAP")[0]
        if re.search(r"^\s*FLASH_BANK0", m, re.M):  # AM13: flash banks and two RAM blocks, summed
            fl = sum(int(x, 16) for x in re.findall(r"^\s*FLASH_BANK\d\s+\S+\s+\S+\s+(\S+)", m, re.M))
            ram = sum(int(x, 16) for x in re.findall(r"^\s*RAM_[CS]\s+\S+\s+\S+\s+(\S+)", m, re.M))
        else:
            fl = int(re.search(r"^\s*FLASH\s+\S+\s+\S+\s+(\S+)", m, re.M).group(1), 16)
            ram = int(re.search(r"^\s*SRAM\s+\S+\s+\S+\s+(\S+)", m, re.M).group(1), 16)
        nonmain = sum(int(x, 16) for x in re.findall(r"^\s*B[CS]L?_?C?O?N?F?I?G?\S*\s+41c0\S+\s+\S+\s+(\S+)", m, re.M))
        heap = re.search(r"Heap:\s+0\s+0\s+(\d+)", open(mp[0]).read())
        return fl, ram, secs, nonmain_from_map(m), int(heap.group(1)) if heap else 0
    return fl, ram, secs, nonmain_from_hex(hexf), 0

def nonmain_from_map(m):  # bytes placed in BCR_CONFIG or BSL_CONFIG (NONMAIN, 0x41C00000)
    return sum(int(x, 16) for x in re.findall(r"^\s*(?:BCR|BSL)_CONFIG\s+41c0\S+\s+\S+\s+(\S+)", m, re.M))

def nonmain_from_hex(path):  # bytes at or above 0x41C00000 in an Intel HEX file
    base, n = 0, 0
    for l in open(path):
        l = l.strip(); cnt, addr, typ = int(l[1:3], 16), int(l[3:7], 16), int(l[7:9], 16)
        if typ == 4: base = int(l[9:13], 16) << 16
        elif typ == 2: base = int(l[9:13], 16) << 4
        elif typ == 0 and base + addr >= 0x41C00000: n += cnt
    return n

shared = sum(code_lines(f) for f in SHARED)
rows = []
for way, board, glue, cfg, build, log in WAYS:
    try:
        fl, ram, secs, nm, heap = sizes(build, log)
    except (FileNotFoundError, AttributeError, IndexError) as e:
        print(f"skip {way} {board}: {e}"); continue
    rows.append(dict(way=way, board=board, shared=shared, glue=sum(code_lines(f) for f in glue),
                     config=sum(code_lines(f) for f in cfg), flash=fl, ram=ram, unused_heap=heap,
                     build_s=secs, nonmain_bytes=nm, glue_files=glue, config_files=cfg))
json.dump(rows, open(os.path.join(HERE, "results.json"), "w"), indent=1)
print("| Board | Way | Shared | Glue | Config | Flash, B | RAM, B | Build, s | NONMAIN bytes |")
for r in rows:
    print(f"| {r['board']} | {r['way']} | {r['shared']} | {r['glue']} | {r['config']} | {r['flash']:,} | {r['ram']:,}"
          f"{' (' + format(r['unused_heap'], ',') + ' heap)' if r['unused_heap'] else ''} | {r['build_s']} | {r['nonmain_bytes']} |")
print("CMakeLists.txt (Zephyr):", code_lines("zephyr/CMakeLists.txt"), "code lines")
