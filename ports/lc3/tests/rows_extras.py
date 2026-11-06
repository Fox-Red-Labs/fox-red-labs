#!/usr/bin/env python3
"""Write ports/lc3/RESULTS-ROWS-EXTRAS.csv (results-store rows) from results/boards/lc3x-O2-hard-strict.json. Run from the repository root."""
import csv, json, os
R = os.path.join(os.path.dirname(__file__), '..')
d = json.load(open(os.path.join(R, 'results/boards/lc3x-O2-hard-strict.json')))
SRC = 'ports/lc3/results/boards/lc3x-O2-hard-strict.json'
COND = ('liblc3 v1.1.3 unmodified (LC3plus on); TI Arm Clang 5.1.1.LTS -O2, hard float (fpv5-sp-d16), -ffp-contract=off, no fast-math; %s; DWT cycles around each lc3_encode and lc3_decode call, '
        'summed over the channels of a frame, signal generation and CRC excluded; code and data in flash')
cols = 'board,test,variant,status,value,value_hi,runs,clock_mhz,conditions,date,source,checked,note'.split(',')
rows = []
def add(test, var, val, cond, checked, note, src=SRC):
    rows.append(dict(board='mspm33c321a', test=test, variant=var, status='measured', value=val, value_hi='', runs=1, clock_mhz=160, conditions=cond, date='2026-10-21', source=src, checked=checked, note=note))
for r in d['cfg']:
    dt = r['dt_us']; ms = {7500: '7p5ms', 5000: '5ms', 2500: '2p5ms', 10000: '10ms'}[dt]
    kind = 'plc' if r['loss_every'] else ('stereo' if r['nch'] == 2 else 'mono')
    var = '%s-%dk-%dkbps-%s-o2-hard-strict' % (ms, r['hz'] // 1000, r['kbps'], kind) if kind != 'mono' else '%s-%dk-%dkbps-o2-hard-strict' % (ms, r['hz'] // 1000, r['kbps'])
    what = '%s ms frames, %d channel%s, %d bytes per channel per frame, %d frames (1 s of audio)%s' % (dt / 1000, r['nch'], 's' if r['nch'] > 1 else '', r['nbytes'], r['frames'],
            ', every %dth frame lost (decoder called with no data)' % r['loss_every'] if r['loss_every'] else '')
    cond = COND % what
    fc = 160e6 * dt / 1e6
    add('lc3_encode_cycles', var, round(r['enc_mean']), cond, 'readback', 'mean cycles per frame (all channels); worst frame %d cycles' % r['enc_max'])
    add('lc3_decode_cycles', var, round(r['dec_mean']), cond, 'readback', 'mean cycles per frame (all channels); worst frame %d cycles' % r['dec_max'])
    add('lc3_encode_pct_rt', var, round(r['enc_pct'], 1), cond, 'readback', 'percent of one %s ms frame at 160 MHz' % (dt / 1000))
    add('lc3_decode_pct_rt', var, round(r['dec_pct'], 1), cond, 'readback', 'percent of one %s ms frame at 160 MHz' % (dt / 1000))
    if r['loss_every']:
        add('lc3_decode_lost_cycles', var, round(r['lost_mean']), cond, 'readback', 'mean cycles of a concealed frame (%.0f percent of a good frame at %d cycles)' % (100 * r['lost_mean'] / r['good_mean'], round(r['good_mean'])))
    add('lc3_frames_differ', var + '-vs-mac-strict', 0 if (r['bits_equal'] and r['pcm_equal']) else 1, cond, 'crc',
        'whole-run CRC-32 of the bitstream and of the decoded PCM against the Mac strict build; 0 is bit-exact (equal: bits %s, PCM %s)' % (r['bits_equal'], r['pcm_equal']))
    assert r['errors'] == 0 and r['bits_equal'] and r['pcm_equal']
add('lc3_flash_bytes', 'extras-o2-hard-strict', 162960, 'liblc3 v1.1.3 unmodified; extras bench image (codec, tables, math library, SysConfig clock setup, harness for 15 runs)', 'linker', 'tiarmsize text plus data; 2.5 and 5 ms frames need LC3plus, so this is the full build', 'ports/lc3/results/boards/lc3x-O2-hard-strict.json')
add('lc3_stack_peak_bytes', 'extras-o2-hard-strict', 16384 - d['stack_free_bytes'], 'liblc3 v1.1.3 unmodified; stack painted with 0xDEADBEEF before the run, 16384 bytes reserved', 'readback', 'deepest stack use over the 15 runs (two channels use separate state, one shared stack)')
w = csv.DictWriter(open(os.path.join(R, 'RESULTS-ROWS-EXTRAS.csv'), 'w', newline=''), fieldnames=cols, lineterminator='\n'); w.writeheader(); w.writerows(rows); print(len(rows), 'rows')
