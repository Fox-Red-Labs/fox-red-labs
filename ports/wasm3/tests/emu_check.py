#!/usr/bin/env python3
"""Run a wasm3 port image in a Cortex-M0+ (MSPM0G3519 / G3507) or Cortex-M33 (MSPM33C321A) emulator (Unicorn) and count instructions.

Host check only. Unicorn runs the real firmware instructions. The chip's peripherals are stubs: UART0 transmit data goes to the
output, the clock status and frequency counter answer "PLL good", everything else reads zero. Time does not exist here: the
millisecond counter is advanced by a hook (one tick per 100 translated blocks), so every microsecond figure the image prints
is meaningless. What counts is the instruction tally per phase: Wasm run of fib and bench (inside wasm3) against the same
loops as native C. The ratio of the two tallies is an ESTIMATE of the speed cost; it assumes similar cycles per instruction.

Usage: emu_check.py <build-dir> <g3519|g3507|m33> [--no-count]     build-dir holds wasm3.elf and wasm3.bin
Needs: pip install unicorn; arm-none-eabi-nm and arm-none-eabi-objdump on PATH. Exit 0 when the image prints PASS.
"""
import subprocess, struct, sys, pathlib
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_BLOCK, UC_HOOK_CODE, UcError
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_CPU_ARM_CORTEX_M0, UC_CPU_ARM_CORTEX_M33

build = pathlib.Path(sys.argv[1]); board = sys.argv[2]; count = "--no-count" not in sys.argv
M33 = board == "m33"
flash_len, ram_len = {"g3519": (0x80000, 0x20000), "g3507": (0x20000, 0x8000), "m33": (0x100000, 0x40000)}[board]
FB, RB = (0x10000000, 0x30000000) if M33 else (0, 0x20200000)
image = bytearray((build / "wasm3.bin").read_bytes())
nm = subprocess.run(["arm-none-eabi-nm", str(build / "wasm3.elf")], capture_output=True, text=True, check=True).stdout
sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
SYSCTL = 0x400AF000
CLKSTATUS, FCC, UART_TX = SYSCTL + 0x1204, SYSCTL + 0x1150, 0x40108000 + 0x1120
out = bytearray(); fcc = [0]

def mmio_read(uc, off, size, ud):
    a = ud + off
    if a == CLKSTATUS: return 0xFFFFFFFF
    if a == FCC:
        fcc[0] += 1
        return 1250 if fcc[0] % 2 else 1000
    return 0
def mmio_write(uc, off, size, value, ud):
    if ud + off == UART_TX: out.append(value & 0xFF)

dis = subprocess.run(["arm-none-eabi-objdump", "-d", str(build / "wasm3.elf")], capture_output=True, text=True, check=True).stdout
for line in dis.splitlines():
    p = line.split("\t")
    if len(p) >= 3 and p[2].startswith("wfi"):
        a = int(p[0].strip().rstrip(":"), 16); image[a:a + 2] = b"\x00\xbf"

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M33 if M33 else UC_CPU_ARM_CORTEX_M0)
uc.mem_map(FB, flash_len); uc.mem_write(FB, bytes(image))
uc.mem_map(RB, ram_len)
if M33:
    uc.mem_map(0xE000E000, 0x1000)
    uc.mmio_map(0x40000000, 0x10000000, mmio_read, 0x40000000, mmio_write, 0x40000000)
else:
    uc.mmio_map(0x40000000, 0x800000, mmio_read, 0x40000000, mmio_write, 0x40000000)
    uc.mem_map(0x41C00000, 0x200000)
    uc.mmio_map(0xE0000000, 0x100000, lambda *a: 0, None, lambda *a: None, None)

# Phases: entry of m3_CallV = wasm, entry of frl_native_fib / frl_native_bench = native, entry of hal_us = other.
phase = ["other"]; seq = []; tally = {"other": 0, "wasm": 0, "native": 0}; cur = [None]
def switch(p):
    if p != phase[0]:
        seq.append((phase[0], tally[phase[0]])); tally[phase[0]] = 0; phase[0] = p
entries = {(sym["m3_CallV"] & ~1): "wasm", (sym["frl_native_fib"] & ~1): "native", (sym["frl_native_bench"] & ~1): "native", (sym["hal_us"] & ~1): "other"}
stopped = [False]
def on_code(uc, address, size, ud):
    p = entries.get(address)
    if p: switch(p)
    tally[phase[0]] += 1
def log_text():
    n = struct.unpack("<I", uc.mem_read(sym["frl_log_len"], 4))[0]
    return bytes(uc.mem_read(sym["frl_log"], n))
def on_block(uc, address, size, ud):
    blocks[0] += 1
    if M33 and blocks[0] % 50 == 0:
        t = log_text()
        if t.endswith(b"PASS\n") or (b"FAIL" in t and t.endswith(b"\n")): uc.emu_stop()
    if blocks[0] % 100 == 0:
        t = struct.unpack("<I", uc.mem_read(sym["tick_ms"], 4))[0]
        uc.mem_write(sym["tick_ms"], struct.pack("<I", (t + 1) & 0xFFFFFFFF))
    if out.endswith(b"PASS\n") or b"FAIL" in out and out.endswith(b"\n"): uc.emu_stop()
blocks = [0]
uc.hook_add(UC_HOOK_BLOCK, on_block)
if count: uc.hook_add(UC_HOOK_CODE, on_code)
sp, pc = struct.unpack("<II", bytes(image[:8]))
uc.reg_write(UC_ARM_REG_SP, sp)
try:
    uc.emu_start(pc, 0, count=900_000_000)
except UcError as e:
    print("emulator stopped:", e, "pc=%#x" % uc.reg_read(UC_ARM_REG_PC))
switch("other"); seq.append(("other", tally["other"]))
if M33: out[:] = log_text()
print(out.decode("latin-1"))
ok = out.endswith(b"PASS\n")
if count:
    # wasm segments in order: run, fib, bench. native segments in order: fib, bench.
    w = [n for p, n in seq if p == "wasm" and n > 1000]; nat = [n for p, n in seq if p == "native" and n > 1000]
    print("instructions: wasm run(blink) %s, wasm fib %s, wasm bench %s; native fib %s, native bench %s" % (
        *(w + [0] * 3)[:3], *(nat + [0] * 2)[:2]))
    if len(w) >= 3 and len(nat) >= 2:
        print("ratio wasm/native: fib %.1f, bench %.1f" % (w[1] / nat[0], w[2] / nat[1]))
r = uc.mem_read(sym["frl_result"], 72)
print("frl_result: magic=%#x status=%d fib_val=%d bench_val=%d heap_used=%d" % (struct.unpack_from("<I", r, 0)[0], struct.unpack_from("<i", r, 8)[0], struct.unpack_from("<I", r, 32)[0], struct.unpack_from("<I", r, 48)[0], struct.unpack_from("<I", r, 60)[0]))
print("PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
