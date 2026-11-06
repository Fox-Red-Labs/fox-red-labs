#!/usr/bin/env python3
"""Run the null-display Peanut-GB build (build_target.sh mspm33c321a rom) on a Cortex-M33 model (Unicorn) and count instructions
per frame, split by function group: Game Boy CPU and bus, picture lines (Peanut-GB's own line renderer), scaler, other.
  python3 -I emu_pgb.py <pgb.out> [--frames N] [--skip N] [--nm tiarmnm] [--top 12] [--json]
Counts instructions, not cycles: flash wait states, caches and bus stalls are not modelled. The same ARM code as the board runs (so a
section placed in SRAM must be copied by the boot code, or the run faults). Needs: pip install unicorn."""
import sys, struct, argparse, subprocess, json, bisect
from unicorn import *
from unicorn.arm_const import *

ap = argparse.ArgumentParser()
ap.add_argument('image'); ap.add_argument('--frames', type=int, default=30); ap.add_argument('--skip', type=int, default=0, help='frames to run before counting')
ap.add_argument('--nm', default='/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm')
ap.add_argument('--top', type=int, default=12); ap.add_argument('--json', action='store_true')
ap.add_argument('--block', action='store_true', help='count per translated block (several times faster; Thumb instruction lengths decoded from the bytes, equal to the exact count on straight code)')
a = ap.parse_args()
FLASH, RAM = 0x10000000, 0x30000000
elf = open(a.image, 'rb').read()
e_phoff, = struct.unpack_from('<I', elf, 0x1c); e_phentsize, e_phnum = struct.unpack_from('<HH', elf, 0x2a)
segs = []
for i in range(e_phnum):
    p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', elf, e_phoff + i * e_phentsize)
    if p_type == 1 and p_filesz: segs.append((p_paddr, elf[p_off:p_off + p_filesz]))
syms = {}
for l in subprocess.check_output([a.nm, a.image]).decode().splitlines():
    p = l.split()
    if len(p) == 3 and p[1].lower() in ('t', 'w', 'd', 'b'): syms[p[2]] = int(p[0], 16)
fn = sorted((v & ~1, k) for k, v in syms.items())
fa = [x[0] for x in fn]
def symname(addr):
    i = bisect.bisect_right(fa, addr) - 1
    return fn[i][1] if i >= 0 else '?'

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M33)
uc.mem_map(0xE000E000, 0x1000); uc.mem_map(FLASH, 0x100000); uc.mem_map(RAM, 0x40000)
for addr, data in segs: uc.mem_write(addr, data)
def unmapped(uc, access, addr, size, val, ud):
    uc.mem_map(addr & ~0xfff, 0x1000); return True
uc.hook_add(UC_HOOK_MEM_UNMAPPED, unmapped)
sp0, pc0 = struct.unpack_from('<II', bytes(uc.mem_read(FLASH, 8)))
uc.reg_write(UC_ARM_REG_SP, sp0)

GROUPS = {
  'scaler': ('draw_line_fast', 'emit_row', 'frl_present_shades2', 'pgb_present', 'build_lut', 'draw_line'),
  'lines': ('__gb_draw_line',),
}
def group(name):
    if name in GROUPS['scaler']: return 'scaler'
    if name in GROUPS['lines'] or name.startswith('compare_sprites'): return 'lines'
    if name.startswith('__gb_') or name.startswith('gb_') or name in ('pgb_run_frame', 'main', 'apply_input'): return 'cpu'
    return 'other'
memo = {}
cnt = {}; gcnt = {'scaler': 0, 'lines': 0, 'cpu': 0, 'other': 0}
st = {'n': 0, 'frames': 0, 'counting': a.skip == 0, 'last': 0, 'per': [], 'perg': [], 'lastg': dict(gcnt)}
def on_code(uc, addr, size, ud):
    if not st['counting']: return
    m = memo.get(addr)
    if m is None:
        nm = symname(addr); m = memo[addr] = (nm, group(nm))
    cnt[m[0]] = cnt.get(m[0], 0) + 1
    gcnt[m[1]] += 1
    st['n'] += 1
def present(uc, addr, size, ud):
    st['frames'] += 1
    if st['frames'] == a.skip and a.skip > 0: st['counting'] = True; st['last'] = st['n']; st['lastg'] = dict(gcnt); start_counting()
    elif st['counting'] and st['frames'] > a.skip:
        st['per'].append(st['n'] - st['last']); st['last'] = st['n']
        st['perg'].append({k: gcnt[k] - st['lastg'][k] for k in gcnt}); st['lastg'] = dict(gcnt)
    if st['frames'] >= a.skip + a.frames: uc.emu_stop()
tbmemo = {}
def thumb_count(addr, size):
    key = (addr, size); c = tbmemo.get(key)
    if c is None:
        b = bytes(uc.mem_read(addr, size)); i = 0; c = 0
        while i < len(b):
            hw = b[i] | (b[i + 1] << 8) if i + 1 < len(b) else 0
            i += 4 if (hw >> 11) in (0x1D, 0x1E, 0x1F) else 2; c += 1
        tbmemo[key] = c
    return c
def on_block(uc, addr, size, ud):
    if not st['counting']: return
    addr &= ~1
    m = memo.get(addr)
    if m is None:
        nm = symname(addr); m = memo[addr] = (nm, group(nm))
    n = thumb_count(addr, size)
    cnt[m[0]] = cnt.get(m[0], 0) + n
    gcnt[m[1]] += n
    st['n'] += n
def start_counting():
    # The counting hook goes in only when the window starts, so the frames before it run at full speed.
    if a.block: uc.hook_add(UC_HOOK_BLOCK, on_block)
    else: uc.hook_add(UC_HOOK_CODE, on_code)
    uc.ctl_flush_tb()
uc.hook_add(UC_HOOK_CODE, present, begin=syms['pgb_present'] & ~1, end=(syms['pgb_present'] & ~1) + 1)
if a.skip == 0: start_counting()
try:
    uc.emu_start(pc0 | 1, 0)
except UcError as ex:
    print('EMU fault', ex, 'pc', hex(uc.reg_read(UC_ARM_REG_PC)), symname(uc.reg_read(UC_ARM_REG_PC))); sys.exit(1)
per = st['per']; n = len(per)
res = {'frames': n, 'instr_per_frame_mean': sum(per) / n if n else 0, 'min': min(per) if per else 0, 'max': max(per) if per else 0,
       'groups_per_frame': {k: sum(g[k] for g in st['perg']) / n for k in gcnt} if n else {}}
if a.json: print(json.dumps(res))
else:
    print(f"{a.image}: {n} frames counted after {a.skip} skipped")
    print(f"instructions per frame: mean {res['instr_per_frame_mean']:.0f}, min {res['min']}, max {res['max']}")
    for k, v in res['groups_per_frame'].items(): print(f"  {k:8s} {v:10.0f} per frame ({100 * v / res['instr_per_frame_mean']:.1f} percent)")
    tot = sum(cnt.values())
    for k, v in sorted(cnt.items(), key=lambda x: -x[1])[:a.top]: print(f"  {k:28s} {v / max(n,1):10.0f} per frame")
