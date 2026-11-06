"""Read the CC2340R5's GPIO registers through the XDS110 probe while the Nordic image runs, and stand in for
the button: drive BTN-1's pin (DIO10) low from the debugger, then let it go. Run under CCStudio's Python:

  /Applications/ti/ccs2101/ccs/scripting/python/run.sh host/gpio_probe.py <ccxml> read|press|release [--halt]

Registers (SDK cc23x0r5/inc/hw_gpio.h, GPIO_BASE 0x40023000): DOUT31_0 +0x200, DOUTSET +0x210, DOUTCLR +0x220,
DOE31_0 +0x500, DOESET +0x510, DOECLR +0x520, DIN31_0 +0x700. Red LED DIO14, BTN-1 DIO10 (active low, pull-up).
"press" clears DOUT bit 10 and sets DOE bit 10: the pin is driven low, as if the button closed it to ground.
"release" clears DOE bit 10: the pull-up takes the pin high again. Nothing else is written.
Memory is accessed with the core running unless --halt is given (then halt, access, run)."""
import re, struct, sys, time, tempfile
from pathlib import Path
from scripting import initScripting

G = 0x40023000
DOUT, DOUTCLR, DOE, DOESET, DOECLR, DIN = G + 0x200, G + 0x220, G + 0x500, G + 0x510, G + 0x520, G + 0x700
LED, BTN = 14, 10

ccxml, cmd = sys.argv[1], sys.argv[2]
halt = "--halt" in sys.argv
ds = initScripting(); ds.setScriptingTimeout(30000)
ds.configure(ccxml)
s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
s.target.connect()
tmp = Path(tempfile.gettempdir()) / "frl_gpio_probe.bin"

def rd(a):
    return s.memory.readOne(a) & 0xFFFFFFFF

def wr(a, v):
    tmp.write_bytes(struct.pack("<I", v))
    try:
        s.memory.loadBinary(a, str(tmp))
    except Exception as e:   # write-1-to-set/clear registers fail the read-back verify; the write lands
        if "verification" not in str(e):
            raise

t0 = time.perf_counter()
if halt:
    s.target.halt()
try:
    if cmd == "press":
        wr(DOUTCLR, 1 << BTN); wr(DOESET, 1 << BTN)
    elif cmd == "release":
        wr(DOECLR, 1 << BTN)
    dout, doe, din = rd(DOUT), rd(DOE), rd(DIN)
finally:
    if halt:
        s.target.run(False)
dt = (time.perf_counter() - t0) * 1000
print(f"{cmd}: DOUT {dout:08x} DOE {doe:08x} DIN {din:08x} | LED DIO14 out {dout >> LED & 1} oe {doe >> LED & 1} pad {din >> LED & 1}"
      f" | BTN DIO10 oe {doe >> BTN & 1} pad {din >> BTN & 1} | access {dt:.0f} ms {'halted' if halt else 'running'}", flush=True)
s.target.disconnect()
