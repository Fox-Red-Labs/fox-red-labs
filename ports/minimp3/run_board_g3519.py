#!/usr/bin/env python3
"""Flash the minimp3 bench to the LP-MSPM0G3519 (probe MG000001) under the probe lock, wait for the finished flag, read the results
over the probe (DSLite, core never halted or reset), check the clock against the host's clock and compare every frame checksum with the Mac scalar build.
  python3 ports/minimp3/run_board_g3519.py <mp3.elf> [--no-flash] [--mhz 80]
Run from the repository root. Writes flash only (never the boot configuration). Plays no sound and touches no pin."""
import argparse, os, re, struct, subprocess, sys, time, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DS = '/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite'
ap = argparse.ArgumentParser(); ap.add_argument('elf'); ap.add_argument('--no-flash', action='store_true'); ap.add_argument('--mhz', type=float, default=80)
ap.add_argument('--limit', type=float, default=400); a = ap.parse_args()
elf = os.path.abspath(a.elf); d = os.path.dirname(elf)
syms = {}
for l in subprocess.run(['arm-none-eabi-nm', elf], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
def lock(*cmd): return ['python3', os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', os.environ.get('PROBE') or sys.exit('set PROBE to the probe serial'), '--'] + list(cmd)
def words(addr, n, tries=4):
    """Read n words with DSLite `memory` (it does not reset the core; an OpenOCD connect does, and wipes the run)."""
    for _ in range(tries):
        r = subprocess.run(lock(DS, 'memory', '-c', os.path.join(REPO, 'mspm0g3519/MSPM0G3519_MG000001.ccxml'), '-r', '0x%x,%d' % (addr, n * 4), '-s', '8', '-o', OUT), capture_output=True, text=True, cwd=REPO)
        try:
            b = open(OUT, 'rb').read()
            if r.returncode == 0 and len(b) >= n * 4: return list(struct.unpack('<%dI' % n, b[:n * 4]))
        except OSError: pass
    raise RuntimeError('read failed: ' + r.stdout[-300:])
OUT = os.path.join(d, 'mem.bin')
if not a.no_flash:
    b = open(elf.replace('.elf', '.bin'), 'rb').read(); b += b'\xff' * (-len(b) % 16); pb = elf.replace('.elf', '-pad.bin'); open(pb, 'wb').write(b)
    t = time.time(); r = subprocess.run(lock(DS, 'flash', '-c', os.path.join(REPO, 'mspm0g3519/MSPM0G3519_MG000001.ccxml'), '-e', '-f', '-v', pb + ',0x0'), capture_output=True, text=True, cwd=REPO)
    if r.returncode: print(r.stdout[-600:], r.stderr[-600:]); sys.exit(1)
    print('flash %.1f s' % (time.time() - t), file=sys.stderr)
t_start = time.time()
# the clock check inside the firmware takes 3 s (80M + 160M cycles); poll for the finished flag
while time.time() - t_start < a.limit:
    time.sleep(5)
    if words(syms['mp_done'], 1)[0] == 0x4D503344: break
else: print('timeout, mp_done not set'); sys.exit(1)
t_done = time.time()
val = words(syms['g_val'], 4)
print('wall from flash end to done flag: %.0f s (polls every 5 s plus connect time)' % (t_done - t_start))
print('clock check: counter across delay_cycles(80M) = %d, (160M) = %d (80 MHz would give 80000000 and 160000000)' % (val[0], val[1]))
print('whole bench (both clips, hashing included) = %d cycles = %.2f s at %g MHz; marker %#x' % (val[2], val[2] / a.mhz / 1e6, a.mhz, val[3]))
# independent clock check: the idle loop refreshes g_tick; read it twice, 20 s apart by the host clock
def tick(): return words(syms['g_tick'], 1)[0], time.time()
t1, h1 = tick(); time.sleep(20); t2, h2 = tick()
dt = ((t2 - t1) & 0xFFFFFFFF); host = h2 - h1
print('host clock check: counter advanced %d cycles in %.3f s of host time = %.3f MHz (host read jitter about 0.3 s)' % (dt, host, dt / host / 1e6))
res = words(syms['mp_res'], 24); DET = 256 * 4 * 2 + 2304 * 2 * 2
det = b''
for i in range(2):
    ws = words(syms['mp_det'] + i * DET, 512); det += struct.pack('<512I', *ws)
out = {}
for i, name in enumerate(('stereo128', 'mono64')):
    magic, frames, ch, hz, kbps, spc, h, clo, chi, cmin, cmax, used = res[i * 12:(i + 1) * 12]
    dd = det[i * 2048:(i + 1) * 2048]
    hashes = struct.unpack('<256I', dd[:1024])[:frames]; cyc = struct.unpack('<256I', dd[1024:2048])[:frames]
    tot = chi << 32 | clo; secs = spc / hz
    ref = subprocess.run([os.path.join(REPO, 'ports/minimp3/results/host/ref_scalar'), os.path.join(REPO, 'ports/minimp3/clips/%s.mp3' % name)], capture_output=True, text=True).stdout.split('\n')
    rh = re.search(r'hash_all=(0x[0-9a-f]+)', ref[0]).group(1); rf = [int(x.split()[2], 16) for x in ref[1:] if x.startswith('F ')]
    bad = [j for j in range(frames) if hashes[j] != rf[j]] if len(rf) == frames else ['frame count differs']
    print('%s: magic %#x frames %d ch %d hz %d kbps %d samples/ch %d used %d' % (name, magic, frames, ch, hz, kbps, spc, used))
    print('  hash_all board 0x%08x  mac-scalar %s  %s ; frames differing: %s' % (h, rh, 'EQUAL' if '0x%08x' % h == rh else 'DIFFERENT', bad[:10]))
    print('  decode cycles %d = %.1f ms at %g MHz for %.3f s of audio -> real-time factor %.3f (%.0f MHz-equivalent needed)' % (tot, tot / a.mhz / 1e3, a.mhz, secs, tot / a.mhz / 1e6 / secs, tot / secs / 1e6))
    print('  per frame cycles: min %d max %d mean %.0f ; frame 0 %d, frame 1 %d, frames 2-13 mean %.0f' % (cmin, cmax, tot / frames, cyc[0], cyc[1], sum(cyc[2:14]) / 12))
    out[name] = dict(frames=frames, hash=h, ref=rh, cycles=tot, secs=secs, rtf=tot / a.mhz / 1e6 / secs, cmin=cmin, cmax=cmax, bad=len(bad), steady=sum(cyc[2:14]) / 12)
print(json.dumps(dict(val=val, host_mhz=dt / host / 1e6, **out)))
