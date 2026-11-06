#!/usr/bin/env python3
"""Run the emulator build of the llama2c demo (build.sh O2 emu) on a Cortex-M33 model (Unicorn) and report the text,
the token ids and the instructions spent in each forward pass.
  python3 -I emu_m33.py <llama2c.out> [--nm tiarmnm] [--max-instr N]
Needs: pip install unicorn. The emulator runs the same ARM code as the board and counts instructions; it does not model
wait states, so cycles per instruction are an estimate (see README). Stops when the program reaches ll_halt."""
import sys, struct, subprocess, argparse, json
from unicorn import *
from unicorn.arm_const import *

ap = argparse.ArgumentParser()
ap.add_argument('image'); ap.add_argument('--nm', default='tiarmnm'); ap.add_argument('--max-instr', type=int, default=0)
ap.add_argument('--stack-probe', action='store_true', help='report the deepest stack use'); ap.add_argument('--json'); ap.add_argument('--no-count', action='store_true', help='skip instruction counting: much faster, text and ids only')
a = ap.parse_args()
FLASH, RAM = 0x10000000, 0x30000000
elf = open(a.image, 'rb').read()
assert elf[:4] == b'\x7fELF'
e_phoff, = struct.unpack_from('<I', elf, 0x1c); e_phentsize, e_phnum = struct.unpack_from('<HH', elf, 0x2a)
segs = []
for i in range(e_phnum):
    p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', elf, e_phoff + i * e_phentsize)
    if p_type == 1 and p_filesz: segs.append((p_paddr, elf[p_off:p_off + p_filesz]))
syms = {}
for l in subprocess.check_output([a.nm, a.image]).decode().splitlines():
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M33)
uc.mem_map(0xE000E000, 0x1000); uc.mem_map(FLASH, 0x100000); uc.mem_map(RAM, 0x40000)   # 1 MB flash, 256 kB RAM, as the chip
for addr, data in segs: uc.mem_write(addr, data)
sp0, pc0 = struct.unpack_from('<II', bytes(uc.mem_read(FLASH, 8)))
uc.reg_write(UC_ARM_REG_SP, sp0)
# the FPU is off at reset; the startup code or main enables it through CPACR (0xE000ED88), which sits in the mapped page
state = {'n': 0, 'marks': [], 'stop': None}
def count(uc, addr, size, ud): state['n'] += 1
def mark(uc, addr, size, ud): state['marks'].append(state['n'])
def stop(uc, addr, size, ud): state['stop'] = 'll_halt'; uc.emu_stop()
fwd = syms['forward'] & ~1
if not a.no_count: uc.hook_add(UC_HOOK_CODE, count)
if not a.no_count: uc.hook_add(UC_HOOK_CODE, mark, begin=fwd, end=fwd + 1)
h = syms['ll_halt'] & ~1
uc.hook_add(UC_HOOK_CODE, stop, begin=h, end=h + 1)
minsp = [sp0]
def sp_probe(uc, addr, size, ud):
    sp = uc.reg_read(UC_ARM_REG_SP)
    if sp < minsp[0]: minsp[0] = sp
if a.stack_probe: uc.hook_add(UC_HOOK_BLOCK, sp_probe)
try:
    uc.emu_start(pc0 | 1, 0, count=a.max_instr if a.max_instr else 0)
except UcError as ex:
    state['stop'] = f"UcError {ex} at pc={uc.reg_read(UC_ARM_REG_PC):#x}"
state['marks'].append(state['n'])
res = struct.unpack('<9I', bytes(uc.mem_read(syms['ll_result'], 36)))
magic, rc, ntok, nprompt, outlen = res[:5]
text = bytes(uc.mem_read(syms['ll_out'], outlen)).decode(errors='replace')
ids = struct.unpack(f'<{ntok}i', bytes(uc.mem_read(syms['ll_ids'], 4 * ntok))) if ntok else ()
per = [state['marks'][i + 1] - state['marks'][i] for i in range(len(state['marks']) - 1)]
print('stop:', state['stop'], ' magic %#x init_rc %d tokens %d prompt_tokens %d' % (magic, rc, ntok, nprompt))
if a.stack_probe: print('deepest stack use (checked at block starts): %d bytes' % (sp0 - minsp[0]))
print('TEXT:', text.replace('\n', '\\n'))
print('IDS:', ' '.join(map(str, ids)))
if per and not a.no_count: print('instructions total %d, per forward pass: min %d max %d mean %.0f' % (state['n'], min(per), max(per), sum(per) / len(per)))
if a.json: json.dump({'text': text, 'ids': ids, 'instr_per_pass': per, 'total': state['n']}, open(a.json, 'w'))
