#!/usr/bin/env python3
"""Look at an image before it is flashed and say whether it is likely to lock the board out.

  python3 tools/preflight_image.py <board id> <image.out> [--map image.map] [--objdump PATH] [--json]

Exit codes: 0 safe (no check fired), 1 warn (a heuristic fired), 2 refuse (the image does not fit this chip).
Checks, with how strong each is:
  (c) wrong chip, REFUSE: the ELF machine type, any loadable segment outside the chip's flash/SRAM, a first-word stack
      pointer outside SRAM, a reset vector outside flash, or a linker map whose flash is larger than the chip's.
      Memory regions come from data/recovery.json ("memory", read from the board's own linker map) or, where none is
      recorded, from the flash and RAM sizes in data/boards.csv assuming flash at 0x0 and RAM at 0x20000000 (ARM only).
  (a) debug pins, WARN, heuristic: the board's SWD pins (data/recovery.json "debug_pins") are reconfigured when the
      disassembly builds or loads the address of their IOMUX pin-control register. Misses code that computes the address
      at run time; can fire on a read of the register.
  (b) sleep with no wake source, WARN, heuristic: the disassembly builds the address of the power-mode register (or the
      debug info names setPowerPolicy<mode>) and the image defines no GPIO interrupt handler (a handler whose address is
      not the default handler's). The SDK documents a GPIO as the STANDBY wake source; a timer-only wake is flagged on
      purpose. It cannot see a wake source set up in a way that needs no handler (a wake by event).
(a) and (b) need a disassembler (arm-none-eabi-objdump, tiarmobjdump or llvm-objdump); without one they are skipped and say so.
"""
import csv, json, pathlib, re, shutil, struct, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
EM = {"arm": 40, "riscv": 243}
OBJDUMPS = ["arm-none-eabi-objdump", "tiarmobjdump", "llvm-objdump", "objdump",
            "/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin/arm-none-eabi-objdump",
            "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmobjdump"]
GPIO_HANDLER = re.compile(r"^(GPIO[A-Z0-9]*|GROUP\d)_IRQHandler$")


def parse_elf(path):
    d = pathlib.Path(path).read_bytes()
    if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
        raise ValueError("not a 32-bit little-endian ELF file")
    machine = struct.unpack_from("<H", d, 18)[0]
    phoff, shoff = struct.unpack_from("<II", d, 28)
    phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from("<HHHHH", d, 42)
    segs = []
    for i in range(phnum):
        t, off, va, pa, fs, ms, fl, al = struct.unpack_from("<8I", d, phoff + i * phentsize)
        if t == 1 and ms:
            segs.append({"vaddr": va, "paddr": pa, "filesz": fs, "memsz": ms, "off": off})
    secs = []
    for i in range(shnum):
        nm, ty, fl, ad, off, sz, lk, inf, al, es = struct.unpack_from("<10I", d, shoff + i * shentsize)
        secs.append({"name_off": nm, "type": ty, "addr": ad, "off": off, "size": sz, "link": lk, "entsize": es})
    strsec = secs[shstrndx] if secs else None
    def name(o, base):
        e = d.index(b"\0", base + o)
        return d[base + o:e].decode(errors="replace")
    for s in secs:
        s["name"] = name(s["name_off"], strsec["off"]) if strsec else ""
    syms = {}
    for s in secs:
        if s["type"] == 2 and s["entsize"]:  # SYMTAB
            strs = secs[s["link"]]
            for j in range(s["size"] // s["entsize"]):
                nm, val, sz, info, oth, shn = struct.unpack_from("<IIIBBH", d, s["off"] + j * 16)
                if nm:
                    syms.setdefault(name(nm, strs["off"]), val & ~1)
    return {"data": d, "machine": machine, "segs": segs, "secs": secs, "syms": syms}


def read_at(elf, addr, n):
    for s in elf["segs"]:
        if s["filesz"] and s["paddr"] <= addr and addr + n <= s["paddr"] + s["filesz"]:
            o = s["off"] + addr - s["paddr"]
            return elf["data"][o:o + n]
    return None


def regions_for(board, csvrow):
    m = board.get("memory")
    if m:
        fl = [(n, int(a, 16), int(a, 16) + sz) for n, a, sz in m["regions"] if n.startswith("FLASH")]
        ram = [(n, int(a, 16), int(a, 16) + sz) for n, a, sz in m["regions"] if not n.startswith("FLASH")]
        cfg = [(n, int(a, 16), int(a, 16) + sz) for n, a, sz in m.get("config", [])]
        return fl, ram, cfg, m.get("arch", "arm"), "data/recovery.json memory (" + m["source"] + ")"
    try:
        fk, rk = float(csvrow["flash_kb"]), float(csvrow["ram_kb"])
    except (TypeError, ValueError):
        return [], [], [], None, "no memory sizes recorded"
    if fk <= 0 or "Cortex-M" not in csvrow.get("core", ""):
        return [], [], [], None, "no usable memory sizes for this core in data/boards.csv"
    return ([("FLASH", 0, int(fk * 1024))], [("SRAM", 0x20000000, 0x20000000 + int(rk * 1024))], [], "arm",
            "data/boards.csv sizes, assuming flash at 0x0 and RAM at 0x20000000 (heuristic)")


def inside(lo, hi, regs):
    return any(a <= lo and hi <= b for _, a, b in regs)


def parse_map(path):
    rows = []
    for ln in pathlib.Path(path).read_text(errors="ignore").splitlines():
        m = re.match(r"\s+([A-Za-z_][\w]*)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+[0-9a-f]{8}\s+\S+", ln)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
        elif ln.startswith("SEGMENT ALLOCATION"):
            break
    return rows


def find_objdump(given):
    for c in ([given] if given else []) + OBJDUMPS:
        p = shutil.which(c) or (c if pathlib.Path(c).exists() else None)
        if p:
            return p
    return None


def constants(objdump, image):
    out = subprocess.run([objdump, "-d", image], capture_output=True, text=True, timeout=300).stdout.splitlines()
    consts, last_movw, stores = set(), {}, []
    for i, ln in enumerate(out):
        m = re.search(r"\bmovw\s+(r\d+), #(\d+)", ln)
        if m:
            last_movw[m.group(1)] = (int(m.group(2)), i)
        m = re.search(r"\bmovt\s+(r\d+), #(\d+)", ln)
        if m and m.group(1) in last_movw and i - last_movw[m.group(1)][1] <= 6:
            c = (int(m.group(2)) << 16) | last_movw[m.group(1)][0]
            consts.add(c)
            stores.append((c, m.group(1), i))
        m = re.search(r"\.word\s+0x([0-9a-f]{8})\b", ln)
        if m:
            consts.add(int(m.group(1), 16))
    return consts, stores, out, any(re.search(r"\bwfi\b", x) for x in out)


def stored_values(addr, stores, out):
    """Immediate values stored through a register that was loaded with addr (best effort, small window)."""
    vals = []
    for c, reg, i in stores:
        if c != addr:
            continue
        for j in range(i + 1, min(i + 8, len(out))):
            m = re.search(r"\bstr\s+(r\d+), \[%s(?:, #0)?\]" % reg, out[j])
            if m:
                for k in range(j - 1, max(i, j - 6), -1):
                    mv = re.search(r"\bmovs?(?:\.w)?\s+%s, #(\d+)" % m.group(1), out[k])
                    if mv:
                        vals.append(int(mv.group(1)))
                        break
                break
    return vals


def run(board_id, image, mapfile=None, objdump=None):
    rec = json.loads((ROOT / "data" / "recovery.json").read_text())
    if board_id not in rec["boards"]:
        raise SystemExit(f"unknown board id {board_id}; ids are the keys of data/recovery.json")
    board = rec["boards"][board_id]
    csvrow = next(r for r in csv.DictReader((ROOT / "data" / "boards.csv").open()) if r["board"] == board_id)
    try:
        elf = parse_elf(image)
    except (OSError, ValueError, struct.error) as e:
        raise SystemExit(f"cannot read {image}: {e}")
    F = []  # (level, check, text)
    fl, ram, cfg, arch, src = regions_for(board, csvrow)
    notes = [f"memory source: {src}"]

    # (c) wrong chip
    if arch:
        if elf["machine"] != EM.get(arch, -1):
            F.append((2, "c", f"ELF machine type {elf['machine']} is not {arch} ({EM.get(arch)}) for {board_id}"))
        allr = fl + ram + cfg
        for s in elf["segs"]:
            for what, lo, n in (("load", s["paddr"], s["filesz"]), ("run", s["vaddr"], s["memsz"])):
                if n and not inside(lo, lo + n, allr):
                    F.append((2, "c", f"segment {what} range 0x{lo:08x}-0x{lo + n:08x} is outside this chip's flash and SRAM"))
            if s["filesz"] and any(inside(s["paddr"], s["paddr"] + s["filesz"], [r]) for r in cfg):
                F.append((1, "c", f"image writes a configuration region at 0x{s['paddr']:08x} (NONMAIN, BCR or BSL area); check it is intended"))
        vt = read_at(elf, fl[0][1], 8) if fl else None
        if vt:
            sp, rv = struct.unpack("<II", vt)
            if not any(a < sp <= b for _, a, b in ram):
                F.append((2, "c", f"vector table: stack pointer 0x{sp:08x} is outside this chip's SRAM"))
            if not (rv & 1) or not inside(rv & ~1, (rv & ~1) + 2, fl):
                F.append((2, "c", f"vector table: reset vector 0x{rv:08x} is not an odd address inside this chip's flash"))
        elif fl:
            F.append((1, "c", "no data loaded at the flash base, so no vector table was checked"))
    else:
        notes.append("check (c) skipped: no memory map for this board")
    mp = mapfile or (str(pathlib.Path(image).with_suffix(".map")) if pathlib.Path(image).with_suffix(".map").exists() else None)
    if mp and fl:
        rows = parse_map(mp)
        mflash = sum(l for n, a, l in rows if n.upper().startswith("FLASH"))
        chip_flash = sum(b - a for _, a, b in fl)
        if mflash > chip_flash:
            F.append((2, "c", f"linker map {pathlib.Path(mp).name} describes {mflash // 1024} kB of flash; this chip has {chip_flash // 1024} kB"))
        mram = sum(l for n, a, l in rows if n.upper().startswith(("SRAM", "RAM")))
        chip_ram = sum(b - a for _, a, b in ram)
        if mram > chip_ram:
            F.append((2, "c", f"linker map describes {mram // 1024} kB of RAM; this chip has {chip_ram // 1024} kB"))
    elif fl:
        notes.append("no .map beside the image; the map-size check was skipped")

    # (a) and (b) need the disassembly
    od = find_objdump(objdump)
    if not od:
        notes.append("checks (a) and (b) skipped: no objdump found")
    else:
        consts, stores, out, has_wfi = constants(od, image)
        dp = board.get("debug_pins")
        if dp:
            base = int(dp["iomux_base"], 16)
            for pin, nm in ((dp["swdio"], "SWDIO"), (dp["swclk"], "SWCLK")):
                a = base + 4 * int(re.sub(r"\D", "", pin))
                if a in consts:
                    F.append((1, "a", f"the image builds the address of {pin} ({nm}) pin control, 0x{a:08x}: it may reconfigure a debug pin (heuristic, can also be a read)"))
        else:
            notes.append("check (a) skipped: no debug pin data for this board")
        pm = board.get("power_mode")
        if pm:
            a = int(pm["register"], 16)
            raw = elf["data"]
            named = sorted(set(m.group(0).decode() for m in re.finditer(rb"setPowerPolicy(?:STANDBY\d?|STOP\d?|SHUTDOWN)", raw)))
            hit = a in consts
            if hit or named:
                vals = stored_values(a, stores, out)
                syms = elf["syms"]
                dflt = syms.get("Default_Handler")
                gpio = [n for n, v in syms.items() if GPIO_HANDLER.match(n) and v != dflt]
                what = []
                if hit:
                    what.append(f"builds the power-mode register address 0x{a:08x}" + (f" and stores {vals}" if vals else ""))
                if named:
                    what.append("debug info names " + ", ".join(named))
                if not gpio:
                    F.append((1, "b", "the image sets a power mode (" + "; ".join(what) + (", with WFI" if has_wfi else "") +
                                      ") and defines no GPIO interrupt handler, so no GPIO wake source is visible (heuristic)"))
                else:
                    notes.append("sets a power mode and defines GPIO handler(s) " + ", ".join(gpio) + ": wake source not verified")
        else:
            notes.append("check (b) skipped: no power-mode register recorded for this board")
    level = max([f[0] for f in F], default=0)
    return level, F, notes


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    opt = lambda k: argv[argv.index(k) + 1] if k in argv else None
    if "--map" in argv: args.remove(opt("--map"))
    if "--objdump" in argv: args.remove(opt("--objdump"))
    if len(args) != 2:
        print(__doc__)
        return 2
    try:
        level, F, notes = run(args[0], args[1], opt("--map"), opt("--objdump"))
    except SystemExit as e:
        print(e.code, file=sys.stderr)
        return 2
    if "--json" in argv:
        print(json.dumps({"level": level, "findings": F, "notes": notes}))
    else:
        for lv, ck, tx in F:
            print(("REFUSE" if lv == 2 else "WARN  ") + f" ({ck}) {tx}")
        for n in notes:
            print("note:", n)
        print({0: "SAFE: no check fired (this is not proof; the checks are heuristic)", 1: "WARN: review the findings before flashing",
               2: "REFUSE: do not flash this image to this board"}[level])
    return level


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
