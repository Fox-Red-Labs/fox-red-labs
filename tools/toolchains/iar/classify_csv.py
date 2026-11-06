#!/usr/bin/env python3
"""Add cause_class and cause columns to harness CSVs. Every build that is not `pass` gets a class:
  no-hardware, does-not-fit, not-on-this-chip, toolchain (compiler, SDK or tool bug), port-tool (our harness), not-tried.
  classify_csv.py <csv> [<csv> ...]    rewrites in place; rules are matched on the status, error text and project name."""
import csv, sys

def cause(r):
    st, err, proj = r["status"], r["error"], r["project"]
    if st == "pass":
        return "", ""
    if "Pe852" in err:
        return "toolchain", "IAR rejects arithmetic on void * (a GNU extension) in TinyUSB's musb driver (dcd_musb.c lines 288 and 319, hcd_musb.c line 491); TI Arm Clang accepts it, and a cast to uint8_t * compiles"
    if "tusb_config.h" in err:
        return "toolchain", "the SDK ships no tusb_config.h for this example; TI Arm Clang stops at the same include"
    if "3sh_hall_foc_drv8323rh_" in proj:
        return "toolchain", "stale SDK project file: it names sources that are not in the SDK (common_files/motor_config/drv8323_3sh_config); its sibling 3sh_hall_sen_foc_drv8323rh project builds"
    if "multistep_oad" in proj:
        return "toolchain", "the MS-OAD pre-build script starts its own IarBuild with a SysConfig path of c:/ti/sysconfig_1.26.3 (a Windows install), so the project stops before it compiles"
    if "sbtool" in err:
        return "toolchain", "the image links; the post-build step runs sbtool, which the SDK ships only as a macOS binary (no source)"
    if "Ta108" in err:
        return "port-tool", "harness: the project file carries no FPU while its arguments pass --aapcs=vfp (fixed by sdk_fixes.py)"
    if "Ta105" in err:
        return "port-tool", "harness: IAR's device support lacks the MSP32 device, so the cpu falls back to ARM7TDMI (fixed by sdk_fixes.py)"
    if "LMSC" in err or "licence" in st:
        return "toolchain", "IAR licence server did not answer; rerun"
    return "not-tried", "cause not yet read from the log"

for p in sys.argv[1:]:
    rows = list(csv.DictReader(open(p, newline="")))
    fields = [f for f in rows[0].keys() if f not in ("cause_class", "cause")] + ["cause_class", "cause"]
    for r in rows:
        r["cause_class"], r["cause"] = cause(r)
    with open(p, "w", newline="") as f:
        w = csv.DictWriter(f, fields, lineterminator="\n"); w.writeheader(); w.writerows(rows)
    import collections
    print(p, dict(collections.Counter((r["status"], r["cause_class"]) for r in rows)))
