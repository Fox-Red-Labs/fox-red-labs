#!/usr/bin/env python3
"""Inventory every example in every TI SDK on this Mac, with what each one needs.

One JSON line per example in data/inventory.jsonl: sdk, family, board (the SDK's board folder), category,
example, rtos, route (how it builds from a terminal), layers (the software layers its code and SysConfig
use), tags (the peripherals and blocks it needs, in the shared vocabulary of vocab.json), and the native
build size where a sweep in this repo already built it (text, data, bss in bytes, with the file it came
from). Nothing here is built; this is what the example says about itself.

Run:  python3 inventory.py            (writes data/inventory.jsonl and prints the counts)
"""
import glob, json, os, re, sys, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
V = json.load(open(os.path.join(HERE, "vocab.json")))
HOME = os.path.expanduser("~")

# The seven SDK trees on this Mac (2026-10-10). layout says how examples are laid out.
SDKS = [
    dict(sdk="mspm0_sdk_2_11_00_07", family="MSPM0", root="/Applications/ti/mspm0_sdk_2_11_00_07", layout="simplelink"),
    dict(sdk="mspm33_sdk_1_04_00_00", family="MSPM33", root="/Applications/ti/mspm33_sdk_1_04_00_00", layout="simplelink"),
    dict(sdk="simplelink_lowpower_f3_sdk_9_21_00_36_LTS", family="F3", root="/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS", layout="simplelink"),
    dict(sdk="simplelink_wifi_sdk_10_20_00_39", family="WIFI", root="/Applications/ti/simplelink_wifi_sdk_10_20_00_39", layout="simplelink"),
    dict(sdk="am13e230x_sdk_26_01_00_03", family="AM13", root="/Applications/ti/am13e230x_sdk_26_01_00_03", layout="am13"),
    dict(sdk="mcu_plus_sdk_am263px_26_01_00_08", family="AM263P", root=HOME + "/ti/mcu_plus_sdk_am263px_26_01_00_08", layout="mcuplus"),
    dict(sdk="c2000ware_core_26_02_00_00", family="C2000", root="/Applications/ti/c2000ware_core_26_02_00_00", layout="c2000"),
    dict(sdk="mmwave_l_sdk_05_05_04_02", family="MMWAVE", root=HOME + "/ti/mmwave_l_sdk_05_05_04_02", layout="mcuplus", board_dir="xwrL64xx-evm", board="IWRL6432BOOST"),
]
SRC_EXT = (".c", ".h", ".cpp", ".cc")
MOD_RE = re.compile(r'addModule\("(/[A-Za-z0-9_./]+)"')
INC_RE = re.compile(r'#include\s*[<"]([^>"]+)[>"]')
CALL_RE = re.compile(r'\b([A-Z][A-Za-z0-9]*_)[A-Za-z0-9]+\s*\(')

def tag_path(path):
    tags = set()
    low = "/" + path.lower() + "/"
    for frag, tag in V["paths"].items():
        if frag in low:
            tags.add(tag)
    return tags

def scan_sources(files, max_bytes=400_000):
    """Includes and call prefixes of the example's own sources (not the SDK's)."""
    layers, tags, incs = set(), set(), set()
    for f in files:
        try:
            s = open(f, errors="ignore").read(max_bytes)
        except OSError:
            continue
        for inc in INC_RE.findall(s):
            incs.add(inc)
            for frag, what in V["includes"].items():
                if frag in inc:
                    (tags if what.isupper() else layers).add(what)
        for pre in set(CALL_RE.findall(s)):
            t = V["prefixes"].get(pre)
            if t:
                tags.add(t)
    return layers, tags, incs

def scan_syscfg(files):
    layers, tags, mods = set(), set(), set()
    for f in files:
        try:
            s = open(f, errors="ignore").read()
        except OSError:
            continue
        for m in MOD_RE.findall(s):
            mods.add(m)
            t = V["modules"].get(m)
            if t:
                tags.add(t)
            for pre, layer in V["module_layers"].items():
                if m.startswith(pre):
                    layers.add(layer)
    return layers, tags, mods

def walk(d, exts, depth=4):
    out = []
    base = d.rstrip("/").count("/")
    for r, dirs, files in os.walk(d):
        if r.count("/") - base >= depth:
            dirs[:] = []
        for f in files:
            if f.endswith(exts):
                out.append(os.path.join(r, f))
    return out

def route_of(tdir):
    if os.path.exists(os.path.join(tdir, "makefile")) or os.path.exists(os.path.join(tdir, "Makefile")):
        return "make"
    if glob.glob(os.path.join(tdir, "*.projectspec")):
        return "ccs"
    if os.path.exists(os.path.join(tdir, "CMakeLists.txt")):
        return "cmake"
    return "none"

def simplelink(sdk):
    """examples/<nortos|rtos>/<BOARD>/<category>/<example>[/<rtos>]/<toolchain>/"""
    for kind in ("nortos", "rtos"):
        for bdir in sorted(glob.glob(os.path.join(sdk["root"], "examples", kind, "*"))):
            board = os.path.basename(bdir)
            if board == "CUSTOM_BOARD":
                continue
            for cat in sorted(os.listdir(bdir)):
                cdir = os.path.join(bdir, cat)
                if not os.path.isdir(cdir):
                    continue
                for name in sorted(os.listdir(cdir)):
                    edir = os.path.join(cdir, name)
                    if not os.path.isdir(edir):
                        continue
                    variants = []
                    for t in glob.glob(os.path.join(edir, "*", "*")) + glob.glob(os.path.join(edir, "*")):
                        if os.path.isdir(t) and os.path.basename(t) in ("ticlang", "gcc"):
                            rtos = os.path.basename(os.path.dirname(t)) if os.path.dirname(t) != edir else kind
                            variants.append((rtos, os.path.basename(t), t))
                    if not variants:
                        yield dict(board=board, category=cat, example=name, rtos=kind, toolchain="", route="none", edir=edir, tdir=edir)
                        continue
                    # one row per RTOS flavour, ticlang preferred
                    seen = set()
                    for rtos, tc, t in sorted(variants, key=lambda v: (v[0], v[1] != "ticlang")):
                        if rtos in seen:
                            continue
                        seen.add(rtos)
                        yield dict(board=board, category=cat, example=name, rtos=rtos, toolchain=tc, route=route_of(t), edir=edir, tdir=t)

def am13(sdk):
    """examples/<category>/<example>/am13e230x_lp/<core>_<rtos>/CMakeLists.txt (lp = LaunchPad, som = module)"""
    for cm in sorted(glob.glob(os.path.join(sdk["root"], "examples", "**", "am13e230x_lp", "*", "CMakeLists.txt"), recursive=True)):
        tdir = os.path.dirname(cm)
        edir = os.path.dirname(os.path.dirname(tdir))
        parts = edir.split("/")
        rel = parts[parts.index("examples") + 1:]
        cat, name = rel[0], "/".join(rel[1:]) or rel[0]
        rtos = os.path.basename(tdir).split("_", 1)[-1]
        yield dict(board="LP_AM13E230", category=cat, example=name, rtos=rtos, toolchain="ticlang", route="cmake", edir=edir, tdir=tdir)

def mcuplus(sdk):
    """examples/<category>/<sub>/<example>/<board dir>/<core>_<rtos>/ti-arm-clang/makefile; also examples/<category>/<example>/...
    MCU+ SDK layout: the AM263Px (board dir am263px-lp) and the MMWAVE-L-SDK for the IWRL6432 (xwrL64xx-evm) share it."""
    bd = sdk.get("board_dir", "am263px-lp"); bname = sdk.get("board", "LP_AM263P")
    pats = [os.path.join(sdk["root"], "examples", "*", "*", bd, "*", "ti-arm-clang", "makefile"),
            os.path.join(sdk["root"], "examples", "*", "*", "*", bd, "*", "ti-arm-clang", "makefile")]
    for pat in pats:
        for mk in sorted(glob.glob(pat)):
            tdir = os.path.dirname(mk)
            core_rtos = os.path.basename(os.path.dirname(tdir))
            edir = os.path.dirname(os.path.dirname(os.path.dirname(tdir)))
            parts = edir.split("/")
            cat = parts[parts.index("examples") + 1]
            name = "/".join(parts[parts.index("examples") + 2:])
            core, rtos = core_rtos.rsplit("_", 1)
            yield dict(board=bname, category=cat, example=name, rtos=rtos, toolchain="ticlang", route="make", edir=edir, tdir=tdir, core=core)

def c2000(sdk):
    """driverlib/f280013x/examples/<periph>/<name>.c with CCS/<name>.projectspec"""
    base = os.path.join(sdk["root"], "driverlib", "f280013x", "examples")
    for cdir in sorted(glob.glob(os.path.join(base, "*"))):
        cat = os.path.basename(cdir)
        for ps in sorted(glob.glob(os.path.join(cdir, "CCS", "*.projectspec"))):
            name = os.path.basename(ps)[:-len(".projectspec")]
            srcs = [f for f in glob.glob(os.path.join(cdir, name + "*")) if f.endswith(SRC_EXT)]
            yield dict(board="LAUNCHXL_F2800137", category=cat, example=name, rtos="nortos", toolchain="c2000", route="ccs", edir=cdir, tdir=os.path.dirname(ps), files=srcs + glob.glob(os.path.join(cdir, name + "*.syscfg")))

# What each family calls its register-level driver layer. Generic header names (driverlib.h, device.h,
# board.h) exist in several SDKs and mean a different library in each, so the family decides the name.
FAMILY_DRIVER = {"MSPM0": "driverlib_msp", "MSPM33": "driverlib_msp", "F3": "tidrivers", "WIFI": "tidrivers",
                 "AM13": "am13_driverlib", "AM263P": "mcuplus_drivers", "C2000": "c2000_driverlib", "MMWAVE": "mcuplus_drivers"}
GENERIC = {"soc_driverlib_or_c2000", "soc_driverlib", "c2000_driverlib", "mcuplus"}

def family_layers(family, layers):
    out = set(layers)
    if out & GENERIC:
        out -= GENERIC
        out.add(FAMILY_DRIVER[family])
    if family in ("AM263P", "MMWAVE"):
        out.add("mcuplus_drivers")
    return out

def load_sizes():
    """Native sizes from every builds.jsonl a sweep in this repo wrote, keyed by the example path."""
    sizes = {}
    for f in glob.glob(os.path.join(ROOT, "**", "builds*.jsonl"), recursive=True):
        rel = os.path.relpath(f, ROOT)
        if rel.startswith("tmp/") or "/out/" in rel:
            continue
        for line in open(f, errors="ignore"):
            try:
                r = json.loads(line)
            except ValueError:
                continue
            p = r.get("path")
            if p and r.get("ok") and r.get("text") is not None:
                sizes.setdefault(p.rstrip("/"), dict(text=r["text"], data=r.get("data", 0), bss=r.get("bss", 0), from_file=rel))
    return sizes

def main():
    sizes = load_sizes()
    out = open(os.path.join(HERE, "data", "inventory.jsonl"), "w")
    counts = collections.Counter()
    n = 0
    for sdk in SDKS:
        gen = {"simplelink": simplelink, "am13": am13, "mcuplus": mcuplus, "c2000": c2000}[sdk["layout"]]
        for ex in gen(sdk):
            if "files" in ex:
                srcs = [f for f in ex["files"] if f.endswith(SRC_EXT)]
                cfgs = [f for f in ex["files"] if f.endswith(".syscfg")]
            else:
                srcs = [f for f in walk(ex["edir"], SRC_EXT, depth=3) if "/ticlang/" not in f and "/gcc/" not in f and "/iar/" not in f] \
                       or walk(ex["tdir"], SRC_EXT, depth=2)
                cfgs = walk(ex["tdir"], (".syscfg",), depth=1) or walk(ex["edir"], (".syscfg",), depth=3)
            l1, t1, incs = scan_sources(srcs)
            l2, t2, mods = scan_syscfg(cfgs)
            layers = family_layers(sdk["family"], l1 | l2)
            if ex["rtos"] == "freertos":
                layers.add("rtos_freertos")
            tags = t1 | t2 | tag_path(ex["edir"])
            rel_t = ex["tdir"]
            size = sizes.get(rel_t.rstrip("/")) or sizes.get(ex["edir"].rstrip("/"))
            row = dict(id=f'{sdk["family"]}:{ex["board"]}:{ex["category"]}/{ex["example"]}:{ex["rtos"]}',
                       sdk=sdk["sdk"], family=sdk["family"], board=ex["board"], category=ex["category"], example=ex["example"],
                       rtos=ex["rtos"], toolchain=ex["toolchain"], route=ex["route"], core=ex.get("core", ""),
                       layers=sorted(layers), tags=sorted(tags), hw_tags=sorted(t for t in tags if t in V["hardware_tags"]),
                       modules=sorted(mods), n_sources=len(srcs), path=ex["edir"].replace(HOME, "~"), tdir=ex["tdir"].replace(HOME, "~"),
                       native_size=size)
            out.write(json.dumps(row) + "\n")
            counts[(sdk["family"], ex["board"])] += 1
            n += 1
    out.close()
    print(f"{n} examples")
    for (fam, b), c in sorted(counts.items()):
        print(f"  {fam:7} {b:22} {c}")

if __name__ == "__main__":
    main()
