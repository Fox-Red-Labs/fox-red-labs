"""One CCStudio scripting session: load an image into a core, run it, halt, read the PC and a few memory words.
Run with CCStudio's Python:  /Applications/ti/ccs2101/ccs/scripting/python/run.sh dss_run_read.py <ccxml> <core regex> <image> <seconds> <addr,words> [<addr,words> ...]
Prints one JSON line. Used where a fresh DSLite connection resets the view (LP-AM263P RAM loads, 2026-10-11)."""
import json, re, sys, time
from scripting import initScripting
ccxml, core, img, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
reads = [(int(a, 16), int(n)) for a, n in (x.split(",") for x in sys.argv[5:])]
ds = initScripting(); ds.setScriptingTimeout(180000)
ds.configure(ccxml)
s = ds.openSession(re.compile(core, re.IGNORECASE))
s.target.connect()
try:
    s.target.halt()
except Exception as e:
    pass
s.memory.loadProgram(img)
t0 = time.time(); s.target.run(False); time.sleep(secs); s.target.halt()
rec = {"image": img, "ran_s": round(time.time() - t0, 1)}
try:
    rec["pc"] = "%08x" % (s.memory.readRegister("PC") & 0xFFFFFFFF)
except Exception as e:
    rec["pc"] = "unreadable: " + str(e)[:80]
for a, n in reads:
    try:
        rec[hex(a)] = [w & 0xFFFFFFFF for w in s.memory.read(a, n)]
    except Exception as e:
        rec[hex(a)] = "unreadable: " + str(e)[:80]
print(json.dumps(rec))
try:
    s.target.disconnect()
except Exception:
    pass
ds.stop()
