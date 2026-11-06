#!/usr/bin/env python3
"""Flash the llama2c image to the LP-MSPM33C321A (probe M3010001) under the probe lock, wait for the run to finish,
read the result block back over the debug probe, and print the text, the token ids and tokens per second.
  python3 ports/llama2c/run_board.py <llama2c.out> [--serial <probe>] [--no-flash]
Run it from the repository root. Writes flash only (never the boot configuration); nothing else on the board is touched.
Compare the ids with ports/llama2c/results/ids-int8-seed42.txt (Mac) and ids-m33-emulator.txt (emulator)."""
import argparse, os, re, subprocess, sys
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('--serial', default=os.environ.get("PROBE"), required=not os.environ.get("PROBE")); ap.add_argument('--no-flash', action='store_true')
a = ap.parse_args()
cmd = [sys.executable, os.path.join(REPO, 'tools/frl/lock.py'), 'exec', '--serial', a.serial, '--',
       sys.executable, os.path.join(REPO, 'mspm33c321a/tools/run_image.py'), os.path.abspath(a.image), '40']
cmd = cmd + ['ll_result:36', 'll_ids:512', 'll_fwd_cycles:512', 'll_out:1024', '--text', 'll_out', '--serial', a.serial, '--until', 'll_result=0x4C4C4D32']
if a.no_flash: cmd.append('--no-flash')
r = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
print(r.stderr.strip())
if r.returncode: print(r.stdout[-800:]); sys.exit(r.returncode)
def hexof(name):
    m = re.search(r'^%s @0x[0-9a-f]+: ([0-9a-f]+)$' % name, r.stdout, re.M); return bytes.fromhex(m.group(1))
import struct
magic, rc, ntok, nprompt, outlen, clo, chi, rlo, rhi = struct.unpack('<9I', hexof('ll_result')[:36])
ids = struct.unpack('<128i', hexof('ll_ids')[:512])[:ntok]
cyc = struct.unpack('<128I', hexof('ll_fwd_cycles')[:512])[:ntok]
text = r.stdout.split('ll_out @')[1].split(':\n', 1)[1].rstrip('\n')
print('magic %#x init_rc %d tokens %d prompt_tokens %d' % (magic, rc, ntok, nprompt))
print('TEXT:', text.replace('\n', '\\n'))
print('IDS:', ' '.join(map(str, ids)))
tot = chi << 32 | clo
print('forward-pass cycles total %d  (%.0f per token, min %d max %d)' % (tot, tot / ntok, min(cyc), max(cyc)))
print('tokens per second at 160 MHz, forward passes only: %.1f' % (ntok / (tot / 160e6)))
print('whole generate call: %d cycles, %.1f tokens per second' % (rhi << 32 | rlo, ntok / ((rhi << 32 | rlo) / 160e6)))
