#!/usr/bin/env python3
"""Flash an LC3 bench image to the LP-MSPM33C321A (probe M3010001) under the probe lock, wait for the finished flag, read the
results back over the probe, compare the CRCs with both Mac references (strict and fast-math) and print cycles, share of real time and stack use.
  python3 ports/lc3/run_board.py <lc3.out> [--serial <probe>] [--mhz 160] [--no-flash] [--json out.json]
Run from the repository root after ports/lc3/host/build.sh. Writes flash only (never the boot configuration). Plays no sound and drives no pin."""
import argparse, os, re, struct, subprocess, sys, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument('--mhz', type=float, default=160)
ap.add_argument('--no-flash', action='store_true'); ap.add_argument('--json'); ap.add_argument('--wait', default='120'); ap.add_argument('--pcm', action='store_true', help='image built with -DLC3_KEEP_OUT: also read the decoded 48 kHz PCM and compare sample by sample')
a = ap.parse_args()
DET = 4 * 100 * 4   # note: DSLite with -s 8 returns four times the bytes asked for; the first ones are the symbol, the rest is ignored
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--',
       sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'), os.path.abspath(a.image), a.wait,
       'lc3_res:216', 'lc3_det:%d' % (DET * 3), 'lc3_stack_free:8',] + (['keep_out:24000'] if a.pcm else []) + [ '--serial', a.serial, '--until', 'lc3_done=0x4C433344']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name):
    m = re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M); return bytes.fromhex(m.group(1))
res = hexof('lc3_res'); det = hexof('lc3_det'); stack_free = struct.unpack('<I', hexof('lc3_stack_free')[:4])[0]
def refs(kind):
    out = subprocess.run([os.path.join(REPO, 'ports/lc3/results/host/ref_' + kind), 'v'], capture_output=True, text=True).stdout.split('\n')
    cfgs = []
    for ln in out:
        m = re.match(r'cfg (\d) .* bits_crc=0x([0-9a-f]+) pcm_crc=0x([0-9a-f]+)', ln)
        if m: cfgs.append(dict(bits=int(m.group(2), 16), pcm=int(m.group(3), 16), enc=[], dec=[]))
        m = re.match(r'F (\d+) ([0-9a-f]+) ([0-9a-f]+)', ln)
        if m: cfgs[-1]['enc'].append(int(m.group(2), 16)); cfgs[-1]['dec'].append(int(m.group(3), 16))
    return cfgs
R = {k: refs(k) for k in ('strict', 'fast')}
out = {'stack_free_bytes': stack_free, 'cfg': []}
for i in range(3):
    f = struct.unpack('<18I', res[i * 72:(i + 1) * 72])
    magic, hz, kbps, nb, frames, esz, dsz, bcrc, pcrc, elo, ehi, dlo, dhi, emin, emax, dmin, dmax, errs = f
    d = det[i * DET:(i + 1) * DET]
    ecrc = struct.unpack('<100I', d[0:400]); dcrc = struct.unpack('<100I', d[400:800]); ecyc = struct.unpack('<100I', d[800:1200]); dcyc = struct.unpack('<100I', d[1200:1600])
    et = ehi << 32 | elo; dt = dhi << 32 | dlo; frame_cyc = a.mhz * 1e6 * 0.01
    print('%d Hz %d kbps (%d bytes/frame): magic %#x frames %d errors %d  encoder state %d B, decoder state %d B' % (hz, kbps, nb, magic, frames, errs, esz, dsz))
    row = dict(hz=hz, kbps=kbps, nbytes=nb, errors=errs, enc_state=esz, dec_state=dsz, enc_mean=et / frames, dec_mean=dt / frames, enc_min=emin, enc_max=emax, dec_min=dmin, dec_max=dmax,
               enc_pct=100 * et / frames / frame_cyc, dec_pct=100 * dt / frames / frame_cyc, enc_pct_worst=100 * emax / frame_cyc, dec_pct_worst=100 * dmax / frame_cyc, bits_crc=bcrc, pcm_crc=pcrc)
    print('  encode cycles per 10 ms frame: mean %.0f  min %d  max %d  -> %.1f percent of real time at %g MHz (worst frame %.1f)' % (et / frames, emin, emax, row['enc_pct'], a.mhz, row['enc_pct_worst']))
    print('  decode cycles per 10 ms frame: mean %.0f  min %d  max %d  -> %.1f percent (worst frame %.1f)' % (dt / frames, dmin, dmax, row['dec_pct'], row['dec_pct_worst']))
    for k in ('strict', 'fast'):
        ref = R[k][i]; bad_e = [j for j in range(frames) if ecrc[j] != ref['enc'][j]]; bad_d = [j for j in range(frames) if dcrc[j] != ref['dec'][j]]
        row['vs_' + k] = dict(bits_equal=bcrc == ref['bits'], pcm_equal=pcrc == ref['pcm'], enc_frames_differ=len(bad_e), dec_frames_differ=len(bad_d), first_bad_enc=bad_e[:1], first_bad_dec=bad_d[:1])
        print('  against Mac %-6s: bitstream CRC %s (%d of %d frames differ), decoded PCM CRC %s (%d of %d frames differ)' % (k, 'EQUAL' if bcrc == ref['bits'] else 'different', len(bad_e), frames, 'EQUAL' if pcrc == ref['pcm'] else 'different', len(bad_d), frames))
    print('  bitstream CRC 0x%08x  PCM CRC 0x%08x' % (bcrc, pcrc))
    out['cfg'].append(row)
print('stack never touched: %d bytes' % stack_free)
if a.pcm:
    pcm = struct.unpack('<48000h', hexof('keep_out')[:96000])
    for k in ('strict', 'fast'):
        mac = struct.unpack('<48000h', open(os.path.join(REPO, 'ports/lc3/results/host/pcm48k_%s.raw' % k), 'rb').read())
        diff = [abs(x - y) for x, y in zip(pcm, mac)]
        out['pcm48k_vs_' + k] = dict(samples=48000, differ=sum(1 for v in diff if v), max_error=max(diff))
        print('48 kHz decoded PCM, board against Mac %s: %d of 48000 samples differ, maximum error %d' % (k, sum(1 for v in diff if v), max(diff)))
if a.json: json.dump(out, open(a.json, 'w'), indent=1)
