#!/usr/bin/env python3
"""Make RESULTS-ROWS.csv from results/boards/*.json (board read-backs). Run from ports/plaits: python3 tools/make_rows.py"""
import csv, json, os, statistics
H = ['board', 'test', 'variant', 'status', 'value', 'value_hi', 'runs', 'clock_mhz', 'conditions', 'date', 'source', 'checked', 'note']
B = 'results/boards/'
cond = ('Plaits (pichenettes/eurorack, MIT) stmlib e3bd7c9 / eurorack f4cfa76, unmodified; TI Arm Clang 5.1.1.LTS %s, %s float, -ffp-contract=off, no fast-math; '
        'Voice::Render, 12-sample blocks at 48 kHz, 256 blocks per engine (4 notes, trigger patched, LPG on); DWT cycles around Render only; CRC-32 outside the timing; code and tables in flash')
rows = []
def add(test, variant, value, build, note, src, checked='readback', hi='', status='measured'):
    rows.append(['mspm33c321a', test, variant, status, value, hi, 1, 160, cond % build, '2026-10-21', src, checked, note])
fl = {'O2-hard': (207040, 115664, 90896), 'O3-hard': (216784, 125392, 90896), 'Os-hard': (168128, 76704, 90944), 'O2-soft': (241600, 150224, 90896)}
J = {'O2-hard': 'plaits-O2-hard-strict', 'O3-hard': 'plaits-O3-hard-strict', 'Os-hard': 'plaits-Os-hard-strict', 'O2-soft': 'plaits-O2-soft-strict'}
for t, name in J.items():
    d = json.load(open(B + name + '.json')); E = d['engines']
    opt, fp = t.split('-'); build = ('-' + opt, 'hard (fpv5-sp-d16)' if fp == 'hard' else 'soft (FPU not used)')
    src = B + name + '.json'
    if t == 'O2-hard':
        for e in E:
            v = '%s-o2-hard-strict' % e['name']
            add('plaits_cycles_per_block', v, round(e['mean_after_first']), build, 'mean over blocks 2 to 256, cycles for one 12-sample block; block 1 (engine load) kept apart', src)
            add('plaits_pct_rt', v, round(e['pct_mean'], 1), build, 'percent of the 40,000 cycles a 12-sample block has at 160 MHz; worst block %.1f percent' % e['pct_worst'], src)
            add('plaits_worst_block_cycles', v, e['max'], build, 'slowest block after the first (block %d)' % e['worst_block'], src)
            add('plaits_first_block_cycles', v, e['first'], build, 'block 1: engine reset and user-data load; %.0f percent of a block period' % e['pct_first'], src)
            add('plaits_blocks_differ', v, e['blocks_differ'], build, '256 blocks compared by CRC-32 with the Mac strict build; 0 is bit-exact', src, checked='crc')
    m = [e['pct_mean'] for e in E]
    v = '%s-%s-strict' % (opt.lower(), fp)
    add('plaits_engines_bitexact', v, sum(1 for e in E if e['crc_equal']), build, 'engines of 24 whose CRC-32 of all 256 blocks equals the Mac strict build', src, checked='crc')
    add('plaits_engines_realtime', v, sum(1 for e in E if e['pct_worst'] < 100), build,
        'engines of 24 whose slowest block after the first stays under 100 percent of real time (speech fails only at its three note-on blocks: 166, 263 and 621 percent)', src)
    add('plaits_pct_rt_median', v, round(statistics.median(m), 1), build, 'median over 24 engines of the mean percent of real time (min %.1f, max %.1f)' % (min(m), max(m)), src, hi=round(max(m), 1))
    add('plaits_flash_bytes', v, fl[t][0], build, 'whole test image: %d code, %d constant tables (resources.cc), 192 init' % (fl[t][1], fl[t][2]), 'ports/plaits/results/size.txt', checked='linker')
    if t == 'O2-hard':
        add('plaits_ram_bytes', v, 96068, build, 'bss 77,832 + data 10,044 + stack 8,192; of which Voice object 8,868 and shared buffer 16,384, the rest is the harness arrays', 'ports/plaits/results/size.txt', checked='linker')
        add('plaits_stack_peak_bytes', v, 8192 - d['stack_free_bytes'], build, 'stack painted before the run, 8,192 bytes reserved', src)
d = json.load(open(B + 'plaits-O2-hard-contract-on.json'))
add('plaits_engines_bitexact', 'o2-hard-contract-on', sum(1 for e in d['engines'] if e['crc_equal']), ('-O2', 'hard (fpv5-sp-d16), -ffp-contract=on'), 'engines of 24 equal to the Mac strict build; the compiler emits no fused multiply-add either way', B + 'plaits-O2-hard-contract-on.json', checked='crc')
k = json.load(open(B + 'plaits-O2-hard-strict-pcm-speech.json'))['keep']
add('plaits_pcm_samples_differ', 'speech-o2-hard-strict', k['differ'], ('-O2', 'hard (fpv5-sp-d16)'), 'of %d rendered 16-bit samples (256 blocks, stereo); maximum error %d' % (k['samples'], k['max_error']), B + 'plaits-O2-hard-strict-pcm-speech.json', checked='readback')
with open('RESULTS-ROWS.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(H); w.writerows(rows)
print(len(rows), 'rows')
