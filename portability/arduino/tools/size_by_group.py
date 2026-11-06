#!/usr/bin/env python3
"""Split a linked program's size by who wrote the code, from the linker map file.
Usage: size_by_group.py out/<sketch>/<sketch>.map
Groups: sketch, Arduino_GFX, Adafruit_FT6206, Adafruit_BusIO, ArduinoCore-API (Print, String, ...),
shim (core/ + SysConfig output + start-up), TI driverlib, TI C/C++ runtime (libc, libc++, builtins).
Flash = .text .const .rodata .cinit .init_array .intvecs; RAM = .data .bss .sysmem .stack (static and reserved)."""
import re, sys, collections

FLASH = {".text", ".const", ".rodata", ".cinit", ".init_array", ".intvecs", ".binit", ".pinit", ".ARM.exidx"}
RAM = {".data", ".bss", ".sysmem", ".stack", ".vtable", ".args"}
shim_objs = {"wiring.cpp.o", "main.cpp.o", "HardwareSerial.cpp.o", "SPI.cpp.o", "Wire.cpp.o",
             "ti_msp_dl_config.c.o", "startup_mspm0g350x_ticlang.c.o"}
api_objs = {"Print.cpp.o", "Stream.cpp.o", "String.cpp.o", "Common.cpp.o", "IPAddress.cpp.o"}

def group(obj, arch):
    if arch:
        if arch == "driverlib.a": return "TI driverlib"
        return "TI runtime (libc, libc++, builtins)"
    if obj == "sketch.o": return "sketch"
    if obj in shim_objs: return "shim"
    if obj in api_objs: return "ArduinoCore-API"
    if obj.startswith("Adafruit_FT6206"): return "Adafruit_FT6206"
    if obj.startswith("Adafruit_"): return "Adafruit_BusIO"
    if obj.startswith("Arduino_"): return "Arduino_GFX"
    return "other: " + obj

tot = {"flash": collections.Counter(), "ram": collections.Counter()}
sec = None
arch = None
for line in open(sys.argv[1]):
    m = re.match(r'^(\.[\w.]+)\s+\d+\s+[0-9a-f]+\s+[0-9a-f]+', line)
    if m:
        sec = m.group(1); arch = None
        continue
    if line.startswith("SEGMENT") or line.startswith("GLOBAL SYMBOLS") or line.startswith("MODULE SUMMARY"):
        if line.startswith("GLOBAL") or line.startswith("MODULE"): break
    m = re.match(r'^\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(?:(\S+)\s+:\s+)?(\S+)\s+\((\S+)\)', line) or \
        re.match(r'^\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(\s)(\S+)\s+\((\S+)\)', line)
    if not m or sec is None:
        # continuation lines of an archive:  "      : member (section)"
        m2 = re.match(r'^\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+:\s+(\S+)\s+\((\S+)\)', line)
        if m2 and sec is not None and arch:
            size = int(m2.group(2), 16); g = group(m2.group(3), arch)
            kind = "flash" if sec in FLASH else "ram" if sec in RAM else None
            if kind: tot[kind][g] += size
        continue
    size = int(m.group(2), 16)
    if m.group(3) and m.group(3).strip():
        arch = m.group(3)
    obj = m.group(4)
    g = group(obj, arch if m.group(3) and m.group(3).strip() else None) if True else None
    kind = "flash" if sec in FLASH else "ram" if sec in RAM else None
    if kind: tot[kind][g] += size
for kind in ("flash", "ram"):
    print(kind)
    for g, n in sorted(tot[kind].items(), key=lambda x: -x[1]):
        print(f"  {g:40s} {n:7d}")
    print(f"  {'total':40s} {sum(tot[kind].values()):7d}")
