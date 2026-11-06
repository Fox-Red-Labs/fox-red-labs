# Regenerate mspm0g3507/memory-breakdown.json from the linker maps (the same method as mspm0l1306/tools/memory/gen.py,
# with the G3507's regions: FLASH 0x0 length 0x20000, SRAM 0x20200000 length 0x8000).
# Run from the repository root: python3 mspm0g3507/tools/memory_gen.py
import collections, json, pathlib, re, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mspm0l1306" / "tools" / "memory"))
import gnuparse

APP, RT, DRV, CL, HS, OT = ("Your app (the example's own code)", "RTOS", "TI drivers and device library",
                            "C library and compiler runtime", "Heap and stacks", "Other")
NAMES = [RT, "Radio stack", DRV, CL, APP, HS, OT]
FLASH_END, RAM0, RAM1 = 0x20000, 0x20200000, 0x20208000


def ti_parse(path):
    txt = (ROOT / path).read_text()
    fu = int(re.search(r"^\s+FLASH\s+(\w+)\s+(\w+)\s+(\w+)", txt, re.M).group(3), 16)
    ru = int(re.search(r"^\s+SRAM\s+(\w+)\s+(\w+)\s+(\w+)", txt, re.M).group(3), 16)
    for reg in ("BCR_CONFIG", "BSL_CONFIG"):
        assert int(re.search(reg + r"\s+\w+\s+\w+\s+(\w+)", txt).group(1), 16) == 0
    sec = txt.split("SECTION ALLOCATION MAP")[1].split("MODULE SUMMARY")[0]
    fl, ra = collections.defaultdict(int), collections.defaultdict(int)
    lib = osec = None
    for line in sec.splitlines():
        m = re.match(r"^(\S+)\s+\d+\s+([0-9a-f]{8})\s+([0-9a-f]{8})", line)
        if m:
            osec, lib = m.group(1), None
            continue
        if line and not line.startswith(" "):
            osec = line.split()[0] if line.split() else osec
            lib = None
            continue
        m = re.match(r"^\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(.*)$", line)
        if not m:
            continue
        a, n, rest = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        if n == 0:
            continue
        if a < FLASH_END:
            reg = fl
        elif RAM0 <= a < RAM1:
            reg = ra
        else:
            continue
        if "HOLE" in rest:
            c, obj = (HS if osec in (".stack", ".sysmem") else OT), "hole"
        else:
            mm = re.match(r"(?:(\S+)\s+)?:\s+(\S+)\s+\((.*)\)", rest)
            if mm:
                if mm.group(1):
                    lib = mm.group(1)
                obj = mm.group(2)
            else:
                lib = None
                mm = re.match(r"(\S+)\s+\((.*)\)", rest)
                obj = mm.group(1) if mm else "(" + (re.match(r"\((.*?)\)", rest) or [0, "?"])[1] + ")"
            if osec in (".stack", ".sysmem"):
                c = HS
            elif obj.startswith("(.cinit") or obj.startswith("(__TI"):
                c = OT
            elif lib and re.search(r"freertos|nortos_", lib):
                c = RT
            elif lib and re.search(r"driverlib|drivers_mspm0", lib):
                c = DRV
            elif lib and re.search(r"libc|libsysbm|libclang_rt|libm", lib):
                c = CL
            elif re.match(r"(startup|ticlang_startup|ti_msp_dl_config|ti_drivers_config|cfg\.o|st\.o)\b", obj):   # cfg.o and st.o: the touch build's names for the config and startup objects (ports/bitnetmcu/touch/build.sh)
                c = DRV
            else:
                c = APP
        reg[c] += n
    assert sum(fl.values()) == fu and sum(ra.values()) == ru, (path, sum(fl.values()), fu, sum(ra.values()), ru)
    return fu, ru, fl, ra


def full(d):
    return {n: d.get(n, 0) for n in NAMES}


builds = []


def ti(id, label, sdk, path):
    fu, ru, fl, ra = ti_parse(path)
    builds.append(dict(id=id, label=label, sdk=sdk, source=path, flash=full(fl), ram=full(ra),
                       check=dict(flash_used_in_map=fu, flash_sum=sum(fl.values()), ram_used_in_map=ru, ram_sum=sum(ra.values()),
                                  flash_region=dict(origin="0x0", length=0x20000), nonmain_used=0, unexplained_flash=0, unexplained_ram=0)))


SDK = "MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1"
W = "portability/three-ways/maps/"
ti("driverlib", "DriverLib, no OS (a small app)", SDK + ", no RTOS, portability/three-ways/driverlib", W + "driverlib-g3507.map")
ti("tidrivers_nortos", "TI Drivers, no OS", SDK + ", no RTOS, portability/three-ways/tidrivers", W + "tidrivers-g3507-nortos.map")
ti("tidrivers_freertos", "TI Drivers on FreeRTOS", SDK + ", FreeRTOS kernel built by our own script, portability/three-ways/tidrivers", W + "tidrivers-g3507-freertos.map")
ti("digit_touch", "Handwritten-digit demo (screen, touch, neural net)", SDK + ", -O3, ports/bitnetmcu/touch", "ports/bitnetmcu/touch/out/digit_touch.map")
fl, ra, gf, gr, of, orr, fo, ro = gnuparse.account(str(ROOT / W / "zephyr-g3507.map"), 12704, 3992)
assert of == 0 and orr == 0 and sum(fl.values()) == 12704 and sum(ra.values()) == 3992
builds.append(dict(id="zephyr", label="Zephyr blinky-class app", sdk="Zephyr 4.4.2, GCC 12.2.0 (Zephyr SDK 0.17.4), upstream lp_mspm0g3507 board, portability/three-ways/zephyr",
                   source=W + "zephyr-g3507.map", flash=full(fl), ram=full(ra),
                   check=dict(flash_used_in_map=12704, flash_sum=sum(fl.values()), ram_used_in_map=3992, ram_sum=sum(ra.values()),
                              flash_region=dict(origin="0x0", length=0x20000), nonmain_used=0, unexplained_flash=0, unexplained_ram=0)))
out = dict(board="LP-MSPM0G3507", chip="MSPM0G3507", flash_total=131072, ram_total=32768, units="bytes", generated="2026-10-15",
           method="Each object and library in the linker map was put in one component. TI maps: SECTION ALLOCATION MAP entries placed by address in FLASH or SRAM, grouped by library or object (driverlib.a, drivers_mspm0g1x0x_g3x0x.a, the SysConfig-generated ti_msp_dl_config and ti_drivers_config and the startup file go to TI drivers and device library; freertos.lib and the no-RTOS layer nortos_mspm0g1x0x_g3x0x.a to RTOS; libc.a, libsysbm.a and libclang_rt.builtins.a to C library and compiler runtime; the app's own objects and the neural-net model to Your app), the .stack and .sysmem sections to Heap and stacks, and the compressed start-up tables (.cinit) and alignment holes to Other. Zephyr GNU map: every input section was added up by address (run address in RAM; initialized data also counted at its load address in flash). Zephyr kernel, arch, drivers and the C-library glue went to RTOS; picolibc and libgcc to C library and compiler runtime; app/libapp.a to Your app; TI's hal_ti driverlib to TI drivers and device library; stacks to Heap and stacks. Every build's components add up exactly to the used FLASH and RAM the linker or west reported. The chip has no radio, so no build has a radio stack. None of these builds writes the chip's boot configuration area (NONMAIN).",
           builds=builds)
(ROOT / "mspm0g3507" / "memory-breakdown.json").write_text(json.dumps(out, indent=2) + "\n")
for b in builds:
    print(b["id"], b["check"]["flash_used_in_map"], b["check"]["ram_used_in_map"], b["flash"], b["ram"])
