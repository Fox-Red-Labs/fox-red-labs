#!/usr/bin/env python3
"""Print the EEMBC CoreMark reporting line for one LP-MSPM0L1306 run folder.

Reads coremark-output.txt (the board's CoreMark log) and ram.txt (cm_result, then SYSCTL MCLKCFG and
CPUSS CTL, as run-gcc.sh reads them in one OpenOCD session). The board prints no float, so the score is
iterations x clock / total ticks, worked out here from the board's own numbers.
Format: "CoreMark 1.0 : N / <compiler> <flags> / <memory>" (EEMBC CoreMark README, "Run and reporting rules"),
followed by the per-MHz figure with the flash wait states and cache settings the per-MHz rule asks for.
"""
import re, sys, pathlib
d = pathlib.Path(sys.argv[1])
log = (d / "coremark-output.txt").read_text()
words = [l.split(":")[1].split() for l in (d / "ram.txt").read_text().splitlines() if l.startswith("0x")]
res, mclk, cpuss = words[0], int(words[1][0], 16), int(words[2][0], 16)
clk = int(res[1], 16); ticks = int(res[3], 16) + (int(res[4], 16) << 32)
it = int(re.search(r"Iterations\s*:\s*(\d+)", log).group(1))
ver = re.search(r"Compiler version\s*:\s*(.*)", log).group(1).strip()
flags = re.search(r"Compiler flags\s*:\s*(.*)", log).group(1).strip()
mem = re.search(r"Memory location\s*:\s*(.*)", log).group(1).strip()
ok = "Correct operation validated" in log
kind = re.search(r"2K (\w+) run", log).group(1) if re.search(r"2K (\w+) run", log) else "?"
score = it * clk / ticks
clkstatus = int(res[2], 16)
# This chip has no HSCLK: MCLK runs from SYSOSC (CLKSTATUS CURMCLKSEL, bit 17, is 0). The MCLKCFG FLASHWAIT field
# then has no effect (it reads its reset value, 2): "Flash wait states are managed automatically by SYSCTL when
# MCLK is running from SYSOSC or LFCLK" (driverlib dl_sysctl_mspm0l11xx_l13xx.h). The data sheet's figure applies:
# 0 wait states up to 24 MHz, 1 up to 32 MHz.
fw = (mclk >> 8) & 0xF
if (clkstatus >> 17) & 1 == 0:
    ws = f"{0 if clk <= 24_000_000 else 1} flash wait state(s), set by hardware for MCLK from SYSOSC (data sheet; the MCLKCFG FLASHWAIT field reads {fw} and applies only to HSCLK)"
else:
    ws = f"{fw} flash wait state(s) (MCLKCFG FLASHWAIT)"
print(f"{kind} run, {'Correct operation validated' if ok else 'NOT VALIDATED'}; {it} iterations, {ticks} ticks, {ticks/clk:.2f} s")
print(f"CoreMark 1.0 : {score:.2f} / {ver} {flags} / {mem}")
print(f"CoreMark/MHz : {score/(clk/1e6):.3f} at {clk/1e6:g} MHz; code in flash, {ws}; "
      f"memory to core clock 1:1; CPUSS CTL 0x{cpuss:08x} (prefetch {cpuss & 1}, cache {(cpuss >> 1) & 1}, literal cache {(cpuss >> 2) & 1})")
