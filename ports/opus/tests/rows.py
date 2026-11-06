#!/usr/bin/env python3
"""Write ports/opus/RESULTS-ROWS.csv (results-store rows) from results/boards/*.json and results/sizes.json. Run from the repository root."""
import csv, json, os
R = os.path.join(os.path.dirname(__file__), '..')
sizes = json.load(open(os.path.join(R, 'results/sizes.json')))
cols = 'board,test,variant,status,value,value_hi,runs,clock_mhz,conditions,date,source,checked,note'.split(',')
rows = []
# build id -> (ref name, build description)
BUILDS = {
 'opus-fixed-O2': ('Mac int32 macros', 'fixed-point, -O2, libopus multiply macros for a 32-bit core (OPUS_FAST_INT64 0, the default here)'),
 'opus-fixed-O2-i64_1': ('Mac int64 macros', 'fixed-point, -O2, OPUS_FAST_INT64 forced to 1 (64-bit multiply macros)'),
 'opus-fixed-O2-edsp': ('Mac int32 macros', 'fixed-point, -O2, -DOPUS_ARM_INLINE_EDSP (library inline-assembly multiplies)'),
 'opus-fixed-O3': ('Mac int32 macros', 'fixed-point, -O3'),
 'opus-fixed-Os': ('Mac int32 macros', 'fixed-point, -Os'),
 'opus-float-O2': ('Mac float', 'floating-point, -O2'),
 'opus-float-Os': ('Mac float', 'floating-point, -Os'),
}
def vid(b):
    k = b.replace('opus-', '').lower().replace('_', '')
    return k  # fixed-o2, fixed-o2-i641, ...
def cfgname(r):
    mode = 'silk' if r['silk'] else 'celt'
    return '%s-%dk-%dms-%dkbps-c%d' % (mode, r['hz'] // 1000, r['frame_us'] // 1000, r['kbps'], r['complexity'])
def add(test, var, val, cond, checked, note, src, mhz=160):
    rows.append(dict(board='mspm33c321a', test=test, variant=var, status='measured', value=val, value_hi='', runs=1, clock_mhz=mhz, conditions=cond, date='2026-10-21', source=src, checked=checked, note=note))
for b, (refname, desc) in BUILDS.items():
    p = os.path.join(R, 'results/boards/%s.json' % b)
    if not os.path.exists(p): continue
    d = json.load(open(p)); src = 'ports/opus/results/boards/%s.json' % b
    for r in d['cfg']:
        v = vid(b) + '-' + cfgname(r)
        cond = ('libopus 1.5.2 unmodified; TI Arm Clang 5.1.1.LTS, %s, hard float (fpv5-sp-d16), -ffp-contract=off; mono, %.0f ms frames, %d kbps constant bit rate, complexity %d, %s; 1 s of audio (%d frames); '
                'DWT cycles around each opus_encode and opus_decode call, signal generation and CRC excluded; code and data in flash') % (
                desc, r['frame_us'] / 1000, r['kbps'], r['complexity'], {'voip': 'VoIP, voice signal, wideband', 'audio': 'audio, music signal, full band', 'lowdelay': 'restricted low delay, music signal, wideband'}[r['app']], r['frames'])
        fc = 160e6 * r['frame_us'] / 1e6
        add('opus_encode_cycles', v, round(r['enc_mean']), cond, 'readback', 'mean cycles per frame; worst frame %d cycles' % r['enc_max'], src)
        add('opus_decode_cycles', v, round(r['dec_mean']), cond, 'readback', 'mean cycles per frame; worst frame %d cycles' % r['dec_max'], src)
        add('opus_encode_pct_rt', v, round(r['enc_pct'], 1), cond, 'readback', 'percent of one %.0f ms frame at 160 MHz' % (r['frame_us'] / 1000), src)
        add('opus_decode_pct_rt', v, round(r['dec_pct'], 1), cond, 'readback', 'percent of one %.0f ms frame at 160 MHz' % (r['frame_us'] / 1000), src)
        add('opus_frames_differ', v + '-vs-' + refname.lower().replace(' ', '-'), r['dec_frames_differ'], cond, 'crc',
            'frames whose decoded PCM CRC-32 differs from the %s build (%d of %d packets differ; 0 is bit-exact)' % (refname, r['enc_frames_differ'], r['frames']), src)
    s = sizes[b]
    add('opus_flash_bytes', vid(b), s['text'] + s['data'], 'libopus 1.5.2 unmodified; whole bench image (codec, tables, math library, SysConfig clock setup, harness); %s' % desc, 'linker',
        'tiarmsize text plus data; code %d bytes, constants %d bytes' % (s['secs']['.text'], s['secs']['.rodata']), 'ports/opus/results/sizes.json', mhz='')
    add('opus_stack_peak_bytes', vid(b), 65536 - d['stack_free_bytes'], 'libopus 1.5.2 unmodified; stack painted with 0xDEADBEEF before the run, 65536 bytes reserved; %s' % desc, 'readback',
        'deepest stack use over the 12 runs (VLA stack allocation, encoder at complexity 10)', src)
st = json.load(open(os.path.join(R, 'results/boards/opus-fixed-O2.json')))['cfg'][0]; fl = json.load(open(os.path.join(R, 'results/boards/opus-float-O2.json')))['cfg'][0]
for kind, r in (('fixed', st), ('float', fl)):
    add('opus_encoder_state_bytes', kind, r['enc_state'], 'libopus 1.5.2; opus_encoder_get_size(1), mono; same for every configuration', 'readback', 'state the application supplies; read from the board', 'ports/opus/results/boards/opus-%s-O2.json' % kind, mhz='')
    add('opus_decoder_state_bytes', kind, r['dec_state'], 'libopus 1.5.2; opus_decoder_get_size(1), mono; same for every configuration', 'readback', 'state the application supplies; read from the board', 'ports/opus/results/boards/opus-%s-O2.json' % kind, mhz='')
w = csv.DictWriter(open(os.path.join(R, 'RESULTS-ROWS.csv'), 'w', newline=''), fieldnames=cols, lineterminator='\n'); w.writeheader(); w.writerows(rows); print(len(rows), 'rows')
