#!/usr/bin/env python3
"""Flash the LC3 extras image to the LP-MSPM33C321A (probe M3010001) under the probe lock, read the results over the probe, compare the CRCs
with the Mac strict reference (ports/lc3/results/host/refx_strict.txt) and print cycles and share of real time.
  python3 ports/lc3/run_boardx.py <lc3x.out> [--serial <probe>] [--mhz 160] [--no-flash] [--json out.json]
Run from the repository root after ports/lc3/host/buildx.sh. Writes flash only. Plays no sound and drives no pin."""
import argparse, os, re, struct, subprocess, sys, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument('--mhz', type=float, default=160)
ap.add_argument('--no-flash', action='store_true'); ap.add_argument('--json'); ap.add_argument('--wait', default='180')
a = ap.parse_args()
NW = 29
refs = [dict(zip(('bits', 'pcm'), (int(m.group(1), 16), int(m.group(2), 16)))) for m in
        (re.search(r'bits_crc=0x([0-9a-f]+) pcm_crc=0x([0-9a-f]+)', l) for l in open(os.path.join(REPO, 'ports/lc3/results/host/refx_strict.txt'))) if m]
n = len(refs)
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--', sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'),
       os.path.abspath(a.image), a.wait, 'lc3x_res:%d' % (n * NW * 4), 'lc3x_stack_free:8', '--serial', a.serial, '--until', 'lc3x_done=0x4C583344']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO); print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name): return bytes.fromhex(re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M).group(1))
res = hexof('lc3x_res'); stack_free = struct.unpack('<I', hexof('lc3x_stack_free')[:4])[0]
out = {'stack_free_bytes': stack_free, 'cfg': []}
for i in range(n):
    f = struct.unpack('<%dI' % NW, res[i * NW * 4:(i + 1) * NW * 4])
    (magic, dt, hz, kbps, nch, loss, nb, frames, errs, esz, dsz, bcrc, pcrc, elo, ehi, dlo, dhi, emin, emax, dmin, dmax, glo, ghi, gn, llo, lhi, ln, h1, h2) = f
    et = ehi << 32 | elo; dt_ = dhi << 32 | dlo; fc = a.mhz * 1e6 * dt / 1e6
    row = dict(dt_us=dt, hz=hz, kbps=kbps, nch=nch, loss_every=loss, nbytes=nb, frames=frames, errors=errs, magic=hex(magic), enc_state=esz, dec_state=dsz,
               enc_mean=et / frames, dec_mean=dt_ / frames, enc_min=emin, enc_max=emax, dec_min=dmin, dec_max=dmax, enc_pct=100 * et / frames / fc, dec_pct=100 * dt_ / frames / fc,
               enc_pct_worst=100 * emax / fc, dec_pct_worst=100 * dmax / fc, bits_crc=bcrc, pcm_crc=pcrc, bits_equal=bcrc == refs[i]['bits'], pcm_equal=pcrc == refs[i]['pcm'])
    row['dec_half1_mean'] = h1 / (frames // 2); row['dec_half2_mean'] = h2 / (frames - frames // 2)
    if gn: row['good_mean'] = ((ghi << 32 | glo) / gn)
    if ln: row['lost_mean'] = ((lhi << 32 | llo) / ln)
    out['cfg'].append(row)
    print('%4.1f ms %5d Hz %3d kbps x%d loss/%d (%d B) magic %s err %d: enc %.0f cyc (%.1f%%, worst %.1f%%) dec %.0f cyc (%.1f%%, worst %.1f%%)%s | bits %s pcm %s' % (
        dt / 1000, hz, kbps, nch, loss, nb, hex(magic), errs, row['enc_mean'], row['enc_pct'], row['enc_pct_worst'], row['dec_mean'], row['dec_pct'], row['dec_pct_worst'],
        (' good %.0f lost %.0f' % (row['good_mean'], row['lost_mean'])) if ln else ' halves %.0f/%.0f' % (row['dec_half1_mean'], row['dec_half2_mean']), 'EQUAL' if row['bits_equal'] else 'DIFF', 'EQUAL' if row['pcm_equal'] else 'DIFF'))
print('stack never touched: %d bytes' % stack_free)
if a.json: json.dump(out, open(a.json, 'w'), indent=1)
