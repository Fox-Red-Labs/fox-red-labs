#!/usr/bin/env python3
"""Build the three CC13x4 libraries TI's GitHub F2 SDK (8.33.00.16) ships only as source:
driverlib.lib, drivers_cc13x4.a and nortos_cc13x4.a, with TI Arm Clang, from the SDK's own CMake source lists.
usage: build_f2_libs.py <f2 sdk dir>   (writes the libraries where SysConfig's genlibs expects them)"""
import os, re, subprocess, sys, glob
from concurrent.futures import ThreadPoolExecutor
SDK = os.path.abspath(sys.argv[1])
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "frl")); import toolfind  # noqa: E402
CC = os.path.join(os.environ.get("FRL_TICLANG") or toolfind.find("ticlang", ["5.1.1.LTS"])["path"], "bin")   # standalone or inside CCStudio
FLAGS = ["-mcpu=cortex-m33", "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16", "-mthumb", "-Oz", "-ffunction-sections", "-fdata-sections",
         "-DDeviceFamily_CC13X4", "-w",
         "-I" + SDK + "/source", "-I" + SDK + "/kernel/nortos", "-I" + SDK + "/kernel/nortos/posix",
         "-I" + SDK + "/source/third_party/CMSIS/Core/Include", "-I" + SDK + "/source/ti/devices/cc13x4_cc26x4"]

def cmake_lists(path):
    """Expand set(NAME ...) lists with ${VAR} references and ${TI_TOOLCHAIN_NAME}."""
    txt = re.sub(r"#.*", "", open(path).read())
    out = {}
    for m in re.finditer(r"set\(\s*(\w+)\s+(.*?)\)", txt, re.S):
        items = []
        for tok in m.group(2).split():
            tok = tok.replace("${TI_TOOLCHAIN_NAME}", "ticlang")
            r = re.fullmatch(r"\$\{(\w+)\}", tok)
            items += out.get(r.group(1), []) if r else [tok]
        out[m.group(1)] = items
    return out

def build(name, base, sources, outlib):
    od = os.path.join(SDK, "build-frl", name); os.makedirs(od, exist_ok=True)
    def one(src):
        p = os.path.join(base, src)
        o = os.path.join(od, re.sub(r"[/.]", "_", src) + ".o")
        r = subprocess.run([CC + "/tiarmclang", *FLAGS, "-c", p, "-o", o], capture_output=True, text=True)
        return (src, o if r.returncode == 0 else None, r.stderr[-300:])
    with ThreadPoolExecutor(4) as ex:
        res = list(ex.map(one, sources))
    ok = [o for s, o, e in res if o]; bad = [(s, e) for s, o, e in res if not o]
    os.makedirs(os.path.dirname(outlib), exist_ok=True)
    if os.path.exists(outlib): os.remove(outlib)
    subprocess.run([CC + "/tiarmar", "rc", outlib, *ok], check=True)
    print(f"{name}: {len(ok)} of {len(sources)} compiled -> {outlib}")
    for s, e in bad: print(f"  FAILED {s}: {e.strip().splitlines()[-1] if e.strip() else ''}")

# driverlib: every .c in driverlib/ except rfc.c (the SDK's CMakeSources.cmake)
dl = os.path.join(SDK, "source/ti/devices/cc13x4_cc26x4")
build("driverlib", dl, sorted(os.path.relpath(p, dl) for p in glob.glob(dl + "/driverlib/*.c") if not p.endswith("/rfc.c")),
      dl + "/driverlib/bin/ticlang/driverlib.lib")
# nortos: SOURCES_CC13X4_CC26X4
nb = os.path.join(SDK, "kernel/nortos")
build("nortos", nb, cmake_lists(nb + "/CMakeSources.cmake")["SOURCES_CC13X4_CC26X4"], nb + "/lib/ticlang/m33f/nortos_cc13x4.a")
# drivers: SOURCES_SEC_ONLY_CC13X4_CC26X3_CC26X4 (what drivers_cc13x4 is built from)
db = os.path.join(SDK, "source/ti/drivers")
lists = cmake_lists(db + "/CMakeSources_common.cmake"); lists.update(cmake_lists(db + "/CMakeSources_cc13xx_cc26xx.cmake"))
# second pass so the cc13xx file can reference SOURCES_COMMON
txt = open(db + "/CMakeSources_cc13xx_cc26xx.cmake").read()
srcs = []
def expand(var, seen=()):
    m = re.search(r"set\(\s*" + var + r"\s+(.*?)\)", re.sub(r"#.*", "", txt), re.S)
    if not m: return lists.get(var, [])
    out = []
    for tok in m.group(1).split():
        r = re.fullmatch(r"\$\{(\w+)\}", tok)
        out += expand(r.group(1)) if r else [tok]
    return out
srcs = [s for s in expand("SOURCES_SEC_ONLY_CC13X4_CC26X3_CC26X4") if s.endswith(".c")]
build("drivers", db, sorted(set(srcs)), db + "/lib/ticlang/m33f/drivers_cc13x4.a")
