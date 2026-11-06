#!/usr/bin/env python3
"""Write am263p/SWEEP.md and the examples_built / examples_ran_clean rows from the sweep's results.tsv and runs.jsonl.
sweep_report.py [--copy] [--md] [--rows]
  --copy: copy results.tsv, runs.jsonl and the per-example console logs into am263p/data/sweep/ (what the page cites)"""
import collections, glob, json, os, re, shutil, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); REPO = os.path.dirname(HERE)
SRC = os.path.abspath("out/am263p/sweep"); DST = os.path.join(HERE, "data", "sweep")

def load():
    b = [l.rstrip("\n").split("\t") for l in open(os.path.join(DST, "results.tsv")) if l.strip()]
    r = {}
    for l in open(os.path.join(DST, "runs.jsonl")):
        if l.strip(): x = json.loads(l); r[x["example"]] = x
    return b, r

NOTES = {
    "ai/fan_blade_anomalydetection": "**Tag: Tool or docs.** It printed `Golden vectors matched: 0 not matched: 192` and 192 `Difference at index` lines, then its pass line, which it prints whatever the result (see the FAQ).",
    "drivers/adc/adc_dac_loopback": "**Tag: Unknown.** Uses the ADC's internal calibration channels to read the DAC; it read about 0.004 V for every DAC code. No wiring named in the example's `.c` header comment.",
    "drivers/ecap/ecap_signal_monitoring": "**Tag: Unknown.** Its `.c` header comment says \"This example doesn't need any external connections\" (an internal PWM to GPIO route); it observed no pulses.",
    "drivers/epwm/epwm_dccap": "**Tag: Unknown.** Its `.c` header comment says EPWM1A \"is routed internally via GPIO\"; no edges were captured (CAPEVT and DCCAP status 0).",
    "drivers/eqep/eqep_position_speed": "**Tag: Setup or handling.** It prints \"Please ensure EPWM to EQEP loopback is connected\": its `.c` header comment asks for eQEP0A to ePWM0A and eQEP0B to ePWM0B wires.",
}

def group(ex):
    p = ex.split("/am263px-lp/")[0].split("/")
    return p[0] if p[0] != "drivers" else "drivers/" + p[1]

if __name__ == "__main__":
    a = sys.argv[1:]
    if "--copy" in a:
        os.makedirs(os.path.join(DST, "runs"), exist_ok=True)
        for f in ("results.tsv", "runs.jsonl"): shutil.copy(os.path.join(SRC, f), DST)
        for f in glob.glob(os.path.join(SRC, "runs", "*.txt")): shutil.copy(f, os.path.join(DST, "runs"))
        for f in glob.glob(os.path.join(SRC, "*.faillog")):
            t = open(f, errors="replace").read().replace(os.path.expanduser("~"), "~")   # no home paths in the repo copy
            open(os.path.join(DST, os.path.basename(f)), "w").write(t)
    b, r = load()
    skipc = {"flash": sum(1 for x in r.values() if x["cls"] == "skipped" and "skip list" in x.get("why", "")),
             "core": sum(1 for x in r.values() if x["cls"] == "skipped" and "single-core" in x.get("why", ""))}
    fl = glob.glob(os.path.join(DST, "*.faillog"))
    nshm = sum(1 for f in fl if "memory range not found" in open(f, errors="replace").read())
    nrpm = sum(1 for f in fl if "expected expression" in open(f, errors="replace").read())
    nchanged = 0
    if os.path.exists(os.path.join(DST, "pass1-noreset", "runs.jsonl")):
        old = {json.loads(l)["example"]: json.loads(l)["cls"] for l in open(os.path.join(DST, "pass1-noreset", "runs.jsonl")) if l.strip()}
        nchanged = sum(1 for k, x in r.items() if k in old and old[k] != x["cls"])
    nb = len(b); ok = sum(1 for x in b if x[1] == "ok")
    cls = collections.Counter(x["cls"] for x in r.values())
    ran = sum(v for k, v in cls.items() if k not in ("skipped",))
    if "--md" in a:
        print("# Which of TI's LP-AM263P SDK examples build and run from a Mac?\n")
        print(f"**Short answer.** {ok} of {nb} example projects for the LaunchPad in MCU+ SDK 26.01.00.08 build from the Mac terminal with TI's makefiles. "
              f"Of the single-core examples we were willing to load, {cls['pass']} printed a pass, {cls['fail']} printed a failure, "
              f"{cls['printed']} printed output with no verdict and {cls['silent']} printed nothing in 30 s.\n\n"
              f"{cls['skipped']} were not run: {skipc['flash']} write the flash, use the security module or reset the chip on purpose, and {skipc['core']} are for cores other than the first. "
              f"The {nb - ok} that did not build are all parts of TI's multicore projects, which TI builds as one \"system\" project per example (SysConfig, TI's configuration tool, sets up all the cores together); we built them one core at a time. "
              f"{nshm} of them fail for that reason, our method. {nrpm} RPMsg builds fail in SysConfig's generated code for a reason we did not find.\n")
        print(f"**Load order matters.** {nchanged} examples gave a different result depending on whether the debugger reset the chip before loading them. Reset before every load: `tools/sysreset.js` does it from the terminal. Which examples write the flash: [FAQ](FAQ.md#which-sdk-examples-change-the-flash).\n")
        print("Terms: [vocab](../docs/vocab.md). Method: each project built with `tools/sweep_build.sh` (TI Arm Clang 5.1.1, SysConfig 1.28, the SDK makefile, no `.mcelf` step), "
              "then loaded into RAM over the debug probe right after the debugger's System Reset (`tools/sysreset.js`, so nothing carries over from the example before), and its console captured for 8 s by `tools/sweep_run.py` (30 s for any that printed nothing in 8 s). No flash was written. A first pass without the reset counted 9 failures; 4 of them passed after a reset (`data/sweep/pass1-noreset/recheck.jsonl`). "
              "Most examples need wires, a BoosterPack (TI's plug-on boards), a network or a second board to do their job; \"silent\" and \"printed\" usually mean that, not a fault. "
              "Raw records: `data/sweep/`. Problems and fixes: [FAQ](FAQ.md).\n")
        print("## By group\n\n| Group | Projects | Built | Pass | Fail | Printed, no verdict | Silent | Not run |\n|---|---|---|---|---|---|---|---|")
        g = collections.defaultdict(collections.Counter)
        for x in b:
            k = group(x[0]); g[k]["projects"] += 1; g[k]["built"] += x[1] == "ok"
            if x[0] in r: g[k][r[x[0]]["cls"]] += 1
        for k in sorted(g):
            c = g[k]; print(f"| {k} | {c['projects']} | {c['built']} | {c['pass']} | {c['fail']} | {c['printed']} | {c['silent']} | {c['skipped']} |")
        fails = [x for x in b if x[1] != "ok"]
        if fails:
            print(f"\n## Did not build\n\nAll are cores of TI's multicore \"system\" projects. {nshm} fail at the link: built alone, a core's linker file lacks the shared-memory ranges that the system project's SysConfig step defines. **Tag: Setup or handling.** The FAQ has the fix. "
              f"{nrpm} RPMsg builds, including core 0's, fail earlier: SysConfig wrote `&gIpcSharedMem[]` with an empty index into `ti_drivers_config.c`, and the compiler stops with `error: expected expression`. **Tag: Unknown.** We did not find why, and did not try building those as system projects.\n\n| Example | First error or warning |\n|---|---|")
            for x in fails:
                fl = os.path.join(DST, x[0].replace("/", "_") + ".faillog"); e = ""
                if os.path.exists(fl):
                    m = re.search(r"((?:error|warning): [^\n]{0,140})", open(fl, errors="replace").read()); e = m.group(1).replace("|", "/").strip() if m else ""
                print(f"| `{x[0]}` | {e} |")
        print("\n## Printed a failure\n\nEach was loaded right after a System Reset. Notes from the run's console log and the example's own `.c` file.\n\n| Example | Last line | Note |\n|---|---|---|")
        for k, x in sorted(r.items()):
            if x["cls"] == "fail": print(f"| `{k}` | {x.get('last','').replace('|','/')} | {NOTES.get(k.split('/am263px-lp/')[0], '')} |")
    if "--md" in a and os.path.exists(os.path.join(DST, "pass1-noreset", "runs.jsonl")):
        old = {}
        for l in open(os.path.join(DST, "pass1-noreset", "runs.jsonl")):
            if l.strip(): x = json.loads(l); old[x["example"]] = x["cls"]
        ch = [(k, old[k], x["cls"]) for k, x in sorted(r.items()) if k in old and old[k] != x["cls"]]
        print("\n## Changed by the reset\n\nThe first pass loaded each example without a System Reset, straight after the one before it. "
              "Against this pass:\n\n| Example | Without the reset | With it |\n|---|---|---|")
        for k, o, n in ch: print(f"| `{k}` | {o} | {n} |")
        print("\nSix CAN FD loopback builds go the other way: pass without the reset, nothing printed with it. For `canfd_loopback_polling` (no-RTOS) we checked by hand: "
              "after a reset it stops in the C library's `abort()` before its first console line (program counter read over the probe), and loaded straight after TI's "
              "`mcan_loopback_polling` it passes (`data/sweep/canfd-check-2026-10-11.md`, copied from the terminal). The other five were not halted. In the first pass each "
              "ran straight after another MCAN example, and `canfd_loopback_dma` passes either way. **Tag: Unknown.** Suspected: a check in their start-up fails unless an "
              "MCAN example has set the peripheral up first; we did not find which.")
        print("\n`icss_emac_lwip` (pass without the reset, silent with it) ran after `mathlib/benchmark` in the first pass, not an MCAN example; it was not looked into.")
    if "--rows" in a:
        base = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
                "every project under examples/ with an am263px-lp folder, built by am263p/tools/sweep_build.sh (SDK makefile, no .mcelf step)")
        print(f'am263p,examples_built,mcu-plus-sdk-26.01-lp,measured,{ok},{nb},1,400,"{base}",2026-10-11,am263p/data/sweep/results.tsv,build-log,"{ok} of {nb} projects built (value_hi = projects found)"')
        print(f'am263p,examples_ran_clean,mcu-plus-sdk-26.01-lp,measured,{cls["pass"]},{ran},1,400,"{base};run: single-core R5FSS0-0 projects loaded into RAM over the debugger right after a debugger System Reset (am263p/tools/sysreset.js), console captured 8 s, 30 s for those silent at 8 s (am263p/tools/sweep_run.py); '
              f'flash-writing, security and reset examples and other-core projects skipped ({cls["skipped"]})",2026-10-11,am263p/data/sweep/runs.jsonl,serial,'
              f'"{cls["pass"]} printed a pass of {ran} run; {cls["fail"]} printed a failure, {cls["printed"]} output with no verdict, {cls["silent"]} silent, {cls["load_failed"]} load failed"')
