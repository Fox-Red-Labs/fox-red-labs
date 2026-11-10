#!/usr/bin/env python3
"""Sweep of the MCU+ SDK examples for the LP-AM243: build, run, report. One script, resumable.
  sweep.py build [regex]            every examples/**/am243x-lp/<core>/ti-arm-clang project, one at a time with build_sdk_example.sh
                                    (SysConfig 1.27.0); keeps each .out in $OUT/sweep/, clears the build products; stops under 3 GB free
  sweep.py run [--seconds N] [regex]  R5FSS0-0 projects only, RAM load with ram_run.js (nothing is written to flash), console captured by
                                    ram_capture.py; skips flash, boot, key, security-module, reset and EEPROM-writing examples by name
  sweep.py report                   writes am243x/SWEEP.md and prints the examples_built / examples_ran_clean rows
Records: $OUT/sweep/results.tsv, runs.jsonl, runs/<example>.txt; `report` copies them to am243x/data/sweep/."""
import collections, glob, json, os, re, shutil, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__)); BOARD = os.path.dirname(HERE)
OUT = os.environ.get("OUT", os.path.expanduser("<scratch>/runs/arrivals/am243x"))
SW = os.path.join(OUT, "sweep"); os.makedirs(os.path.join(SW, "runs"), exist_ok=True)
E = os.path.expanduser("~/ti/mcu_plus_sdk_am243x_12_01_00_33/examples")
RES = os.path.join(SW, "results.tsv"); RUNS = os.path.join(SW, "runs.jsonl")
SIZE = "/Applications/ti/ti-cgt-armllvm_release/bin/tiarmsize"
# skip by the example's own path: flash and boot loaders, anything with keys, fuses, the security module's key store, EEPROM writers,
# the chip's own resets and the XIP examples that run from the flash the bootloader fills
SKIP = re.compile(r"ospi|flash|sbl|boot|fota|xip|otp|keywriter|keyring|hsm|tifs|sciclient|watchdog|wdt|eeprom|i2c_memory|reset|dfu|mmcsd|sdl|lockstep|ecc", re.I)
PASS = re.compile(r"All tests have passed|Test Passed|Passed\b|PASSED|test passed", re.I)
FAIL = re.compile(r"(?i:\bfail(ed|ure)?\b|\bexception\b)|\bERROR\b|ASSERT|\bAbort")
DONE = r"All tests have passed|Some tests have failed|ASSERT|Test Passed|Test Failed"

def examples():
    for line in open("/dev/stdin") if False else []: pass
    out = []
    for root, dirs, files in os.walk(E):
        if root.endswith("/ti-arm-clang") and "/am243x-lp/" in root and "gcc" not in root:
            out.append(os.path.relpath(root, E)[:-len("/ti-arm-clang")])
    return sorted(out)

def build(rx):
    done = {l.split("\t")[0] for l in open(RES)} if os.path.exists(RES) else set()
    for d in examples():
        if d in done or (rx and not re.search(rx, d)): continue
        free = shutil.disk_usage(os.path.expanduser("~")).free / 2**30
        if free < 3: sys.exit(f"STOP: {free:.1f} GB free")
        r = subprocess.run([os.path.join(HERE, "build_sdk_example.sh"), os.path.join(E, d)], capture_output=True, text=True).stdout.strip().splitlines()[-1].split()
        n = d.replace("/", "_"); st, sec, out = r[1], r[2], r[3] if len(r) > 3 else ""
        if st == "ok":
            sz = subprocess.run([SIZE, out], capture_output=True, text=True).stdout.strip().splitlines()[-1].split()[:3]
            shutil.copy(out, os.path.join(SW, n + ".out"))
            row = [d, "ok", sec] + sz + [str(os.path.getsize(out))]
        else:
            lg = os.path.join(E, d, "ti-arm-clang", "build.log")
            if os.path.exists(lg): shutil.copy(lg, os.path.join(SW, n + ".faillog"))
            row = [d, "fail", sec, "", "", "", ""]
        open(RES, "a").write("\t".join(row) + "\n"); print(*row[:3], flush=True)
        subprocess.run("rm -rf obj generated *.map *.lnkxml build.log *.mcelf* *.optishare.out 2>/dev/null", shell=True, cwd=os.path.join(E, d, "ti-arm-clang"))
    print("BUILD DONE")

def classify(body, loaded=True):
    if not loaded: return "load_failed"
    if FAIL.search(body): return "fail"
    if PASS.search(body): return "pass"
    return "printed" if body.strip() else "silent"

def run(rx, secs):
    done = {json.loads(l)["example"] for l in open(RUNS) if l.strip()} if os.path.exists(RUNS) else set()
    py = os.path.expanduser("~/.venvs/csi/bin/python")
    for line in open(RES):
        f = line.rstrip("\n").split("\t"); ex = f[0]
        if f[1] != "ok" or ex in done or (rx and not re.search(rx, ex)): continue
        rec = {"example": ex}
        if "/r5fss0-0_" not in ex + "/": rec.update(cls="skipped", why="not an R5FSS0-0 project")
        elif SKIP.search(ex.split("/am243x-lp/")[0]): rec.update(cls="skipped", why="writes flash or keys, resets the chip, or needs a bootloader (skip list)")
        else:
            n = ex.replace("/", "_"); log = os.path.join(SW, "runs", n + ".txt"); t0 = time.time()
            r = subprocess.run([py, os.path.join(HERE, "ram_capture.py"), os.path.join(SW, n + ".out"), log, "--secs", str(secs), "--until", DONE.split("|")[0]], capture_output=True, text=True)
            ok = r.returncode == 0 and os.path.exists(log)
            body = open(log, errors="replace").read() if ok else ""
            if ok and not body.strip() and secs < 30:   # a second, longer look at the ones that printed nothing
                r = subprocess.run([py, os.path.join(HERE, "ram_capture.py"), os.path.join(SW, n + ".out"), log, "--secs", "30"], capture_output=True, text=True)
                body = open(log, errors="replace").read() if os.path.exists(log) else ""
            lines = [l for l in body.splitlines() if l.strip()]
            rec.update(cls=classify(body, ok), seconds=round(time.time() - t0, 1), last=lines[-1][:160] if lines else (r.stdout + r.stderr)[-160:].strip() if not ok else "")
        open(RUNS, "a").write(json.dumps(rec) + "\n"); print(f"{rec['cls']:12s} {ex}  {rec.get('last', rec.get('why', ''))[:90]}", flush=True)
    print("RUN DONE")

def group(ex):
    p = ex.split("/am243x-lp/")[0].split("/"); return p[0] if p[0] != "drivers" else "drivers/" + p[1]

def report():
    dst = os.path.join(BOARD, "data", "sweep"); os.makedirs(os.path.join(dst, "runs"), exist_ok=True)
    for f in ("results.tsv", "runs.jsonl"): shutil.copy(os.path.join(SW, f), dst)
    for f in glob.glob(os.path.join(SW, "runs", "*.txt")): shutil.copy(f, os.path.join(dst, "runs"))
    for f in glob.glob(os.path.join(SW, "*.faillog")):
        t = open(f, errors="replace").read().replace(os.path.expanduser("~"), "~")
        t = re.sub(r"\(ssh://[^)]*\)", "(compiler build string removed)", t)
        open(os.path.join(dst, os.path.basename(f)), "w").write(t)
    b = [l.rstrip("\n").split("\t") for l in open(RES) if l.strip()]
    r = {json.loads(l)["example"]: json.loads(l) for l in open(RUNS) if l.strip()}
    nb = len(b); ok = sum(1 for x in b if x[1] == "ok"); cls = collections.Counter(x["cls"] for x in r.values())
    ran = sum(v for k, v in cls.items() if k != "skipped")
    skipf = sum(1 for x in r.values() if x["cls"] == "skipped" and "skip list" in x.get("why", "")); skipc = cls["skipped"] - skipf
    L = [f"# Which of TI's LP-AM243 SDK examples build and run from a Mac?\n",
         f"**Short answer.** {ok} of {nb} example projects for the LaunchPad in MCU+ SDK 12.01.00.33 build from the Mac terminal with TI's makefiles. Of the {ran} R5FSS0-0 projects we loaded, {cls['pass']} printed a pass, {cls['fail']} printed a failure, {cls['printed']} printed output with no verdict and {cls['silent']} printed nothing in 30 s; {cls['load_failed']} did not load.\n",
         f"{cls['skipped']} were not run: {skipf} write the flash or keys, reset the chip or need a bootloader, and {skipc} are for cores other than the first.\n",
         "Terms: **R5F** is the chip's Arm Cortex-R5F real-time core; **SysConfig** is TI's configuration tool. Method: each project built with `tools/sweep.py build` (TI Arm Clang 4.0.4.LTS, SysConfig 1.27.0, the SDK makefile, no `.mcelf` step), then loaded into RAM over the debug probe with `tools/ram_run.js` (switches as shipped, nothing written to flash) and its console read by `tools/ram_capture.py` for 8 s (30 s for any that printed nothing). The chip is not reset between examples, so what one leaves on can reach the next; `ram_run.js` stops the tick timer and closes the interrupt controller each time. Most examples need wires, a BoosterPack (TI's plug-on boards), a network or a second board; \"silent\" and \"printed\" usually mean that, not a fault. Raw records: `data/sweep/`. Problems and fixes: [FAQ](FAQ.md).\n",
         "## By group\n\n| Group | Projects | Built | Pass | Fail | Printed, no verdict | Silent | Not run |\n|---|---|---|---|---|---|---|---|"]
    g = collections.defaultdict(collections.Counter)
    for x in b:
        k = group(x[0]); g[k]["p"] += 1; g[k]["b"] += x[1] == "ok"
        if x[0] in r: g[k][r[x[0]]["cls"]] += 1
    for k in sorted(g):
        c = g[k]; L.append(f"| {k} | {c['p']} | {c['b']} | {c['pass']} | {c['fail']} | {c['printed']} | {c['silent']} | {c['skipped']} |")
    fails = [x for x in b if x[1] != "ok"]
    if fails:
        def first(x):
            fl = os.path.join(dst, x[0].replace("/", "_") + ".faillog")
            return open(fl, errors="replace").read() if os.path.exists(fl) else ""
        nshm = sum("memory range not found" in first(x) for x in fails); nexp = sum("expected expression" in first(x) for x in fails); nmk = sum("no makefile" in first(x) for x in fails)
        L.append(f"\n## Did not build\n\n{nshm} fail at the link: TI builds the other cores of its multi-core examples as one \"system\" project, and built alone a core's linker file lacks the shared-memory ranges the system step defines (building a core with `SYSTEM_FLAG=true` after the system's `syscfg` step, as `tools/ipc_build.sh` does, avoids it). {nexp} stop earlier: SysConfig wrote an empty index into `ti_drivers_config.c` and the compiler says `error: expected expression` (RPMsg, SafeIPC and XIP benchmark examples). {nmk} has no makefile in the SDK folder.\n\n| Example | First error or warning |\n|---|---|")
        for x in fails:
            fl = os.path.join(dst, x[0].replace("/", "_") + ".faillog"); e = ""
            if os.path.exists(fl):
                m = re.search(r"((?:error|warning): [^\n]{0,140})", open(fl, errors="replace").read()); e = m.group(1).replace("|", "/").strip() if m else ""
            L.append(f"| `{x[0]}` | {e} |")
    for title, c in (("Printed a failure", "fail"), ("Did not load", "load_failed"), ("Printed nothing in 30 s", "silent")):
        rows = [(k, x) for k, x in sorted(r.items()) if x["cls"] == c]
        if rows:
            L.append(f"\n## {title}\n\n| Example | Last line |\n|---|---|")
            for k, x in rows: L.append(f"| `{k}` | {x.get('last','').replace('|','/')} |")
    eep = sorted(k for k in r if r[k]["cls"] != "skipped" and re.search(r"EEPROM", open(os.path.join(dst, "runs", k.replace("/", "_") + ".txt"), errors="replace").read()) and re.search(r"[Ww]rit", open(os.path.join(dst, "runs", k.replace("/", "_") + ".txt"), errors="replace").read()))
    if eep:
        L.append("\n## Wrote to the board's EEPROM\n\nThese ran before the skip list covered them (name `i2c_memory`): each writes ten bytes (0 to 9) to the I2C EEPROM at address 0x50, offset 0x400, and reads them back. Offset 0 of that EEPROM reads 0xFF (`i2c_read`), so the start of the part, where a board-ID record would sit, was not touched; the old contents at 0x400 were not read first. Do not run them on a board whose EEPROM you need.\n")
        for k in eep: L.append(f"- `{k}`")
    open(os.path.join(BOARD, "SWEEP.md"), "w").write("\n".join(L) + "\n")
    base = ("compiler=ticlang-4.0.4.LTS;sdk=mcu_plus_sdk_am243x_12_01_00_33;sysconfig=1.27.0;machine=macos x86_64;every project under examples/ with an am243x-lp folder (gcc variants excluded), "
            "built by am243x/tools/sweep.py (SDK makefile, no .mcelf step)")
    print(f'am243x,examples_built,mcu-plus-sdk-12.01-lp,measured,{ok},{nb},1,,"{base}",{time.strftime("%Y-%m-%d")},am243x/data/sweep/results.tsv,build-log,"{ok} of {nb} projects built (value_hi = projects found)"')
    print(f'am243x,examples_ran_clean,mcu-plus-sdk-12.01-lp,measured,{cls["pass"]},{ran},1,800,"{base};run: R5FSS0-0 projects loaded into RAM by am243x/tools/ram_run.js with SW4 as shipped, no reset between examples, console captured 8 s, 30 s for those silent at 8 s; flash, key, reset and other-core projects skipped ({cls["skipped"]})",{time.strftime("%Y-%m-%d")},am243x/data/sweep/runs.jsonl,serial,"{cls["pass"]} printed a pass of {ran} run; {cls["fail"]} printed a failure, {cls["printed"]} output with no verdict, {cls["silent"]} silent, {cls["load_failed"]} load failed"')

if __name__ == "__main__":
    a = sys.argv[1:]; mode = a.pop(0) if a else ""
    secs = 8.0; rx = None
    while a:
        x = a.pop(0)
        if x == "--seconds": secs = float(a.pop(0))
        else: rx = x
    {"build": lambda: build(rx), "run": lambda: run(rx, secs), "report": report}.get(mode, lambda: sys.exit(__doc__))()
