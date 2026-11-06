#!/usr/bin/env python3
"""Split a GNU ld (Zephyr) map into the same beginner components as ti_map_split.py."""
import re, sys, json
from collections import defaultdict

C_RTOS = "RTOS"; C_RADIO = "Radio stack"; C_DRV = "TI drivers and device library"
C_LIBC = "C library and compiler runtime"; C_APP = "Your app (the example's own code)"
C_HEAP = "Heap and stacks"; C_CCFG = "Boot configuration (CCFG)"; C_OTHER = "Other"
ORDER = [C_RTOS, C_RADIO, C_DRV, C_LIBC, C_APP, C_HEAP, C_CCFG, C_OTHER]

NONALLOC = re.compile(r"^\.(debug|comment|ARM\.attributes|symtab|strtab|shstrtab|stab)")
HEX = r"0x[0-9a-fA-F]+"


def parse(path):
    lines = open(path, errors="replace").read().splitlines()
    i = lines.index("Memory Configuration")
    regions = {}
    for ln in lines[i + 3:]:
        if ln.startswith("Linker script"):
            break
        p = ln.split()
        if len(p) >= 3 and p[0] != "*default*":
            regions[p[0]] = dict(origin=int(p[1], 16), length=int(p[2], 16))
    j = lines.index("Linker script and memory map")
    out = []  # output sections: dict(name, vma, size, lma, inputs=[(name, addr, size, file, syms)])
    cur = None
    pending_in = None
    last_in = None
    for ln in lines[j + 1:]:
        if ln.startswith("OUTPUT(") or ln.startswith("LOAD linker stubs"):
            break
        if ln and not ln[0].isspace():
            m = re.match(rf"^(\S+)\s+({HEX})\s+({HEX})(?:\s+load address\s+({HEX}))?", ln)
            if m:
                cur = dict(name=m.group(1), vma=int(m.group(2), 16), size=int(m.group(3), 16),
                           lma=int(m.group(4), 16) if m.group(4) else None, inputs=[])
                out.append(cur); pending_out = None
            else:
                cur = dict(name=ln.strip(), vma=None, size=None, lma=None, inputs=[]); out.append(cur)
            last_in = None; pending_in = None
            continue
        if cur is None:
            continue
        if cur["vma"] is None:
            m = re.match(rf"^\s+({HEX})\s+({HEX})(?:\s+load address\s+({HEX}))?\s*$", ln)
            if m:
                cur["vma"] = int(m.group(1), 16); cur["size"] = int(m.group(2), 16)
                cur["lma"] = int(m.group(3), 16) if m.group(3) else None
                continue
        # input section, one line
        m = re.match(rf"^ (\S+)\s+({HEX})\s+({HEX})\s+(\S.*)$", ln)
        if m and not ln.startswith(" *(") :
            name = m.group(1)
            if name == "*fill*":
                cur["inputs"].append(("*fill*", int(m.group(2), 16), int(m.group(3), 16), None, []))
            else:
                last_in = [name, int(m.group(2), 16), int(m.group(3), 16), m.group(4).strip(), []]
                cur["inputs"].append(last_in)
            pending_in = None
            continue
        m = re.match(rf"^ (\*fill\*)\s+({HEX})\s+({HEX})\s*$", ln)
        if m:
            cur["inputs"].append(("*fill*", int(m.group(2), 16), int(m.group(3), 16), None, [])); continue
        m = re.match(r"^ (\S+)\s*$", ln)
        if m and not ln.startswith(" *") and not m.group(1).startswith("*("):
            pending_in = m.group(1); continue
        m = re.match(rf"^\s+({HEX})\s+({HEX})\s+(\S.*)$", ln)
        if m and pending_in:
            last_in = [pending_in, int(m.group(1), 16), int(m.group(2), 16), m.group(3).strip(), []]
            cur["inputs"].append(last_in); pending_in = None; continue
        m = re.match(rf"^\s+({HEX})\s+([A-Za-z_]\w*)\s*$", ln)
        if m and last_in is not None:
            last_in[4].append(m.group(2))
    return regions, out


def region_of(regions, a):
    for n, r in regions.items():
        if r["origin"] <= a < r["origin"] + r["length"]:
            return n
    return None


def classify(f):
    if f is None:
        return C_OTHER
    o = f.split("(")[-1].rstrip(")") if "(" in f else f.split("/")[-1]
    if "ltrans" in f:
        return C_DRV  # LTO object; its only symbols are TI's software ECC (ECCSW_*), the ecc library
    if f.startswith("app/"):
        return C_APP
    if "simplelink_lpf3" in f or "ble_controller" in f or "/ecc.a" in f:
        if "ble_controller" in f or "__ti__ble.a" in f:
            return C_RADIO
        if re.match(r"^(RCL|LRF|ble5|adc_noise|lrf_)", o):
            return C_RADIO
        if o.endswith("_zephyr.c.obj") or o == "dpl.c.obj":
            return C_RTOS  # TI's kernel adapter (DPL) for Zephyr
        return C_DRV
    if "/kernel/" in f or f.startswith("zephyr/kernel") or "/arch/" in f or "isr_tables" in f or "offsets" in f or "empty_file" in f:
        return C_RTOS
    if "libc" in f or "libgcc" in f or "picolibc" in f:
        return C_LIBC
    if "bluetooth" in f or "subsys__net" in f:
        return C_RADIO
    if "/drivers/" in f or "mbedtls" in f or "boards" in f or "random" in f:
        return C_DRV
    if "libzephyr.a" in f:
        if re.match(r"^(ti_radio_config|hrs|bas|ias|dis)\.c", o):
            return C_RADIO
        if re.match(r"^(sha256|aes_encrypt|aes_decrypt|hmac|hmac_prng|cmac_mode|ccm_mode|ctr_mode|ecc|ti_drivers_config|soc)\.c", o):
            return C_DRV
        return C_RTOS  # kernel services built into libzephyr: logging, printk, heap, device model, power management
    return C_OTHER


def split(path, used_flash=None, used_ram=None):
    regions, out = parse(path)
    flash = defaultdict(int); ram = defaultdict(int); ccfg = 0
    detail = defaultdict(int)
    flash_r = "FLASH"; ram_r = "RAM"; ccfg_r = "FLASH_CCFG"
    max_f = 0; max_r = 0
    for o in out:
        if o["vma"] is None or not o["size"] or NONALLOC.match(o["name"]) or o["name"] == "/DISCARD/":
            continue
        reg = region_of(regions, o["vma"])
        lreg = region_of(regions, o["lma"]) if o["lma"] is not None else None
        if reg == flash_r:
            max_f = max(max_f, o["vma"] + o["size"] - regions[flash_r]["origin"])
        if lreg == flash_r:
            max_f = max(max_f, o["lma"] + o["size"] - regions[flash_r]["origin"])
        if reg == ram_r:
            max_r = max(max_r, o["vma"] + o["size"] - regions[ram_r]["origin"])
        end = None
        for name, addr, size, f, syms in sorted(o["inputs"], key=lambda x: x[1]):
            if not size:
                continue
            if end is not None and addr < end:  # merged strings: ld lists pre-merge sizes that overlap
                size = max(0, addr + size - end); addr = end
                if not size:
                    continue
            end = addr + size
            comp = classify(f)
            if f is None:
                comp = C_OTHER
            if reg == ram_r and is_heap_or_stack(o["name"], name, syms):
                comp = C_HEAP
            if reg == ccfg_r:
                ccfg += size; continue
            if reg == flash_r:
                flash[comp] += size; detail[("F", comp, f)] += size
            elif reg == ram_r:
                ram[comp] += size; detail[("R", comp, f)] += size
                if lreg == flash_r:
                    flash[comp] += size; detail[("F", comp, f)] += size
    uf = used_flash if used_flash is not None else max_f
    ur = used_ram if used_ram is not None else max_r
    gf = uf - sum(flash.values()); gr = ur - sum(ram.values())
    return dict(regions=regions, flash=dict(flash), ram=dict(ram), ccfg=ccfg, used_flash=uf, used_ram=ur,
                map_flash_end=max_f, map_ram_end=max_r, gap_flash=gf, gap_ram=gr, detail=detail)


def is_heap_or_stack(outname, secname, syms):
    if outname != "noinit":
        return False  # every stack and heap buffer in these maps sits in noinit; elsewhere "stack" names are bookkeeping
    s = secname + " " + " ".join(syms)
    if "stack" in s.lower():
        return True
    if re.search(r"(kheap|_system_heap|z_malloc_heap_mem|heap_mem)", s):
        return True
    if outname == "noinit" and re.search(r"zephyr/kernel/(init|system_work_q)\.c", secname):
        return True  # idle thread stack and system work queue stack (unnamed in the map)
    return False


if __name__ == "__main__":
    p = sys.argv[1]
    a = [x for x in sys.argv[2:] if x != "-v"]; uf = int(a[0]) if a else None
    ur = int(a[1]) if len(a) > 1 else None
    r = split(p, uf, ur)
    print("flash", json.dumps(r["flash"]), "ccfg", r["ccfg"])
    print("ram", json.dumps(r["ram"]))
    print("used", r["used_flash"], r["used_ram"], "map ends", r["map_flash_end"], r["map_ram_end"], "gaps", r["gap_flash"], r["gap_ram"])
    if "-v" in sys.argv:
        for k, v in sorted(r["detail"].items(), key=lambda x: -x[1])[:60]:
            print(k, v)
