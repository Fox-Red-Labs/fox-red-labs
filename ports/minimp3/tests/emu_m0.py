#!/usr/bin/env python3
"""Run the minimp3 bench image for the LP-MSPM0G3519 (Cortex-M0+) in the Unicorn emulator and ESTIMATE its cycles.

Host check only. Unicorn runs the real firmware instructions, including the soft-float library. Time does not exist in the
emulator, so the image reads a pretend cycle counter at 0x40000F00 (built with -DMP3_EMU): this script answers with a MODELLED
cycle count, so the firmware's own per-frame timing (decode call only, hashing excluded) comes out in modelled cycles.
Model (Arm Cortex-M0+ timing, from the instruction mix in the objdump listing; no flash wait states, no bus stalls):
  most instructions 1 cycle; ldr/str 2; ldm/stm/push/pop 1+registers; taken branch 2; bl 3; bx/blx and pop {pc} 3 (pop adds registers);
  multiply 1 cycle (--mul 32 prices it as 32, the slow-multiplier option of the core).
Usage: emu_m0.py <mp3.elf> <mp3.bin> [--mul N]     prints the result block, per-frame cycles and hashes.
Needs: pip install unicorn; arm-none-eabi-objdump and -nm on PATH."""
import re, subprocess, struct, sys, pathlib, json
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_BLOCK, UcError
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC, UC_CPU_ARM_CORTEX_M0

elf = sys.argv[1]; binf = sys.argv[2]
MUL = int(sys.argv[sys.argv.index('--mul') + 1]) if '--mul' in sys.argv else 1
image = bytearray(pathlib.Path(binf).read_bytes())
nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
sym = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
dis = subprocess.run(["arm-none-eabi-objdump", "-d", elf], capture_output=True, text=True, check=True).stdout
# per instruction: address -> (size, base_cost, kind, taken_cost)   kind: 'n' plain, 'b' branch (always), 'c' conditional, 'x' computed/return
ins = {}
for line in dis.splitlines():
    m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} ?)+)\s+(\S+)\s*(.*)$', line)
    if not m: continue
    addr = int(m.group(1), 16); hexw = m.group(2).split(); size = 2 * len(hexw); mn = m.group(3); ops = m.group(4)
    if mn.startswith('.'): continue
    mn = mn.split('.')[0]
    nreg = lambda: len([x for x in re.search(r'\{(.*)\}', ops).group(1).split(',') if x.strip()]) if '{' in ops else 1
    if mn in ('push',): c = (1 + nreg(), 'n', 0)
    elif mn == 'pop': c = (1 + nreg(), 'x', 3) if 'pc' in ops else (1 + nreg(), 'n', 0)
    elif mn in ('ldm', 'ldmia', 'stm', 'stmia'): c = (1 + nreg(), 'n', 0)
    elif mn.startswith('ldr') or mn.startswith('str'): c = (2, 'n', 0)
    elif mn == 'bl': c = (3, 'b', 3)
    elif mn in ('bx', 'blx'): c = (3, 'x', 3)
    elif mn == 'b': c = (2, 'b', 2)
    elif re.fullmatch(r'b(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)', mn): c = (1, 'c', 2)
    elif mn in ('cbz', 'cbnz'): c = (1, 'c', 2)
    elif mn in ('mul', 'muls'): c = (MUL, 'n', 0)
    elif mn in ('wfi', 'wfe'): c = (1, 'n', 0)
    else: c = (1, 'n', 0)
    ins[addr] = (size, c[0], c[1], c[2], mn in ('mul', 'muls'))

# a pc that left the listing (data in code) is cost 1
FB, RB = 0, 0x20200000
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS); uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M0)
uc.mem_map(FB, 0x80000); uc.mem_write(FB, bytes(image)); uc.mem_map(RB, 0x20000)
cyc = [0]; nins = [0]; nmul = [0]
def mmio_read(uc, off, size, ud):
    if ud + off == 0x40000F00: return cyc[0] & 0xFFFFFFFF
    if ud + off == 0x40000F04: return nins[0] & 0xFFFFFFFF
    return 0
uc.mmio_map(0x40000000, 0x800000, mmio_read, 0x40000000, lambda *a: None, 0x40000000)
uc.mem_map(0x41C00000, 0x200000)
uc.mmio_map(0xE0000000, 0x100000, lambda *a: 0, None, lambda *a: None, None)
done_addr = sym["mp_done"]; blocks = [0]; nxt_expect = [None]; last_taken_cost = [0]
def on_block(uc, address, size, ud):
    # settle the previous block's final branch: taken if we landed somewhere other than its fall-through
    if nxt_expect[0] is not None:
        fall, kind, tc, base = nxt_expect[0]
        if kind == 'n': pass
        elif address != fall: cyc[0] += tc - base
    a = address; end = address + size; c = 0; n = 0; last = None
    while a < end:
        t = ins.get(a)
        if t is None: c += 1; n += 1; a += 2; continue
        sz, cost, kind, tc, ismul = t
        c += cost; n += 1; nmul[0] += ismul; last = (a + sz, kind, tc, cost); a += sz
    cyc[0] += c; nins[0] += n
    nxt_expect[0] = last if last and last[1] != 'n' else None
    blocks[0] += 1
    if blocks[0] % 20000 == 0 and struct.unpack("<I", uc.mem_read(done_addr, 4))[0] == 0x4D503344: uc.emu_stop()
uc.hook_add(UC_HOOK_BLOCK, on_block)
sp, pc = struct.unpack("<II", bytes(image[:8])); uc.reg_write(UC_ARM_REG_SP, sp)
try: uc.emu_start(pc, 0, count=3_000_000_000)
except UcError as e: print("emulator stopped:", e, "pc=%#x" % uc.reg_read(UC_ARM_REG_PC))
res = bytes(uc.mem_read(sym["mp_res"], 96)); det = bytes(uc.mem_read(sym["mp_det"], 11264 * 2))
DET = 11264; out = {}
for i, name in enumerate(("stereo128", "mono64")):
    magic, frames, ch, hz, kbps, spc, h, clo, chi, cmin, cmax, used = struct.unpack('<12I', res[i * 48:(i + 1) * 48])
    d = det[i * DET:(i + 1) * DET]
    hashes = struct.unpack('<256I', d[:1024])[:frames]; cy = struct.unpack('<256I', d[1024:2048])[:frames]
    out[name] = dict(magic=hex(magic), frames=frames, ch=ch, hz=hz, hashes=['0x%08x' % x for x in hashes], cycles=list(cy))
    print(name, 'frames', frames, 'cycles per frame (modelled):', cy)
print("modelled total cycles %d, instructions %d, multiplies %d, mul cost %d" % (cyc[0], nins[0], nmul[0], MUL))
print("JSON", json.dumps(dict(out=out, total_cycles=cyc[0], instructions=nins[0], multiplies=nmul[0], mul=MUL)))
