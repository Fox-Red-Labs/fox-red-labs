#!/usr/bin/env python3
"""Run the emulator build of the engine on a Cortex-M33 model (Unicorn). A copy of ../host/emu_m33.py with the additions for the
software page cache (README section 6):
  --qspi                the WHX is NOT mapped; any direct read of its address range stops the run and names the function; the firmware
                        reads it through a QSPI device model at 0x50001000 (address, length, destination, go) that charges CPU cycles
                        (--qspi-mhz, --qspi-bits, --qspi-fixed-clk, --qspi-sw-cycles, --cpu-mhz)
  --frame-crcs FILE     one CRC-32 per displayed frame; --dump-frames a,b,c with --frames-dir writes those frames as PGM
  --watch FRAME:OFFSET  log every write to one byte of the frame buffers during one frame
  --mmio-whx / --trace  serve the WHX through a callback region and write the 128-byte units read (input of tools/cachesim.c)
  --ram-fill BYTE       pre-fill RAM
Counters the cached image keeps (whx_st_*) and the cache hit rate per demo are printed at the end.
  python3 -I emu_qspi.py <doom.out> <DOOM1.whx> --nm <tiarmnm> --ram-kb 256 --ms-per-read 35 --qspi --stop-demos 3
Needs: pip install unicorn. The emulator checks the program and counts instructions (--count or --windows); it does not simulate cycles."""
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
ap.add_argument('--qspi', action='store_true', help='cached mode: the WHX is NOT mapped; any direct read of its address range stops the run and names the code; the firmware reads it through a QSPI device model')
ap.add_argument('--qspi-mhz', type=float, default=40.0, help='QSPI clock (SDK example: 160 MHz CPUCLK / 4 = 40 MHz)')
ap.add_argument('--qspi-bits', type=int, default=4, help='data lines: 4 = quad I/O read in QPI mode (2 clocks per byte), 1 = plain fast read (8 clocks per byte)')
ap.add_argument('--qspi-fixed-clk', type=int, default=None, help='clocks before the first data byte (default 14 for quad: 2 command + 6 address + 6 dummy; 40 for plain: 8 + 24 + 8)')
ap.add_argument('--qspi-sw-cycles', type=int, default=400, help='CPU cycles of driver software per read (command set-up, DMA start, completion); an assumption until measured on the board')
ap.add_argument('--cpu-mhz', type=float, default=160.0)
ap.add_argument('--frame-crcs', help='write one CRC32 per displayed frame to this file (one hex value a line)')
ap.add_argument('--dump-frames', help='comma list of frame numbers to write as PGM into --frames-dir')
ap.add_argument('--watch', help='frame:offset, log every write to that byte of either frame buffer during that frame')
ap.add_argument('--ram-fill', type=lambda x: int(x, 0), default=None, help='fill all RAM with this byte before the image is loaded (shows reads of never-written stack)')
ap.add_argument('--noop-hook', action='store_true')
ap.add_argument('--mmio-whx', action='store_true', help='serve the WHX through an MMIO region (every read is a callback)')
ap.add_argument('--trace', help='write every change of 128-byte unit read in the mapped WHX to this file (uint32 units, 0xFFFFFFFF + frame number at each frame end)')
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
uc.mem_map(0xE000E000, 0x1000); uc.mem_map(FLASH, 0x100000); uc.mem_map(RAM, a.ram_kb * 1024); wad = open(a.wad, 'rb').read(); (uc.mem_map(WADB, (len(wad) + 0xfff) & ~0xfff), uc.mem_write(WADB, wad)) if not (a.mmio_whx or a.qspi) else None
if a.ram_fill is not None: uc.mem_write(RAM, bytes([a.ram_fill]) * (a.ram_kb * 1024))
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
    if a.qspi:
        q['per_frame'].append(q['cycles'] - q['frame_mark']); q['frame_mark'] = q['cycles']
    if a.trace:
        tr.append(0xFFFFFFFF); tr.append(state['frames'])
    if a.max_frames and state['frames'] >= a.max_frames:
        state['stop'] = 'max-frames'; uc.emu_stop()
    if windows_list:
        f = state['frames']
        inside = any(lo <= f < hi for lo, hi in windows_list)
        if inside and not state.get('hook'):
            state['hook'] = uc.hook_add(UC_HOOK_CODE, on_code); uc.ctl_flush_tb(); state['wstart'] = (f, state['icount'], dict(calls), q['cycles'], q['reads'])
        if not inside and state.get('hook'):
            uc.hook_del(state['hook']); state['hook'] = None; uc.ctl_flush_tb()
            f0, i0, c0, qc0, qr0 = state['wstart']; state.setdefault('wres', []).append((f0, f - f0, state['icount'] - i0, {k: calls.get(k, 0) - c0.get(k, 0) for k in calls}, q['cycles'] - qc0, q['reads'] - qr0))


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
    if a.frame_crcs: fcrc.append('%08x' % zlib.crc32(buf))
    if a.frames_dir and (state['frames'] % a.every == 0 or (a.dump_frames and str(state['frames']) in a.dump_frames.split(','))):
        open(f"{a.frames_dir}/v{state['frames']:05d}.pgm", 'wb').write(b'P5\n320 168\n255\n' + buf)
    after_frame(uc)
if 'fox_frame_done' in syms: uc.hook_add(UC_HOOK_CODE, frame_hook2, begin=syms['fox_frame_done'] & ~1, end=(syms['fox_frame_done'] & ~1) + 1)
fcrc = []
if a.watch and 'frame_buffer' in syms:
    wf, wo = [int(x, 0) for x in a.watch.split(':')]
    def wr(uc, access, addr, size, val, ud):
        if state['frames'] == wf:
            print('EMU WATCH frame', wf, 'addr', hex(addr), 'val', val & 0xff, 'size', size, 'pc', symname(int(uc.reg_read(UC_ARM_REG_PC))))
    for k in (0, 1):
        ad = syms['frame_buffer'] + k * 53760 + wo
        uc.hook_add(UC_HOOK_MEM_WRITE, wr, begin=ad - 3, end=ad + 3)
state['demos'] = []
def unm(uc, access, addr, size, val, ud):
    print('EMU unmapped access', access, hex(addr), size, 'pc', hex(uc.reg_read(UC_ARM_REG_PC)), 'r0-r3', [hex(uc.reg_read(r)) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)])
    return False
uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, unm)
QSPI_BASE = 0x50001000
q = {'reads': 0, 'bytes': 0, 'cycles': 0.0, 'addr': 0, 'len': 0, 'dst': 0, 'frame_mark': 0.0, 'per_frame': []}
if a.qspi:
    fixed = a.qspi_fixed_clk if a.qspi_fixed_clk is not None else (14 if a.qspi_bits == 4 else 40)
    ratio = a.cpu_mhz / a.qspi_mhz
    def qspi_cycles(n): return a.qspi_sw_cycles + (fixed + n * 8 / a.qspi_bits) * ratio
    def trap(uc, off, size, ud):
        pc = int(uc.reg_read(UC_ARM_REG_PC)); lr = int(uc.reg_read(UC_ARM_REG_LR))
        state['stop'] = 'DIRECT-WHX-READ off=%#x size=%d pc=%#x in %s lr=%s' % (off, size, pc, symname(pc), symname(lr & ~1)); uc.emu_stop(); return 0
    uc.mmio_map(WADB, (len(wad) + 0xfff) & ~0xfff, trap, None, None, None)
    def qrd(uc, off, size, ud):
        if off == 0x10: return int(q['cycles']) & 0xffffffff
        if off == 0x14: return q['reads'] & 0xffffffff
        return 0
    def qwr(uc, off, size, val, ud):
        if off == 0: q['addr'] = val
        elif off == 4: q['len'] = val
        elif off == 8: q['dst'] = val
        elif off == 0xC:
            if q['addr'] + q['len'] > len(wad): print('EMU qspi read past the end', hex(q['addr']), q['len']); state['stop'] = 'qspi-past-end'; uc.emu_stop(); return
            uc.mem_write(q['dst'], wad[q['addr']:q['addr'] + q['len']])
            q['reads'] += 1; q['bytes'] += q['len']; q['cycles'] += qspi_cycles(q['len'])
    uc.mmio_map(QSPI_BASE, 0x1000, qrd, None, qwr, None)
if a.mmio_whx:
    from array import array
    tr = array('I'); last = [-1]; samp = {}; cnt = [0]
    def whx_read(uc, off, size, ud):
        cnt[0] += 1
        u = off >> 7
        if u != last[0]:
            tr.append(u); last[0] = u
        u2 = (off + size - 1) >> 7
        if u2 != u:
            tr.append(u2); last[0] = u2
        if not (cnt[0] & 4095):
            n = symname(int(uc.reg_read(UC_ARM_REG_PC))); samp[n] = samp.get(n, 0) + 1
        return int.from_bytes(wad[off:off + size], 'little')
    uc.mmio_map(WADB, (len(wad) + 0xfff) & ~0xfff, whx_read, None, None, None)
    a.trace = a.trace or '/dev/null'
elif a.noop_hook:
    uc.hook_add(UC_HOOK_MEM_READ, lambda *x: None, begin=WADB, end=WADB + len(wad) - 1)
if a.trace and not a.mmio_whx:
    from array import array
    tr = array('I'); last = [-1]; samp = {}; cnt = [0]
    def rd_hook(uc, access, addr, size, val, ud):
        u = (addr - WADB) >> 7
        cnt[0] += 1
        if u != last[0]:
            tr.append(u); last[0] = u
        u2 = (addr + size - 1 - WADB) >> 7
        if u2 != u:
            tr.append(u2); last[0] = u2
        if not (cnt[0] & 4095):
            n = symname(int(uc.reg_read(UC_ARM_REG_PC))); samp[n] = samp.get(n, 0) + 1
    uc.hook_add(UC_HOOK_MEM_READ, rd_hook, begin=WADB, end=WADB + len(wad) - 1)
calls = {}
def call_hook(uc, addr, size, ud):
    calls[ud] = calls.get(ud, 0) + 1
for n in [x for x in a.calls.split(',') if x]:
    if n in syms: uc.hook_add(UC_HOOK_CODE, call_hook, n, begin=syms[n] & ~1, end=(syms[n] & ~1) + 1)
def demo_hook(uc, addr, size, ud):
    gt = struct.unpack('<i', bytes(uc.mem_read(syms['gametic'], 4)))[0] if 'gametic' in syms else -1
    hw = struct.unpack('<I', bytes(uc.mem_read(syms['fox_zone_hw'], 4)))[0] if 'fox_zone_hw' in syms else -1
    state['demos'].append((gt, state['frames'], hw))
    if a.qspi:
        snap = {nm: struct.unpack('<I', bytes(uc.mem_read(syms[nm], 4)))[0] for nm in ('whx_st_fast', 'whx_st_slow', 'whx_st_set', 'whx_st_miss') if nm in syms}
        snap.update(frames=state['frames'], qspi_cycles=q['cycles'], qspi_reads=q['reads'], chain=state['crc'], gametic=gt); state.setdefault('demo_snaps', []).append(snap)
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
if a.frame_crcs: open(a.frame_crcs, 'w').write('\n'.join(fcrc) + '\n')
for i, sn in enumerate(state.get('demo_snaps', [])):
    pv = state['demo_snaps'][i - 1] if i else {k: 0 for k in sn}
    d = {k: sn[k] - pv[k] for k in sn}
    print('EMU demo-end %d: frames %d gametic %d chain_crc %08x misses %d' % (i + 1, sn['frames'], sn['gametic'], sn['chain'], sn.get('whx_st_miss', 0)))
    acc = d.get('whx_st_fast', 0) + d.get('whx_st_slow', 0)
    print('EMU demo %d: frames %d accesses %d misses %d hit%% %.3f qspi_ms/frame %.2f misses/frame %.0f' % (i + 1, d['frames'], acc, d.get('whx_st_miss', 0), 100.0 * (1 - d.get('whx_st_miss', 0) / max(acc, 1)), d['qspi_cycles'] / d['frames'] / a.cpu_mhz / 1e3, d['qspi_reads'] / d['frames']))
print('EMU calls:', calls)
if a.qspi:
    pf = q['per_frame']
    print('EMU qspi reads %d bytes %d cycles %.0f (%.2f s at %.0f MHz CPU) frames %d' % (q['reads'], q['bytes'], q['cycles'], q['cycles'] / a.cpu_mhz / 1e6, a.cpu_mhz, len(pf)))
    if pf: print('EMU qspi per frame: mean %.0f cycles (%.2f ms), max %.0f cycles (%.2f ms)' % (sum(pf) / len(pf), sum(pf) / len(pf) / a.cpu_mhz / 1e3, max(pf), max(pf) / a.cpu_mhz / 1e3))
    for nm in ('whx_st_fast', 'whx_st_set', 'whx_st_miss', 'whx_st_slow', 'whx_st_bytes'):
        if nm in syms: print('EMU', nm, struct.unpack('<I', bytes(uc.mem_read(syms[nm], 4)))[0])
if a.trace:
    open(a.trace, 'wb').write(tr.tobytes())
    print('EMU whx reads', cnt[0], 'trace records', len(tr)); print('EMU read sample by function', sorted(samp.items(), key=lambda x: -x[1])[:40])
if 'fox_zone_hw' in syms:
    print('EMU zone_in_use_high_water_bytes', struct.unpack('<I', bytes(uc.mem_read(syms['fox_zone_hw'], 4)))[0])
print('EMU demos (gametic at end, frames so far, zone high water so far):', state['demos'])
print('EMU pc in', symname(int(uc.reg_read(UC_ARM_REG_PC))), hex(int(uc.reg_read(UC_ARM_REG_PC))), 'lr', hex(int(uc.reg_read(UC_ARM_REG_LR))), symname(int(uc.reg_read(UC_ARM_REG_LR)) & ~1))
if 'fox_stuck_sem' in syms:
    v = struct.unpack('<I', bytes(uc.mem_read(syms['fox_stuck_sem'], 4)))[0]
    if v: print('EMU stuck semaphore:', symname(v), hex(v))
for f0, n, ins, dc, qc, qr in state.get('wres', []): print(f"EMU window frames {f0}..{f0+n}: {ins} instructions, {ins/n:.0f} per frame, qspi {qc/n:.0f} cycles and {qr/n:.1f} reads per frame, calls in window {dc}")
print(f"EMU stop={state['stop']} instr={state['icount']} frames={state['frames']} framecrc={state['crc']:08x}")
if a.stack_probe: print(f"EMU stack_low={state['minsp']:#x} stack_used={sp0 - state['minsp']}")
