#!/usr/bin/env python3
"""Print U-Boot commands that place a 7-instruction AArch64 stub in DDR and run it with `go`,
so U-Boot (non-secure EL2 under TF-A) makes one SMC call: x0=FUNC, x1..x3=ARGS, then returns x0.
U-Boot's `go` prints the return code ("## Application terminated, rc = 0x...").
  psci_stub.py version            -> PSCI_VERSION (0x84000000); expect rc 0x10001 for PSCI 1.1
  psci_stub.py cpu_on MPIDR ENTRY -> CPU_ON 64-bit (0xC4000003), context 0; expect rc 0x0
  psci_stub.py affinity MPIDR     -> AFFINITY_INFO 64-bit (0xC4000004); 0 = on, 1 = off
Options: --at ADDR (default 0x88000000: free DDR while U-Boot idles, clear of the kernel at
0x82000000 and the device tree at 0x84F00000)."""
import sys
AT = 0x88000000
args = sys.argv[1:]
if '--at' in args:
    i = args.index('--at'); AT = int(args[i + 1], 0); del args[i:i + 2]
def movz(rd, imm, hw=0): return 0xD2800000 | (hw << 21) | ((imm & 0xFFFF) << 5) | rd
def movk(rd, imm, hw): return 0xF2800000 | (hw << 21) | ((imm & 0xFFFF) << 5) | rd
def load64(rd, v):
    out = [movz(rd, v & 0xFFFF)]
    for hw in (1, 2, 3):
        part = (v >> (16 * hw)) & 0xFFFF
        if part: out.append(movk(rd, part, hw))
    return out
op = args[0]
if op == 'version': f, a = 0x84000000, [0, 0, 0]
elif op == 'cpu_on': f, a = 0xC4000003, [int(args[1], 0), int(args[2], 0), 0]
elif op == 'affinity': f, a = 0xC4000004, [int(args[1], 0), 0, 0]
else: sys.exit(__doc__)
code = load64(0, f) + load64(1, a[0]) + load64(2, a[1]) + load64(3, a[2]) + [0xD4000003, 0xD65F03C0]  # smc #0; ret
cmds = [f"mw.l 0x{AT + 4 * i:08x} 0x{w:08x}" for i, w in enumerate(code)]
cmds += ["dcache flush", "icache flush", f"go 0x{AT:08x}"]
print("\n".join(cmds))
