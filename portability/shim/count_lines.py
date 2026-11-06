#!/usr/bin/env python3
"""Count app lines per twin cell: shared vs different between the MSPM0 and F3 sources, and what each shim adds.
Reads the SDK sources only; builds nothing. Usage: count_lines.py [--md] [--csv]
Lines are non-blank lines, trimmed; the license block at the top of a file is skipped.
For each cell and direction: app lines unchanged (the app is copied byte for byte, so this is the app's line count),
shim lines the app uses (a #define or #include in the shim header whose name the app mentions),
overlay lines (board SysConfig appended to the F3 example) and -D flags the build script adds."""
import glob, os, re, sys, difflib
HERE = os.path.dirname(os.path.abspath(__file__))
M = "/Applications/ti/mspm0_sdk_2_11_00_07/examples/rtos/LP_MSPM0G3507/drivers"
F = "/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/rtos/LP_EM_CC2340R5/drivers"
CELLS = [l.split(":") for l in "adc_singlechannel:adcsinglechannel gpio_interrupt:gpiointerrupt i2c_controller:i2ccontroller i2c_target:i2ctarget i2c_tmp:i2ctmp spi_controller:spicontroller spi_peripheral:spiperipheral uart_callback:uart2callback uart_echo:uart2echo empty_freertos:empty".split()]

def code(path):
    t = open(path, errors="replace").read()
    t = re.sub(r"\A\s*/\*.*?\*/", "", t, count=1, flags=re.S)  # license block
    return [l.strip() for l in t.splitlines() if l.strip()]

def m0app(m):
    return [p for p in sorted(glob.glob(f"{M}/{m}/*.c")) if not re.search(r"main_freertos|ti_drivers_config|ti_msp_dl_config", p)][0]

def f3app(f):
    return sorted(glob.glob(f"{F}/{f}/*.c"))[0]

def shim_lines(header):
    """(name, line) for each #define / #include in a shim header."""
    out, cond = [], False
    for l in open(header).read().splitlines():
        if re.match(r"\s*#\s*ifdef\s+FR_NO_GPIO_DRIVER", l): cond = True
        elif re.match(r"\s*#\s*endif", l): cond = False
        m = re.match(r"\s*#\s*define\s+(\w+)", l)
        if m: out.append((m.group(1), l, cond))
    return out

def used(shim, text, no_gpio=False):
    """Shim #define lines whose name the app mentions (the conditional GPIO block only where the board file has no GPIO table)."""
    n = 0
    for name, _, cond in shim:
        if cond and not no_gpio: continue
        if re.search(r"\b" + re.escape(name) + r"\b", text): n += 1
    return n

def syscfg_text(f):
    return open(sorted(glob.glob(f"{F}/{f}/freertos/*.syscfg"))[0]).read()

def overlays(m_text, f):
    """Overlay lines the build script appends to the F3 example's SysConfig."""
    sc, n = syscfg_text(f), 0
    if "CONFIG_GPIO_LED_0" in m_text and "CONFIG_GPIO_LED_0" not in sc:
        n += len([l for l in open(f"{HERE}/mspm0_on_f3/led0.syscfg") if l.strip() and not l.lstrip().startswith("/*")])
    if "ti/display/DisplayUart.h" in m_text and "ti/display/Display" not in sc:
        n += len([l for l in open(f"{HERE}/mspm0_on_f3/display.syscfg") if l.strip() and not l.lstrip().startswith("/*")])
    return n

def main():
    sh_a = shim_lines(f"{HERE}/mspm0_on_f3/fr_mspm0_on_f3.h")
    sh_b = shim_lines(f"{HERE}/f3_on_mspm0/fr_f3_on_mspm0.h")
    rows = []
    for m, f in CELLS:
        mt, ft = code(m0app(m)), code(f3app(f))
        sm = difflib.SequenceMatcher(None, mt, ft, autojunk=False)
        same = sum(b.size for b in sm.get_matching_blocks())
        mtxt, ftxt = "\n".join(mt), "\n".join(ft)
        iomux = len(set(re.findall(r"\bCONFIG_\w+_IOMUX\b", mtxt)))
        rows.append(dict(cell=m + " / " + f, m0_lines=len(mt), f3_lines=len(ft), identical=same,
                         m0_only=len(mt) - same, f3_only=len(ft) - same,
                         a_shim=used(sh_a, mtxt), a_overlay=overlays(mtxt, f), a_flags=iomux,
                         b_shim=used(sh_b, ftxt, "CONFIG_GPIO_LED_0" not in open(f"{M}/{m}/ti_drivers_config.h").read())))
    return rows, len(sh_a), len(sh_b)

if __name__ == "__main__":
    rows, na, nb = main()
    cols = ["cell", "m0_lines", "f3_lines", "identical", "m0_only", "f3_only", "a_shim", "a_overlay", "a_flags", "b_shim"]
    if "--csv" in sys.argv:
        print(",".join(cols))
        for r in rows: print(",".join(str(r[c]).replace(",", ";") for c in cols))
    else:
        print("| " + " | ".join(cols) + " |"); print("|" + "---|" * len(cols))
        for r in rows: print("| " + " | ".join(str(r[c]) for c in cols) + " |")
        t = {c: sum(r[c] for r in rows) for c in cols[1:]}
        print("| total | " + " | ".join(str(t[c]) for c in cols[1:]) + " |")
        print(f"\nshim header lines (#define): MSPM0-on-F3 {na}, F3-on-MSPM0 {nb}")
