#!/usr/bin/env python3
"""Split TI Arm Clang linker maps (.map) into beginner components.

Source of truth per module: the map's MODULE SUMMARY (code, ro data, rw data).
Placement (which memory region each byte lands in) and the heap/stack carve-outs
come from the SECTION ALLOCATION MAP input-section lines.
"""
import re, sys, json, os
from collections import defaultdict

C_RTOS = "RTOS"
C_RADIO = "Radio stack"
C_DRV = "TI drivers and device library"
C_LIBC = "C library and compiler runtime"
C_APP = "Your app (the example's own code)"
C_HEAP = "Heap and stacks"
C_CCFG = "Boot configuration (CCFG)"
C_OTHER = "Other"
ORDER = [C_RTOS, C_RADIO, C_DRV, C_LIBC, C_APP, C_HEAP, C_CCFG, C_OTHER]

IN_RE = re.compile(r"^\s{6,}([0-9a-f]{8})\s+([0-9a-f]{8})\s+(.*\S)\s*$")


def parse(path):
    lines = open(path, errors="replace").read().splitlines()
    regions = {}
    i = lines.index("MEMORY CONFIGURATION")
    for ln in lines[i + 4:]:
        if not ln.strip():
            if regions:
                break
            continue
        p = ln.split()
        regions[p[0]] = dict(origin=int(p[1], 16), length=int(p[2], 16), used=int(p[3], 16))
    # section allocation map
    s0 = lines.index("SECTION ALLOCATION MAP")
    s1 = lines.index("MODULE SUMMARY")
    inputs = []  # (addr, size, lib, obj, sec, outsec, kind)
    lib = None
    outsec = None
    for ln in lines[s0:s1]:
        if ln and not ln[0].isspace() and not ln.startswith("*"):
            outsec = ln.split()[0]
            continue
        m = IN_RE.match(ln)
        if not m:
            continue
        addr, size, rest = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        if rest.startswith("--HOLE--"):
            inputs.append((addr, size, None, None, "HOLE", outsec, "hole")); continue
        if rest.startswith("(.common:"):
            inputs.append((addr, size, None, None, rest, outsec, "common")); continue
        if rest.startswith("("):
            inputs.append((addr, size, None, None, rest, outsec, "linker")); continue
        mm = re.match(r"^(?:(\S+\.a)\s*)?(?::\s*)?(\S+)\s+\((.*)\)", rest)
        if not mm:
            inputs.append((addr, size, None, None, rest, outsec, "unknown")); continue
        if mm.group(1):
            lib = mm.group(1)
        elif ":" not in rest.split("(")[0]:
            lib = None
        inputs.append((addr, size, lib, mm.group(2), mm.group(3), outsec, "obj"))
    # module summary
    mods = []  # (group, name, code, ro, rw)
    group = None
    specials = {}
    for ln in lines[s1 + 4:]:
        if ln.startswith("LINKER GENERATED"):
            break
        if re.match(r"^    \S", ln):
            group = ln.strip(); continue
        st = ln.strip()
        if not st or st.startswith("+--") or st.startswith("Total:") or st.startswith("Module") or st.startswith("------"):
            continue
        m = re.match(r"^(Heap|Stack|Linker Generated|Grand Total):\s+(\d+)\s+(\d+)\s+(\d+)", st)
        if m:
            specials[m.group(1)] = tuple(int(x) for x in m.group(2, 3, 4)); continue
        p = st.split()
        if len(p) >= 4 and all(x.isdigit() for x in p[-3:]):
            mods.append((group, " ".join(p[:-3]), int(p[-3]), int(p[-2]), int(p[-1])))
    return regions, inputs, mods, specials


def region_of(regions, addr):
    for n, r in regions.items():
        if r["origin"] <= addr < r["origin"] + r["length"]:
            return n
    return None


def classify(group, name):
    g = (group or "").lower()
    n = name.lower()
    libname = os.path.basename(g)
    if g in ("./",):
        return C_RADIO  # link-time-optimised object built from the Zigbee (ZBOSS) libraries
    if g == "<current directory>" or g.endswith("/"):
        if n.startswith("ti_freertos"):
            return C_RTOS
        if n.startswith("ti_devices_config"):
            return C_CCFG
        if n.startswith(("ti_drivers_config", "ti_log_config", "common_drivers_nv_")):
            return C_DRV
        if n.startswith(("ti_radio_config", "ti_ble_config", "dmm_")):
            return C_RADIO
        if n.startswith(("common_bleapputil", "common_menumodule", "common_icall", "common_lib_opt",
                         "common_config", "common_startup_rom_init", "healthtoolkit",
                         "common_services_", "common_profiles_", "common_util_")):
            return C_RADIO  # compiled from the SDK's Bluetooth folder (source/ti/ble)
        return C_APP
    if libname in ("libc.a", "libsysbm.a", "libclang_rt.builtins.a"):
        return C_LIBC
    if libname.startswith(("onelib", "stackwrapper", "zb_", "rcl_", "lrf_", "dmm", "ble_")):
        return C_RADIO
    if libname.startswith(("driverlib", "drivers_", "secure_drivers", "display_", "log_", "nortos_", "ecc")):
        return C_DRV
    if "freertos" in libname:
        return C_RTOS
    return None


HEAP_SEC = re.compile(r"(^\.bss\.ucHeap$|uxIdleTaskStack$|uxTimerTaskStack$)")  # .stack and .sysmem come from the summary rows
ZB_POOL = re.compile(r"^\.(data|bss)\.gc_")  # ZBOSS memory pools declared by zb_mem_config_*.h in the app file


def split(path):
    regions, inputs, mods, specials = parse(path)
    flash_names = [n for n in regions if n == "FLASH"]
    ram_names = [n for n in regions if n == "SRAM"]
    flash = defaultdict(int); ram = defaultdict(int); ccfg = 0
    unclassified = []
    notes = []
    # explicit placements per (lib basename, obj)
    placed = defaultdict(lambda: defaultdict(int))
    moves = []  # (lib, obj, region, size, target component)
    for addr, size, lib, obj, sec, outsec, kind in inputs:
        if kind != "obj":
            continue
        reg = region_of(regions, addr)
        key = (lib, obj)
        placed[key][reg] += size
        if reg == "SRAM" and HEAP_SEC.search(sec):
            moves.append((key, size, C_HEAP))
        elif reg == "SRAM" and ZB_POOL.search(sec) and lib is None:
            moves.append((key, size, C_RADIO))
    for group, name, code, ro, rw in mods:
        comp = classify(group, name)
        if comp is None:
            unclassified.append((group, name)); comp = C_OTHER
        lib = os.path.basename(group) if group not in ("<current directory>", "./") else None
        key = (lib, name)
        pl = placed.get(key, {})
        total = code + ro + rw
        in_flash = pl.get("FLASH", 0)
        in_ccfg = pl.get("CCFG", 0)
        elsewhere = sum(v for r, v in pl.items() if r not in ("FLASH", "SRAM", "CCFG") and not (r or "").startswith("LOG"))
        in_ram = total - in_flash - in_ccfg - elsewhere  # commons have no object name; they are .bss in SRAM
        flash_comp = comp if comp != C_CCFG else C_OTHER
        flash[flash_comp] += in_flash
        ccfg += in_ccfg
        ram[comp if comp != C_CCFG else C_OTHER] += in_ram
        for (mk, sz, tgt) in moves:
            if mk == key:
                ram[comp] -= sz; ram[tgt] += sz
    # summary-level specials
    for k in ("Heap", "Stack"):
        if k in specials:
            ram[C_HEAP] += specials[k][2]
    lg = specials.get("Linker Generated", (0, 0, 0))
    linker_flash = sum(s for a, s, l, o, sec, os_, k in inputs if k == "linker" and region_of(regions, a) == "FLASH")
    flash[C_OTHER] += linker_flash
    # .stack: TI lists only 4 bytes from boot_cortex_m.c.obj plus a HOLE; the module summary "Stack:" row holds the full size,
    # so undo the 4-byte move made above to avoid double counting.
    holes = defaultdict(int)
    for addr, size, lib, obj, sec, outsec, kind in inputs:
        if kind == "hole" and outsec not in (".stack", ".sysmem"):
            holes[region_of(regions, addr)] += size
    used_f = regions["FLASH"]["used"]; used_r = regions["SRAM"]["used"]
    gap_f = used_f - sum(flash.values()); gap_r = used_r - sum(ram.values())
    flash[C_OTHER] += gap_f; ram[C_OTHER] += gap_r
    return dict(
        regions=regions,
        flash={c: flash.get(c, 0) for c in ORDER if c != C_CCFG} | {C_CCFG: ccfg},
        ram={c: ram.get(c, 0) for c in ORDER if c != C_CCFG},
        gap_flash=gap_f, gap_ram=gap_r, holes_flash=holes.get("FLASH", 0), holes_ram=holes.get("SRAM", 0), linker_generated_flash=linker_flash,
        linker_generated_summary=lg[1], unclassified=unclassified,
    )


if __name__ == "__main__":
    for p in sys.argv[1:]:
        r = split(p)
        print("==", p)
        print(" flash", json.dumps(r["flash"]))
        print(" ram  ", json.dumps(r["ram"]))
        print(" gap flash", r["gap_flash"], "holes", r["holes_flash"], "gap ram", r["gap_ram"], "holes", r["holes_ram"], "linker flash", r["linker_generated_flash"], "summary", r["linker_generated_summary"], "unclassified", r["unclassified"])
