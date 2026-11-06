#!/usr/bin/env python3
"""Make RESULTS-ROWS.csv from the board JSON files in results/boards/, the emulator JSON in results/ and the size table results/sizes.json.
  python3 -I ports/lc3/tools/make_rows.py > RESULTS-ROWS.csv"""
import json, os, csv, sys
H = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'results')
sizes = json.load(open(os.path.join(H, 'sizes.json')))
cols = 'board,test,variant,status,value,value_hi,runs,clock_mhz,conditions,date,source,checked,note'.split(',')
w = csv.writer(sys.stdout, lineterminator='\n'); w.writerow(cols)
DATE = '2026-10-21'
def cond(build, extra=''):
    opt, fp, fm = build['opt'], build['fp'], build['fm']
    fpt = 'hard float (fpv5-sp-d16)' if fp == 'hard' else 'soft float on the same core, no FPU instruction in the codec or the bench'
    fmt = 'upstream -ffast-math' if fm == 'fast' else '-ffp-contract=off, no fast-math'
    return ('liblc3 v1.1.3 unmodified; TI Arm Clang 5.1.1.LTS -%s, %s, %s%s; mono, 10 ms frames, 100 frames per configuration; DWT cycles around each lc3_encode and lc3_decode call, signal generation and CRC excluded; code and data in flash' % (opt.upper() if opt != 'Os' else 'Os', fpt, fmt, extra))
builds = [
 ('O2-hard-strict', 'boards/lc3-O2-hard-strict.json', dict(opt='O2', fp='hard', fm='strict'), ''),
 ('O2-hard-fast', 'boards/hard-fast.json', dict(opt='O2', fp='hard', fm='fast'), ''),
 ('O2-soft-strict', 'boards/soft-strict.json', dict(opt='O2', fp='soft', fm='strict'), ''),
 ('O2-soft-fast', 'boards/soft-fast.json', dict(opt='O2', fp='soft', fm='fast'), ''),
 ('O3-hard-strict', 'boards/lc3-O3-hard-strict.json', dict(opt='O3', fp='hard', fm='strict'), ''),
 ('Os-hard-strict', 'boards/lc3-Os-hard-strict.json', dict(opt='Os', fp='hard', fm='strict'), ''),
 ('O2-hard-strict-armpaths', 'boards/lc3-O2-hard-strict-armpaths.json', dict(opt='O2', fp='hard', fm='strict'), '; compiled with -U__GNUC__ -D__GNUC__=10 so the library takes its Arm DSP paths (ltpf_arm.h, ssat)'),
 ('O2-hard-strict-noplus', 'boards/lc3-O2-hard-strict-noplus.json', dict(opt='O2', fp='hard', fm='strict'), '; -DLC3_PLUS=0 (LC3plus code and tables left out)'),
 ('O2-hard-strict-minimal', 'boards/lc3-O2-hard-strict-minimal.json', dict(opt='O2', fp='hard', fm='strict'), '; -DLC3_PLUS=0 -DLC3_PLUS_HR=0 (LC3 only)'),
 ('Os-hard-strict-minimal', 'boards/lc3-Os-hard-strict-minimal.json', dict(opt='Os', fp='hard', fm='strict'), '; -DLC3_PLUS=0 -DLC3_PLUS_HR=0 (LC3 only)'),
]
for name, jf, b, extra in builds:
    j = json.load(open(os.path.join(H, jf))); src = 'ports/lc3/results/' + jf
    for c in j['cfg']:
        cfg = '%dk-%dkbps' % (c['hz'] // 1000, c['kbps']); v = '%s-%s' % (cfg, name.lower())
        for test, val, unit in (('lc3_encode_cycles', c['enc_mean'], 'mean cycles per 10 ms frame'), ('lc3_decode_cycles', c['dec_mean'], 'mean cycles per 10 ms frame'),
                                ('lc3_encode_pct_rt', c['enc_pct'], 'percent of one 10 ms frame at 160 MHz'), ('lc3_decode_pct_rt', c['dec_pct'], 'percent of one 10 ms frame at 160 MHz')):
            note = unit + '; worst frame %d cycles' % (c['enc_max'] if 'encode' in test else c['dec_max']) if 'cycles' in test else unit
            w.writerow(['mspm33c321a', test, v, 'measured', round(val, 1) if 'pct' in test else round(val), '', 1, 160, cond(b, extra), DATE, src, 'readback', note])
        for k in ('strict', 'fast'):
            r = c['vs_' + k]
            if (b['fm'] == k) or k == 'strict':
                w.writerow(['mspm33c321a', 'lc3_frames_differ', v + '-vs-mac-' + k, 'measured', max(r['enc_frames_differ'], r['dec_frames_differ']), '', 1, 160, cond(b, extra), DATE, src, 'crc',
                            'frames of 100 whose encoded bytes or decoded PCM CRC-32 differs from the Mac %s build (encoder %d, decoder %d); 0 is bit-exact' % (k, r['enc_frames_differ'], r['dec_frames_differ'])])
    s = sizes[name]
    w.writerow(['mspm33c321a', 'lc3_flash_bytes', name.lower(), 'measured', s['flash'], '', 1, '', cond(b, extra).split(';')[0] + '; whole bench image (codec, tables, math library, SysConfig clock setup, test harness)', DATE, 'ports/lc3/results/sizes.json', 'linker', 'tiarmsize text plus data; codec code %d bytes, constant tables %d bytes' % (s['code'], s['rodata'])])
    w.writerow(['mspm33c321a', 'lc3_stack_peak_bytes', name.lower(), 'measured', s['stack_peak'], '', 1, 160, cond(b, extra).split(';')[0] + '; stack painted with 0xDEADBEEF before the run, 16384 bytes reserved', DATE, src, 'readback', 'deepest stack use over the three configurations'])
# bit-exact sample error of the 48 kHz decoded PCM
for name, jf in (('o2-hard-strict', 'pcm-hard-strict.json'), ('o2-hard-fast', 'pcm-hard-fast.json'), ('o2-soft-strict', 'pcm-soft-strict.json'), ('o2-soft-fast', 'pcm-soft-fast.json')):
    j = json.load(open(os.path.join(H, 'boards', jf)))
    for k in ('strict', 'fast'):
        r = j['pcm48k_vs_' + k]
        if (k in name) or k == 'strict' and False: pass
        if k in name:
            w.writerow(['mspm33c321a', 'lc3_max_sample_error', '48k-96kbps-%s-vs-mac-%s' % (name, k), 'measured', r['max_error'], '', 1, 160, 'liblc3 v1.1.3 unmodified; TI Arm Clang 5.1.1.LTS %s; decoded PCM of the 48 kHz configuration, 48,000 int16 samples read back over the probe' % name, DATE, 'ports/lc3/results/boards/' + jf, 'readback', 'largest sample difference in LSB against the Mac %s build; %d of 48000 samples differ' % (k, r['differ'])])
# M0+ model
for m in (1, 32):
    j = json.load(open(os.path.join(H, 'emu-g3519-o2-mul%d.json' % m)))
    for c in j['cfg']:
        cfg = '%dk-%dkbps' % (c['hz'] // 1000, c['kbps'])
        for test, val in (('lc3_encode_cycles', c['enc_mean']), ('lc3_decode_cycles', c['dec_mean'])):
            w.writerow(['mspm0g3519', test, '%s-m0plus-model-mul%d' % (cfg, m), 'computed', round(val), '', 1, '', 'MODEL, not an M0+ run: arm-none-eabi-gcc 14.3.1 -O2 -mcpu=cortex-m0plus -msoft-float, liblc3 v1.1.3 unmodified, run in the Unicorn emulator with Cortex-M0+ cycle prices (multiply %d cycle%s; no flash wait states or bus stalls, so a floor); 10 frames per configuration; mono, 10 ms frames' % (m, 's' if m > 1 else ''), DATE, 'ports/lc3/results/emu-g3519-o2-mul%d.json' % m, 'crc', 'mean modelled cycles per 10 ms frame; emulator bitstream and PCM CRC-32 equal the Mac strict build for all 10 frames'])
