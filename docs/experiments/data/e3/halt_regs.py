# Attach to the running CC2340R5 without loading, halt, read core and fault registers, resume. Run with CCStudio's Python.
import sys
from scripting import initScripting
ds = initScripting(); ds.setScriptingTimeout(60000); ds.configure(sys.argv[1])
s = ds.openSession(".*Cortex.*")
s.target.connect()
for n in range(int(sys.argv[2]) if len(sys.argv) > 2 else 2):
    s.target.halt()
    regs = {r: s.registers.read(r) for r in ["PC", "LR", "SP", "XPSR", "R0", "R1", "R2", "R3"]}
    print("sample", n, " ".join("%s=0x%08x" % (k, v) for k, v in regs.items()), flush=True)
    for name, addr in [("ICSR", 0xE000ED04), ("SHCSR", 0xE000ED24), ("VTOR", 0xE000ED08)]:
        v = s.memory.read(addr, 1); print("  %s=%s" % (name, [hex(x) for x in v]), flush=True)
    sp = regs["SP"]
    print("  stack:", [hex(x) for x in s.memory.read(sp, 16)], flush=True)
    s.target.run(False)
    import time; time.sleep(1)
s.target.disconnect(); ds.shutdown()
