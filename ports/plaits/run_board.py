#!/usr/bin/env python3
"""Flash a Plaits bench image to the LP-MSPM33C321A (probe M3010001) under the probe lock, wait for the finished flag, read the results back over the
probe and compare every block CRC with the Mac reference (host/build.sh, then `ref v > ref.txt`).
  python3 ports/plaits/run_board.py <plaits.out> --ref <ref.txt> [--serial <probe>] [--mhz 160] [--no-flash] [--json out.json] [--keep ENGINE --keep-raw keep.raw]
Run from the repository root. Writes flash only (never the boot configuration). Plays no sound and drives no pin."""
import argparse, os, re, struct, subprocess, sys, json
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('--ref', required=True); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument('--mhz', type=float, default=160)
ap.add_argument('--no-flash', action='store_true'); ap.add_argument('--json'); ap.add_argument('--wait', default='120')
ap.add_argument('--keep', type=int, help='image built with EXTRA=-DPL_KEEP_ENGINE=N: also read the 256 blocks of PCM of that engine'); ap.add_argument('--keep-raw', help='Mac keep.raw of the same engine to compare sample by sample')
a = ap.parse_args()
NE, NB = 24, 256
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--', sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'),
       os.path.abspath(a.image), a.wait, 'pl_res:%d' % (NE * 48), 'pl_crc_blk:%d' % (NE * NB * 4), 'pl_cyc_blk:%d' % (NE * NB * 4), 'pl_info:12', 'pl_stack_free:8']
cmd += (['pl_keep:%d' % (NB * 12 * 4)] if a.keep is not None else [])
cmd += ['--serial', a.serial, '--until', 'pl_done=0x504C3254']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name):
    m = re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M); return bytes.fromhex(m.group(1))
res = hexof('pl_res'); crcb = struct.unpack('<%dI' % (NE * NB), hexof('pl_crc_blk')[:NE * NB * 4]); cycb = struct.unpack('<%dI' % (NE * NB), hexof('pl_cyc_blk')[:NE * NB * 4])
info = struct.unpack('<3I', hexof('pl_info')[:12]); stack_free = struct.unpack('<I', hexof('pl_stack_free')[:4])[0]
ref_all, ref_blk = {}, {}
for ln in open(a.ref):
    m = re.match(r'E (\d+) (\S+) active=\d+ crc_all=([0-9a-f]+)', ln)
    if m: ref_all[int(m.group(1))] = (m.group(2), int(m.group(3), 16))
    m = re.match(r'B (\d+) (\d+) ([0-9a-f]+)', ln)
    if m: ref_blk[(int(m.group(1)), int(m.group(2)))] = int(m.group(3), 16)
budget = a.mhz * 1e6 * 12 / 48000.0
out = {'voice_bytes': info[0], 'shared_bytes': info[1], 'stack_free_bytes': stack_free, 'budget_cycles_per_block': budget, 'engines': []}
print('%2s %-15s %8s %7s %8s %7s %9s  %s' % ('e', 'engine', 'mean', 'pct', 'worst', 'pct', 'first', 'against Mac'))
for e in range(NE):
    f = struct.unpack('<12I', res[e * 48:(e + 1) * 48])
    eng, blocks, crc_all, peak, nonzero, active, lo, hi, first, cmin, cmax, widx = f
    tot = hi << 32 | lo; rest = (tot - first) / (blocks - 1)
    bad = [b for b in range(NB) if crcb[e * NB + b] != ref_blk[(e, b)]]
    ok = (crc_all == ref_all[e][1])
    row = dict(engine=e, name=ref_all[e][0], blocks=blocks, mean_after_first=rest, mean_all=tot / blocks, pct_mean=100 * rest / budget, max=cmax, pct_worst=100 * cmax / budget, min=cmin, first=first,
               pct_first=100 * first / budget, worst_block=widx, crc_all=crc_all, crc_equal=ok, cycles_per_block=list(cycb[e * NB:(e + 1) * NB]), blocks_differ=len(bad), first_bad_block=bad[:1], peak=peak, nonzero=nonzero)
    out['engines'].append(row)
    print('%2d %-15s %8.0f %6.1f%% %8d %6.1f%% %9d  %s' % (e, row['name'], rest, row['pct_mean'], cmax, row['pct_worst'], first, 'EQUAL (256 of 256 blocks)' if ok and not bad else 'DIFFERENT, %d of 256 blocks, first bad %s' % (len(bad), bad[:1])))
print('voice object %d B, shared buffer %d B, stack never touched %d B' % (info[0], info[1], stack_free))
if a.keep is not None:
    pcm = struct.unpack('<%dh' % (NB * 24), hexof('pl_keep')[:NB * 48])
    if a.keep_raw:
        mac = struct.unpack('<%dh' % (NB * 24), open(a.keep_raw, 'rb').read()); d = [abs(x - y) for x, y in zip(pcm, mac)]
        out['keep'] = dict(engine=a.keep, samples=len(d), differ=sum(1 for v in d if v), max_error=max(d)); print('engine %d PCM, board against Mac: %d of %d samples differ, maximum error %d' % (a.keep, out['keep']['differ'], len(d), max(d)))
if a.json: json.dump(out, open(a.json, 'w'), indent=1)
