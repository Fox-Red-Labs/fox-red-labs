#!/usr/bin/env python3
"""Run the built LP-AM263P SDK examples (from tools/sweep_build.sh) on the board, RAM loads only, and classify each run.
sweep_run.py [--seconds N] [--limit N] [name-regex]  |  sweep_run.py --reclassify
Reads $OUT/sweep/results.tsv, writes $OUT/sweep/runs.jsonl (resumable) and one console log per example in $OUT/sweep/runs/.
Runs only single-core R5FSS0-0 projects. Skips, by name, everything that writes the external flash (OSPI, bootloaders,
firmware update, flash file systems), provisions keys or touches the security module, or resets the chip on purpose
(self-tests, watchdog). Each load is a DSLite RAM load with the GEL
connection file; the console is opened before the load. Before each example the debugger issues a System Reset
(tools/sysreset.js), so no peripheral state carries over from the example before it (a RAM load resets only the CPU:
in a first pass without it, 4 of 9 failures passed after a reset). SWEEP_SYSRESET=0 falls back to loading a silent idle
program (bench/quiet) and draining the console instead.
Classes: pass (an "All tests have passed" or "Passed" line and no fail line), fail (a fail, error or assert line),
printed (output, no verdict), silent (nothing in the time), load_failed, skipped."""
import json, os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capload
OUT = os.environ.get("OUT", os.path.abspath("out/am263p"))
SW = os.path.join(OUT, "sweep"); RUNS = os.path.join(SW, "runs.jsonl"); LOGS = os.path.join(SW, "runs"); os.makedirs(LOGS, exist_ok=True)
SKIP = re.compile(r"ospi|flash|sbl|boot|fota|xip|hsm|otfa|secure|security|keywriter|sdl|watchdog|wdt|pbist|lbist|stc|esm|"
                  r"tamper|ecc_bus|mmcsd|fss_|sram_overlay|optishare|lockstep|reset|psram", re.I)
PASS = re.compile(r"All tests have passed|Test Passed|Passed\b|PASSED", re.I)
NOGEL = os.path.join(os.path.dirname(capload.CCXML), f"AM263Px_nogel_{capload.SERIAL}.ccxml")
SCRIPTING = "/Applications/ti/ccs2101/ccs/scripting/run.sh"; SYSRESET = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sysreset.js")
QUIET = os.path.join(SW, "quiet_silent.out")   # am263p/bench/quiet: an idle loop that prints nothing (tools/build_bench.sh)
# fail words in any case; ERROR, ASSERT and Abort only in capitals ("Reconstruction error is ..." is a value, not a failure)
FAIL = re.compile(r"(?i:\bfail(ed|ure)?\b|\bexception\b|not matched: [1-9])|\bERROR\b|ASSERT|\bAbort")  # TI's fan_blade prints its pass line regardless

def classify(body, loaded=True):
    if not loaded: return "load_failed"
    if FAIL.search(body): return "fail"
    if PASS.search(body): return "pass"
    return "printed" if body.strip() else "silent"

def reclassify():
    """Re-read every saved console log and rewrite the class in runs.jsonl (after a change to the patterns)."""
    out = []
    for l in open(RUNS):
        if not l.strip(): continue
        x = json.loads(l)
        if x["cls"] not in ("skipped", "load_failed"):
            body = open(os.path.join(LOGS, x["example"].replace("/", "_") + ".txt")).read()
            if x["cls"] != classify(body): print(f"{x['cls']} -> {classify(body)}  {x['example']}")
            x["cls"] = classify(body)
        out.append(json.dumps(x))
    open(RUNS, "w").write("\n".join(out) + "\n")

def main():
    a = sys.argv[1:]; secs = 8.0; limit = None; rx = None
    while a:
        x = a.pop(0)
        if x == "--seconds": secs = float(a.pop(0))
        elif x == "--limit": limit = int(a.pop(0))
        elif x == "--reclassify": return reclassify()
        else: rx = re.compile(x)
    done = set()
    if os.path.exists(RUNS):
        done = {json.loads(l)["example"] for l in open(RUNS) if l.strip()}
    n = 0
    for line in open(os.path.join(SW, "results.tsv")):
        f = line.rstrip("\n").split("\t")
        ex, st = f[0], f[1]
        if ex in done or (rx and not rx.search(ex)): continue
        rec = {"example": ex}
        if st != "ok": continue                       # build failures are reported from results.tsv
        if "/r5fss0-0_" not in ex + "/":
            rec.update(cls="skipped", why="not a single-core R5FSS0-0 project")
        elif SKIP.search(ex.split("/am263px-lp/")[0]):           # the example's own path, not the core folder
            rec.update(cls="skipped", why="writes flash, keys, or resets the chip (skip list)")
        else:
            img = os.path.join(SW, ex.replace("/", "_") + ".out")
            if os.environ.get("SWEEP_SYSRESET", "1") != "0":
                r = subprocess.run([SCRIPTING, SYSRESET], capture_output=True, text=True, timeout=180)
                if "FRL_SYSRESET done" not in r.stdout: print("system reset failed", r.stdout[-300:], flush=True)
                t0 = time.time()
                text, info, lines = capload.run(img, r"All tests have passed|Some tests have failed|ASSERT", secs)
            else:
                # stop the previous example first by loading the silent program
                subprocess.run([capload.DSLITE, "load", "-c", capload.CCXML, "-n", "0", "-t", "60", QUIET], capture_output=True, timeout=120)
                time.sleep(1.0)
                t0 = time.time()
                # the GEL connection file ends in a core reset that briefly restarts the program already in RAM: the
                # silent program is what restarts, and the drain discards what the previous example printed
                text, info, lines = capload.run(img, r"All tests have passed|Some tests have failed|ASSERT", secs, drain=1.5)
            body = "\n".join(l for _, l in lines if l.strip())
            open(os.path.join(LOGS, ex.replace("/", "_") + ".txt"), "w").write(body + "\n")
            cls = classify(body, info[0])
            rec.update(cls=cls, seconds=round(time.time() - t0, 1), last=body.strip().splitlines()[-1][:160] if body.strip() else "")
        with open(RUNS, "a") as fh: fh.write(json.dumps(rec) + "\n")
        print(f"{rec['cls']:12s} {ex}  {rec.get('last', rec.get('why', ''))[:90]}", flush=True)
        n += 1
        if limit and n >= limit: break

if __name__ == "__main__":
    main()
