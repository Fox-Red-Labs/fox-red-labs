#!/usr/bin/env python3
"""Run the emulator build of the engine (target/build.sh with EMU=1) on a Cortex-M33 model (Unicorn) and report
what it did: console text, frame checksums, instruction count, stack depth, and where it stopped.
  python3 -I emu_m33.py <doom.out> <DOOM1.WAD> [--nm tiarmnm] [--max-instr N] [--count] [--frames-dir DIR --every N]
Needs: pip install unicorn. No board and no TI tool at run time except `tiarmnm` to find symbols (or pass --syms file made with it).
The emulator checks the program (same ARM code as the board, same struct sizes, same zone), not the timing: instructions are counted,
cycles are not simulated (no flash wait states, no QSPI)."""
import sys, struct, zlib, subprocess, argparse
from unicorn import *
from unicorn.arm_const import *

ap = argparse.ArgumentParser()
ap.add_argument('image'); ap.add_argument('wad')
ap.add_argument('--nm', default='tiarmnm'); ap.add_argument('--max-instr', type=int, default=0)
ap.add_argument('--count', action='store_true', help='count every instruction (slow)')
ap.add_argument('--frames-dir'); ap.add_argument('--every', type=int, default=35)
ap.add_argument('--ram-kb', type=int, default=4096)
ap.add_argument('--max-frames', type=int, default=0)
ap.add_argument('--demo', type=int, default=1)
ap.add_argument('--ms-per-read', type=int, default=1, help='fake clock step per read of the timer register; 30 or more makes the game run one tic per frame')
ap.add_argument('--calls', default='', help='comma list of function names: count how often each is entered')
ap.add_argument('--stop-demos', type=int, default=0, help='stop after this many demo ends (G_CheckDemoStatus calls)')
ap.add_argument('--wad-base', type=lambda x:int(x,0), default=0x60000000)
ap.add_argument('--stack-probe', action='store_true')
ap.add_argument('--windows', help='count exact instructions only inside frame windows, e.g. 0-30,1000-1030,3000-3030')
a = ap.parse_args()

FLASH, RAM, MMIO, WADB = 0x10000000, 0x30000000, 0x50000000, a.wad_base
elf = open(a.image, 'rb').read()
assert elf[:4] == b'\x7fELF'
e_phoff, = struct.unpack_from('<I', elf, 0x1c); e_phentsize, e_phnum = struct.unpack_from('<HH', elf, 0x2a)
segs = []
for i in range(e_phnum):
    p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', elf, e_phoff + i * e_phentsize)
    if p_type == 1 and p_filesz:
        segs.append((p_paddr, elf[p_off:p_off + p_filesz]))

syms = {}
for l in subprocess.check_output([a.nm, a.image]).decode().splitlines():
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
symlist = sorted((v & ~1, k) for k, v in syms.items())
def symname(addr):
    best = None
    for v, k in symlist:
        if v <= addr: best = k
        else: break
    return best

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M33)
uc.mem_map(0xE000E000, 0x1000); uc.mem_map(FLASH, 0x100000); uc.mem_map(RAM, a.ram_kb * 1024); wad = open(a.wad, 'rb').read(); uc.mem_map(WADB, (len(wad) + 0xfff) & ~0xfff); uc.mem_write(WADB, wad)
for addr, data in segs: uc.mem_write(addr, data)

state = {'icount': 0, 'frames': 0, 'crc': 0, 'console': bytearray(), 'minsp': 0xffffffff, 'stop': None}
sp0, pc0 = struct.unpack_from('<II', bytes(uc.mem_read(FLASH, 8)))
uc.reg_write(UC_ARM_REG_SP, sp0)

def mmio_read(uc, off, size, ud):
    if off == 8: return a.demo
    # Fake clock: with --count, counted instructions at 160 MHz and one cycle each; otherwise one millisecond per read.
    if a.count:
        return (state['icount'] // 160000) & 0xffffffff
    state['clock'] = state.get('clock', 0) + a.ms_per_read
    return state['clock'] & 0xffffffff
def mmio_write(uc, off, size, val, ud):
    if off == 4: state['console'].append(val & 0xff)
uc.mmio_map(MMIO, 0x1000, mmio_read, None, mmio_write, None)

def on_code(uc, addr, size, ud):
    state['icount'] += 1
def on_block(uc, addr, size, ud):
    state['icount'] += size // 3   # about 3 bytes per Thumb-2 instruction on average; use --count for exact
def stop_hook(uc, addr, size, ud):
    state['stop'] = ud; uc.emu_stop()

def after_frame(uc):
    state['frames'] += 1
    if a.max_frames and state['frames'] >= a.max_frames:
        state['stop'] = 'max-frames'; uc.emu_stop()
    if windows_list:
        f = state['frames']
        inside = any(lo <= f < hi for lo, hi in windows_list)
        if inside and not state.get('hook'):
            state['hook'] = uc.hook_add(UC_HOOK_CODE, on_code); uc.ctl_flush_tb(); state['wstart'] = (f, state['icount'], dict(calls))
        if not inside and state.get('hook'):
            uc.hook_del(state['hook']); state['hook'] = None; uc.ctl_flush_tb()
            f0, i0, c0 = state['wstart']; state.setdefault('wres', []).append((f0, f - f0, state['icount'] - i0, {k: calls.get(k, 0) - c0.get(k, 0) for k in calls}))


import os
def frame_hook(uc, addr, size, ud):
    idx = int(uc.reg_read(UC_ARM_REG_R0)); pal = int(uc.reg_read(UC_ARM_REG_R1))
    buf = bytes(uc.mem_read(idx, 64000)); pb = bytes(uc.mem_read(pal, 768))
    state['crc'] = zlib.crc32(buf, state['crc']); state['crc'] = zlib.crc32(pb, state['crc'])
    if a.frames_dir and state['frames'] % a.every == 0:
        rgb = b''.join(pb[c * 3:c * 3 + 3] for c in buf)
        open(f"{a.frames_dir}/f{state['frames']:05d}-320x200.ppm", 'wb').write(b'P6\n320 200\n255\n' + rgb)
    after_frame(uc)

windows_list = [tuple(map(int, w.split('-'))) for w in a.windows.split(',')] if a.windows else []
if a.count: uc.hook_add(UC_HOOK_CODE, on_code)
def frame_hook2(uc, addr, size, ud):
    fb = int(uc.reg_read(UC_ARM_REG_R0))
    if not (RAM <= fb and fb + 53760 <= RAM + a.ram_kb * 1024):
        print('EMU bad frame pointer', hex(fb), 'type', int(uc.reg_read(UC_ARM_REG_R1))); state['stop'] = 'bad frame'; uc.emu_stop(); return
    buf = bytes(uc.mem_read(fb, 320 * 168))
    state['crc'] = zlib.crc32(buf, state['crc'])
    if a.frames_dir and state['frames'] % a.every == 0:
        open(f"{a.frames_dir}/v{state['frames']:05d}.pgm", 'wb').write(b'P5\n320 168\n255\n' + buf)
    after_frame(uc)
if 'fox_frame_done' in syms: uc.hook_add(UC_HOOK_CODE, frame_hook2, begin=syms['fox_frame_done'] & ~1, end=(syms['fox_frame_done'] & ~1) + 1)
state['demos'] = []
calls = {}
def call_hook(uc, addr, size, ud):
    calls[ud] = calls.get(ud, 0) + 1
for n in [x for x in a.calls.split(',') if x]:
    if n in syms: uc.hook_add(UC_HOOK_CODE, call_hook, n, begin=syms[n] & ~1, end=(syms[n] & ~1) + 1)
def demo_hook(uc, addr, size, ud):
    gt = struct.unpack('<i', bytes(uc.mem_read(syms['gametic'], 4)))[0] if 'gametic' in syms else -1
    hw = struct.unpack('<I', bytes(uc.mem_read(syms['fox_zone_hw'], 4)))[0] if 'fox_zone_hw' in syms else -1
    state['demos'].append((gt, state['frames'], hw))
    if a.stop_demos and len(state['demos']) >= a.stop_demos:
        state['stop'] = 'demos-done'; uc.emu_stop()
if 'G_CheckDemoStatus' in syms: uc.hook_add(UC_HOOK_CODE, demo_hook, begin=syms['G_CheckDemoStatus'] & ~1, end=(syms['G_CheckDemoStatus'] & ~1) + 1)
if 'DG_DrawFrame8' in syms: uc.hook_add(UC_HOOK_CODE, frame_hook, begin=syms['DG_DrawFrame8'] & ~1, end=(syms['DG_DrawFrame8'] & ~1) + 1)
for s in ('_exit', 'abort', '__aeabi_assert', 'fox_exit'):
    if s in syms: uc.hook_add(UC_HOOK_CODE, stop_hook, s, begin=syms[s] & ~1, end=(syms[s] & ~1) + 1)

def sp_probe(uc, addr, size, ud):
    sp = uc.reg_read(UC_ARM_REG_SP)
    if sp < state['minsp']: state['minsp'] = sp
if a.stack_probe: uc.hook_add(UC_HOOK_BLOCK, sp_probe)

pc = pc0
try:
    uc.emu_start(pc | 1, 0, count=a.max_instr if a.max_instr else 0)
except UcError as ex:
    state['stop'] = f"UcError {ex} at pc={uc.reg_read(UC_ARM_REG_PC):#x} lr={uc.reg_read(UC_ARM_REG_LR):#x} sp={uc.reg_read(UC_ARM_REG_SP):#x}"
print(state['console'].decode(errors='replace'))
print('EMU calls:', calls)
if 'fox_zone_hw' in syms:
    print('EMU zone_in_use_high_water_bytes', struct.unpack('<I', bytes(uc.mem_read(syms['fox_zone_hw'], 4)))[0])
print('EMU demos (gametic at end, frames so far, zone high water so far):', state['demos'])
print('EMU pc in', symname(int(uc.reg_read(UC_ARM_REG_PC))), hex(int(uc.reg_read(UC_ARM_REG_PC))), 'lr', hex(int(uc.reg_read(UC_ARM_REG_LR))), symname(int(uc.reg_read(UC_ARM_REG_LR)) & ~1))
if 'fox_stuck_sem' in syms:
    v = struct.unpack('<I', bytes(uc.mem_read(syms['fox_stuck_sem'], 4)))[0]
    if v: print('EMU stuck semaphore:', symname(v), hex(v))
for f0, n, ins, dc in state.get('wres', []): print(f"EMU window frames {f0}..{f0+n}: {ins} instructions, {ins/n:.0f} per frame, calls in window {dc}")
print(f"EMU stop={state['stop']} instr={state['icount']} frames={state['frames']} framecrc={state['crc']:08x}")
if a.stack_probe: print(f"EMU stack_low={state['minsp']:#x} stack_used={sp0 - state['minsp']}")
