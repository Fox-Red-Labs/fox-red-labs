"""Load an image into the LP-CC1354P10-1 and leave it running, in one CCStudio scripting session.
A DSLite flash does not start a new program on this board until a power cycle; this does.
Run: /Applications/ti/ccs2101/ccs/scripting/python/run.sh ccs_load_run.py <image .out>"""
import re, sys
from pathlib import Path
from scripting import initScripting
ds = initScripting(); ds.setScriptingTimeout(120000)
ds.configure(str(Path(__file__).resolve().parents[1] / "CC1354P10.ccxml"))
s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
s.target.connect()
s.memory.loadProgram(sys.argv[1])
s.target.run(False)
print("loaded and running", sys.argv[1], flush=True)
s.target.disconnect()
ds.shutdown()
