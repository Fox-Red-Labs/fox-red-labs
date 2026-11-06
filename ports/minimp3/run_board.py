#!/usr/bin/env python3
"""Flash the minimp3 bench image to the LP-MSPM33C321A (probe M3010001) under the probe lock, wait for the finished flag, read the
results back over the probe, compare the hashes with the Mac reference and print cycles, real-time factor and per-frame spread.
  python3 ports/minimp3/run_board.py <mp3.out> [--serial <probe>] [--mhz 160] [--no-flash]
Run from the repository root. Writes flash only (never the boot configuration). Plays no audio: nothing here touches a pin."""
import argparse, os, re, struct, subprocess, sys, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument('--mhz', type=float, default=160); ap.add_argument('--no-flash', action='store_true')
a = ap.parse_args()
DET = 256 * 4 * 2 + 2304 * 2 * 2
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--',
       sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'), os.path.abspath(a.image), '60',
       'mp_res:96', 'mp_det:%d' % (DET * 2), '--serial', a.serial, '--until', 'mp_done=0x4D503344']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name):
    m = re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M); return bytes.fromhex(m.group(1))
res = hexof('mp_res'); det = hexof('mp_det')
out = {}
for i, name in enumerate(('stereo128', 'mono64')):
    f = struct.unpack('<12I', res[i * 48:(i + 1) * 48])
    magic, frames, ch, hz, kbps, spc, h, clo, chi, cmin, cmax, used = f
    d = det[i * DET:(i + 1) * DET]
    hashes = struct.unpack('<256I', d[:1024])[:frames]; cyc = struct.unpack('<256I', d[1024:2048])[:frames]
    pcm0 = d[2048:2048 + 4608]
    tot = chi << 32 | clo; secs = spc / hz
    ref = subprocess.run([os.path.join(REPO, 'ports/minimp3/results/host/ref_scalar'), os.path.join(REPO, 'ports/minimp3/clips/%s.mp3' % name)], capture_output=True, text=True).stdout.split('\n')
    rh = re.search(r'hash_all=(0x[0-9a-f]+)', ref[0]).group(1); rf = [int(x.split()[2], 16) for x in ref[1:] if x.startswith('F ')]
    bad = [j for j in range(frames) if hashes[j] != rf[j]] if len(rf) == frames else ['frame count differs']
    mhz = a.mhz
    print('%s: magic %#x frames %d ch %d hz %d kbps %d samples/ch %d used %d' % (name, magic, frames, ch, hz, kbps, spc, used))
    print('  hash_all board 0x%08x  mac-scalar %s  %s ; frames differing: %s' % (h, rh, 'EQUAL' if '0x%08x' % h == rh else 'DIFFERENT', bad[:10]))
    print('  decode cycles %d = %.1f ms at %g MHz for %.3f s of audio -> real-time factor %.4f (%.1f MHz-equivalent needed for real time)' % (tot, tot / mhz / 1e3, mhz, secs, tot / mhz / 1e6 / secs, tot / secs / 1e6))
    print('  per frame cycles: min %d max %d mean %.0f  (frame = %d samples/ch = %.2f ms of audio)' % (cmin, cmax, tot / frames, 1152, 1152 / hz * 1e3))
    last = d[2048 + 4608:2048 + 4608 * 2][:1152 * ch * 2]
    macraw = open(os.path.join(REPO, 'ports/minimp3/results/host/%s-scalar.raw' % name), 'rb').read()[-1152 * ch * 2:]
    sb = struct.unpack('<%dh' % (1152 * ch), last); sm = struct.unpack('<%dh' % (1152 * ch), macraw)
    print('  last frame PCM, board against Mac scalar: %d samples, max sample error %d, energy max |x| %d' % (len(sb), max(abs(x - y) for x, y in zip(sb, sm)), max(abs(x) for x in sb)))
    out[name] = dict(frames=frames, hash=h, ref=rh, cycles=tot, secs=secs, rtf=tot / mhz / 1e6 / secs, cmin=cmin, cmax=cmax)
print(json.dumps(out))
