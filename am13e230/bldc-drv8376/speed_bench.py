#!/usr/bin/env python3
"""Bounded Hall PI speed-step comparisons. Run only after the hardware baseline passes."""
import argparse
import datetime
import json
from pathlib import Path
import re
import signal
from statistics import mean
from sweep import (BenchError, SerialSession, parse_status, reject_fault,
                   validate_preflight, validate_pre_stop, wait_coast)


def analyze_trace(lines, target, mode, direction, cap):
    headers = [re.fullmatch(r'trace target=(\d+) mode=(6step|sine) dir=([fr]) count=(\d+)', s) for s in lines]
    headers = [h for h in headers if h]
    if len(headers) != 1 or headers[0].groups()[:3] != (str(target), mode, direction) or 'trace end' not in lines:
        raise BenchError('Missing trace or incorrect target/mode/direction')
    rows = []
    for line in lines:
        reject_fault(line)
        if line.startswith('sample '):
            m = re.fullmatch(r'sample us=(\d+) rpm=(\d+) duty=(\d+) edges=(\d+)', line)
            if not m: raise BenchError('Malformed trace sample')
            rows.append(tuple(map(int, m.groups())))
    if len(rows) != int(headers[0][4]) or len(rows) < 2 or rows[0][0] != 0 or rows[-1][0] < 3_800_000:
        raise BenchError('Incomplete speed trace; need at least 3.8 seconds')
    for prev, row in zip(rows, rows[1:]):
        if not 0 < row[0] - prev[0] <= 50_000 or row[3] < prev[3]:
            raise BenchError('Invalid sample timing or Hall progression')
    if any(r[2] > cap for r in rows): raise BenchError('Duty exceeded cap')
    tail = [r for r in rows if r[0] >= rows[-1][0] - 500_000]
    if tail[-1][3] - tail[0][3] < 24 or any(r[1] <= 0 for r in tail):
        raise BenchError('No sustained mechanical rotation in final half second')
    initial = rows[0][1]
    delta = target - initial
    if abs(delta) < 50: raise BenchError('Speed step too small to characterize')
    fractions = [(r[1] - initial) / delta for r in rows]
    crossings = [next((rows[i][0] for i, f in enumerate(fractions) if f >= x), None) for x in (.1, .9)]
    rise = (crossings[1] - crossings[0]) / 1000 if None not in crossings else None
    band = max(25, target * .05)
    last_out = max((i for i, r in enumerate(rows) if abs(r[1] - target) > band), default=-1)
    settled_index = last_out + 1
    settling = None
    if settled_index < len(rows) and rows[-1][0] - rows[settled_index][0] >= 500_000:
        settling = rows[settled_index][0] / 1000
    steady = mean(r[1] for r in tail)
    return dict(initial_rpm=initial, target_rpm=target, mode=mode, direction=direction,
                duration_ms=rows[-1][0]/1000, rise_10_90_ms=rise,
                overshoot_pct_of_step=max(0, max(fractions)-1)*100,
                settling_5pct_ms=settling, steady_rpm=steady,
                steady_error_rpm=steady-target, target_reached=settling is not None,
                saturation_fraction=mean(r[2] == cap for r in rows),
                mean_tail_duty=mean(r[2] for r in tail), samples=len(rows))


def validate_speed_run(lines, target, mode, direction, cap):
    for line in lines: reject_fault(line)
    rows = [r for line in lines if (r := parse_status(line))]
    if len(rows) < 2: raise BenchError('Missing sustained speed telemetry')
    for row in rows[-2:]:
        if (row['state'] != 'run' or row['mode'] != mode or row['direction'] != direction
            or row.get('speed_mode') != 1 or row.get('target_rpm') != target
            or not 0 <= row['duty'] <= cap or row['rpm'] <= 0):
            raise BenchError('Speed configuration or rotation not sustained')
    if rows[-1]['edges'] - rows[-2]['edges'] < 24: raise BenchError('Insufficient Hall progression')
    return rows[-1]


def capture_step(session, target, mode, direction, cap, start):
    try:
        lines = session.exchange('g' if start else str(target//500), 4.3, on_line=reject_fault)
        validate_speed_run(lines, target, mode, direction, cap)
    finally:
        stopped = session.exchange('svF', .8)
    validate_pre_stop(stopped)
    # Dump only after stopping: a long UART print must not block commutation.
    return analyze_trace(session.exchange('E', 3), target, mode, direction, cap)


def run_trial(session, initial, target, mode, direction, cap):
    try:
        wait_coast(session)
        validate_preflight(session.exchange('s?F', 1), cap)
        preset = initial or target
        session.exchange(('m' if mode == 'sine' else 'n')+'p'+direction+str(preset//500), .8)
        lines = session.exchange('v', .35)
        for line in lines: reject_fault(line)
        rows = [r for line in lines if (r := parse_status(line))]
        if (not rows or rows[-1]['state'] != 'idle' or rows[-1]['duty'] != 0
            or rows[-1].get('speed_mode') != 1 or rows[-1].get('target_rpm') != preset
            or rows[-1]['mode'] != mode or rows[-1]['direction'] != direction):
            raise BenchError('Speed preset not acknowledged before start')
        if initial:
            baseline = validate_speed_run(session.exchange('g', 4.3, on_line=reject_fault), initial, mode, direction, cap)
            if abs(baseline['rpm']-initial) > initial*.1:
                raise BenchError('Initial speed not reached; no step issued')
        return capture_step(session, target, mode, direction, cap, start=not initial)
    finally:
        session.exchange('svF', .8)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', default='/dev/cu.usbmodemAL1300011')
    p.add_argument('--check-only', action='store_true')
    p.add_argument('--modes', nargs='+', choices=['6step', 'sine'], default=['6step','sine'])
    p.add_argument('--directions', nargs='+', choices=['f','r'], default=['f','r'])
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    if args.repeats < 1: p.error('repeats must be positive')
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    output = args.output or Path(__file__).parent/'results'/f'speed-bench-{stamp}.txt'
    output.parent.mkdir(parents=True, exist_ok=True)
    def interrupt(signum, frame): raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupt)
    session = None
    with output.open('x') as log:
        try:
            session = SerialSession(args.port, log)
            session.record('Hall PI speed bench; physical current limit must be independently established')
            lines = session.exchange('s?F', 1)
            validate_preflight(lines, 40)
            rows = [r for line in lines if (r := parse_status(line))]
            if 'target_rpm' not in rows[-1]: raise BenchError('Need timestamped-trace firmware')
            if args.check_only:
                session.record('PREFLIGHT PASS; no motor start requested')
                return 0
            for mode in args.modes:
                for direction in args.directions:
                    for initial,target in [(0,500),(500,1000),(1000,500)]:
                        for repeat in range(1,args.repeats+1):
                            session.record(f'BEGIN mode={mode} dir={direction} step={initial}:{target} repeat={repeat}')
                            result = run_trial(session, initial, target, mode, direction, 40)
                            session.record('MEASURED '+json.dumps(dict(result,repeat=repeat)))
                            if not result['target_reached']:
                                raise BenchError('Target did not settle; trace retained, later trials skipped')
            session.record('SPEED BENCH COMPLETE')
            return 0
        except (BenchError, OSError, KeyboardInterrupt) as exc:
            session.record('ABORT '+(str(exc) or 'interrupted')) if session else print(exc)
            return 1
        finally:
            if session: session.close()

if __name__ == '__main__': raise SystemExit(main())
