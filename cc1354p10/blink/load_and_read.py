"""Load blink.out on the LP-CC1354P10-1, run it, and read blink_count over time (DIO6 is the red LED; the GPIO output register cannot be read while the program runs).
Run: /Applications/ti/ccs2101/ccs/scripting/python/run.sh load_and_read.py <blink.out> <blink_count address, hex>"""
import re, sys, time
from pathlib import Path
from scripting import initScripting
ds = initScripting(); ds.setScriptingTimeout(120000)
ds.configure(str(Path(__file__).resolve().parents[1] / "CC1354P10.ccxml"))
s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
s.target.connect()
s.memory.loadProgram(sys.argv[1])
s.target.run(False)
print(time.strftime("%H:%M:%S"), "loaded and running", flush=True)
addr = int(sys.argv[2], 16)
def rd(a):
    return s.memory.read(a, 1)[0] & 0xFFFFFFFF
for i in range(8):
    time.sleep(0.9 if i < 3 else 0.37)
    try: print(time.strftime("%H:%M:%S.") + ("%03d" % (time.time() % 1 * 1000)), "count", rd(addr), flush=True)
    except Exception as e: print("read failed", e, flush=True)
print("done", flush=True)
s.target.disconnect()
ds.shutdown()
