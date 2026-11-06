#!/usr/bin/env python3
"""Flash an Opus bench image to the LP-MSPM33C321A (probe M3010001) under the probe lock, read the results over the probe, compare the CRCs
with the Mac reference of the same kind (ports/opus/results/host/ref_<fixed|float>.txt) and print cycles and share of real time.
  python3 ports/opus/run_board.py <opus.out> fixed|float [--ref fixed32] [--serial <probe>] [--mhz 160] [--no-flash] [--json out.json]
Run from the repository root after ports/opus/host/build.sh. Writes flash only. Plays no sound and drives no pin."""
import argparse, os, re, struct, subprocess, sys, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('kind', choices=['fixed', 'float']); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE"))
ap.add_argument('--mhz', type=float, default=160); ap.add_argument('--no-flash', action='store_true'); ap.add_argument('--json'); ap.add_argument('--ref', help='Mac reference name if not the kind (fixed32 = Mac build with OPUS_FAST_INT64 off)'); ap.add_argument('--wait', default='240')
a = ap.parse_args()
NW = 27
refs = []
FR = []
for l in open(os.path.join(REPO, 'ports/opus/results/host/ref_%s.txt' % (a.ref or a.kind))):
    m2 = re.match(r'F (\d+) ([0-9a-f]+) ([0-9a-f]+)', l)
    if m2: FR[-1].append((int(m2.group(2), 16), int(m2.group(3), 16)))
    if l.startswith('cfg'): FR.append([])
    m = re.search(r'bits_crc=0x([0-9a-f]+) pcm_crc=0x([0-9a-f]+)', l)
    if m: refs.append((int(m.group(1), 16), int(m.group(2), 16)))
n = len(refs)
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--', sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'),
       os.path.abspath(a.image), a.wait, 'op_res:%d' % (n * NW * 4), 'op_det:%d' % (n * 800), 'op_stack_free:8', '--serial', a.serial, '--until', 'op_done=0x4F505244']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO); print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name): return bytes.fromhex(re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M).group(1))
res = hexof('op_res'); det = hexof('op_det'); stack_free = struct.unpack('<I', hexof('op_stack_free')[:4])[0]
APP = ['voip', 'audio', 'lowdelay']
out = {'kind': a.kind, 'stack_free_bytes': stack_free, 'cfg': []}
for i in range(n):
    f = struct.unpack('<%dI' % NW, res[i * NW * 4:(i + 1) * NW * 4])
    (magic, hz, fus, kbps, app, cx, frames, errs, esz, dsz, bcrc, pcrc, elo, ehi, dlo, dhi, emin, emax, dmin, dmax, btot, bmin, bmax, ns, nh, nc, toc0) = f
    et = ehi << 32 | elo; dt = dhi << 32 | dlo; fc = a.mhz * 1e6 * fus / 1e6
    row = dict(hz=hz, frame_us=fus, kbps=kbps, app=APP[app], complexity=cx, frames=frames, errors=errs, magic=hex(magic), enc_state=esz, dec_state=dsz, toc0=hex(toc0),
               silk=ns, hybrid=nh, celt=nc, bytes_total=btot, bytes_min=bmin, bytes_max=bmax, enc_mean=et / frames, dec_mean=dt / frames, enc_min=emin, enc_max=emax, dec_min=dmin, dec_max=dmax,
               enc_pct=100 * et / frames / fc, dec_pct=100 * dt / frames / fc, enc_pct_worst=100 * emax / fc, dec_pct_worst=100 * dmax / fc,
               bits_crc=bcrc, pcm_crc=pcrc, bits_equal=bcrc == refs[i][0], pcm_equal=pcrc == refs[i][1])
    ec = struct.unpack('<100I', det[i * 800:i * 800 + 400]); dc = struct.unpack('<100I', det[i * 800 + 400:i * 800 + 800])
    row['enc_frames_differ'] = sum(1 for j in range(frames) if ec[j] != FR[i][j][0]); row['dec_frames_differ'] = sum(1 for j in range(frames) if dc[j] != FR[i][j][1])
    out['cfg'].append(row)
    print('%5d Hz %4.1f ms %3d kbps %-8s c%-2d magic %s err %d toc %s (silk %d hyb %d celt %d, %d B/pkt): enc %.0f cyc (%.1f%%, worst %.1f%%) dec %.0f cyc (%.1f%%, worst %.1f%%) | bits %s pcm %s' % (
        hz, fus / 1000, kbps, APP[app], cx, hex(magic), errs, hex(toc0), ns, nh, nc, bmax, row['enc_mean'], row['enc_pct'], row['enc_pct_worst'], row['dec_mean'], row['dec_pct'], row['dec_pct_worst'],
        'EQUAL' if row['bits_equal'] else 'DIFF %d/%d' % (row['enc_frames_differ'], frames), 'EQUAL' if row['pcm_equal'] else 'DIFF %d/%d' % (row['dec_frames_differ'], frames)))
print('stack never touched: %d bytes' % stack_free)
if a.json: json.dump(out, open(a.json, 'w'), indent=1)
