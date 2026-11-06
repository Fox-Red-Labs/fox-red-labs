#!/usr/bin/env python3
"""Run the LC3 bench image built for a Cortex-M0+ (target-g3519/build.sh) in the Unicorn emulator and ESTIMATE its cycles.
A MODEL, not an M0+ run: the same method as ports/minimp3/tests/emu_m0.py. Unicorn executes the real firmware instructions, including
the soft-float library; the image reads a pretend cycle counter at 0x40000F00 and this script answers with a modelled count, so the
firmware's own per-frame timing (encode and decode calls only) comes out in modelled cycles.
Model (Arm Cortex-M0+ timing from the instruction mix in the objdump listing; no flash wait states, no bus stalls, so a floor):
  most instructions 1 cycle; ldr/str 2; ldm/stm/push/pop 1+registers; taken branch 2; bl 3; bx/blx and pop {pc} 3 (pop adds registers);
  multiply 1 cycle (--mul 32 prices it as 32, the slow-multiplier option of the core).
Usage: emu_m0.py <lc3.elf> <lc3.bin> [--mul N]     prints per-configuration mean cycles per 10 ms frame, CRCs, and a JSON line.
Needs: unicorn (a Python with it); arm-none-eabi-objdump and -nm on PATH."""
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
done_addr = sym["lc3_done"]; blocks = [0]; nxt_expect = [None]; last_taken_cost = [0]
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
    if blocks[0] % 20000 == 0 and struct.unpack("<I", uc.mem_read(done_addr, 4))[0] == 0x4C433344: uc.emu_stop()
uc.hook_add(UC_HOOK_BLOCK, on_block)
sp, pc = struct.unpack("<II", bytes(image[:8])); uc.reg_write(UC_ARM_REG_SP, sp)
try: uc.emu_start(pc, 0, count=3_000_000_000)
except UcError as e: print("emulator stopped:", e, "pc=%#x" % uc.reg_read(UC_ARM_REG_PC))
NF = 10
res = bytes(uc.mem_read(sym["lc3_res"], 72 * 3)); det = bytes(uc.mem_read(sym["lc3_det"], NF * 16 * 3))
out = []
for i in range(3):
    f = struct.unpack('<18I', res[i * 72:(i + 1) * 72])
    magic, hz, kbps, nb, frames, esz, dsz, bcrc, pcrc, elo, ehi, dlo, dhi, emin, emax, dmin, dmax, errs = f
    d = det[i * NF * 16:(i + 1) * NF * 16]
    ecyc = struct.unpack('<%dI' % NF, d[NF * 8:NF * 12]); dcyc = struct.unpack('<%dI' % NF, d[NF * 12:NF * 16])
    et = ehi << 32 | elo; dt = dhi << 32 | dlo
    out.append(dict(hz=hz, kbps=kbps, frames=frames, errors=errs, magic=hex(magic), bits_crc='0x%08x' % bcrc, pcm_crc='0x%08x' % pcrc,
                    enc_mean=et / max(frames, 1), dec_mean=dt / max(frames, 1), enc_cycles=list(ecyc), dec_cycles=list(dcyc)))
    print('%d Hz %d kbps: %d frames, errors %d, bits_crc 0x%08x pcm_crc 0x%08x, modelled cycles per frame: encode %.0f, decode %.0f' % (hz, kbps, frames, errs, bcrc, pcrc, et / max(frames, 1), dt / max(frames, 1)))
print("modelled total cycles %d, instructions %d, multiplies %d, mul cost %d" % (cyc[0], nins[0], nmul[0], MUL))
print("JSON", json.dumps(dict(cfg=out, total_cycles=cyc[0], instructions=nins[0], multiplies=nmul[0], mul=MUL)))
