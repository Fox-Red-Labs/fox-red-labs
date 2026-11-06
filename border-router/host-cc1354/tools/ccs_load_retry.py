"""Load an image into the LP-CC1354P10-1 as soon as the debugger can connect, then run it. No backup first: the
connect window after a power-on boot is only a few seconds long on this board, so the load must start inside it.
Run with CCStudio's Python:  run.sh ccs_load_retry.py <ccxml> <image.out> [tries]"""
import re
import sys
from scripting import initScripting

ccxml, image = sys.argv[1], sys.argv[2]
tries = int(sys.argv[3]) if len(sys.argv) > 3 else 200
ds = initScripting()
ds.setScriptingTimeout(300000)
ds.configure(ccxml)
s = None
for attempt in range(tries):
    try:
        if s is None:
            s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))  # fails with -260 while the probe is unplugged
        s.target.connect()
        break
    except Exception as e:
        print("connect failed (%d/%d): %s" % (attempt + 1, tries, str(e)[-60:]), flush=True)
        if "-260" in str(e):
            s = None
        if attempt + 1 == tries:
            raise
# Reset and halt before loading: whatever firmware is in flash never runs, so no watchdog it might arm can reset
# the chip under the flash loader (that is what broke the first loads on 2026-10-07).
try:
    s.target.reset()
    print("reset and halted", flush=True)
except Exception as e:
    print("reset failed, loading anyway:", str(e)[-60:], flush=True)
s.memory.loadProgram(image)
print("loaded", image, flush=True)
s.target.run(False)
print("running", flush=True)
s.target.disconnect()
ds.shutdown()
