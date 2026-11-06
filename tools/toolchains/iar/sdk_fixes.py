#!/usr/bin/env python3
"""Project-file fixes the harness applies to its build-host copy of an SDK before building with iarbuild.
Each fix keeps the original beside the file (.orig) and is safe to run twice. Nothing here changes TI's source.

  AM13E230 SDK: every iar_arm/*.ewp ships FPU2=0 and GFPUCoreSlave2=0 (no FPU) while its .ipcf passes --aapcs=vfp, so
    iccarm stops with Error[Ta108] "VFP calling convention is not supported for this configuration" (330 of 332
    builds, 2026-10-24). The chip has the unit: data sheet SPRSPC3A section 1 ("Floating Point Unit (FPU), Custom
    Datapath Extension (CDE) ... DSP Extension"); the SDK's soc.h sets __FPU_PRESENT; IAR's own device file
    AM13E23019.i79 says fpu=VFPv5_SP. iarbuild uses the .ewp, not the device file, so the fix writes the values the MSPM33
    SDK's M33+FPU projects carry (FPU2=6 = VFPv5 single precision, GFPUCoreSlave2=59). The core stays
    Cortex-M33.no_dsp.no_se, which is what the device file's SIMD=false asks for.
  MSPM0 SDK, CUSTOM_BOARD MSP32C031/G031 projects: the .ipcf names a device (MSP32C031C6, MSP32G031C6, MSP32G031C8) that
    IAR's device support does not list, so iarbuild falls back to --cpu=ARM7TDMI and stops with Error[Ta105] "Multiple cpu
    modes specified" (with --thumb). The projects link the MSPM0C1105/C1106 DriverLib (a Cortex-M0+), so the fix names
    MSPM0C1105, a device IAR does list.

  sdk_fixes.py <sdk dir>        apply what fits this SDK; prints one line per fix
"""
import glob, os, re, shutil, sys


def _write(p, new, old):
    if new != old:
        if not os.path.exists(p + ".orig"):
            shutil.copy2(p, p + ".orig")
        open(p, "w", encoding="utf-8", errors="surrogateescape").write(new)
        return 1
    return 0


def am13(root):
    n = 0
    for p in sorted(glob.glob(os.path.join(root, "**", "iar_arm", "*.ewp"), recursive=True)):
        t = open(p, encoding="utf-8", errors="surrogateescape").read()
        u = t
        for name, state in (("FPU2", 6), ("GFPUCoreSlave2", 59)):
            u = re.sub(r"(<name>%s</name>\s*<version>\d+</version>\s*<state>)\d+(</state>)" % name, r"\g<1>%d\2" % state, u)
        n += _write(p, u, t)
    print("am13 FPU fix: %d project files" % n)


def msp32(root):
    n = 0
    for p in sorted(glob.glob(os.path.join(root, "examples", "nortos", "CUSTOM_BOARD", "**", "iar", "*.ipcf"), recursive=True)):
        t = open(p, encoding="utf-8", errors="surrogateescape").read()
        u = re.sub(r"(<device>\s*<name>)MSP32[A-Z0-9]*(</name>)", r"\1MSPM0C1105\2", t)
        n += _write(p, u, t)
    print("msp32 device alias: %d project files" % n)


def apply(root):
    base = os.path.basename(os.path.normpath(root))
    if base.startswith("am13"):
        am13(root)
    elif base.startswith("mspm0"):
        msp32(root)


if __name__ == "__main__":
    apply(sys.argv[1])
