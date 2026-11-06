#!/usr/bin/env python3
"""Bounded six-step/sinusoidal duty sweep. Never clears faults or changes the Hall table."""
import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import re
import select
import signal
import termios
import time
import tty


class BenchError(RuntimeError):
    pass


def parse_status(line):
    match = re.match(r'^(run|idle) (6step|sine) ', line)
    if not match:
        return None
    row = {k: int(v) for k, v in re.findall(r'(\w+)=\s*(-?\d+)', line)}
    required = {'duty', 'set', 'hall', 'rpm', 'edges', 'hall_changes', 'vm_mV', 'fault'}
    if not required <= row.keys():
        raise BenchError('Incomplete status: ' + line)
    row.update(state=match[1], mode=match[2], direction='r' if ' rev ' in line else 'f')
    return row


def reject_fault(line):
    if any(text in line.lower() for text in ('driver fault', 'no hall edge', 'learn first', 'clear the fault', 'fault still present')):
        raise BenchError(line)
    row = parse_status(line)
    if row and (row['fault'] or row['hall'] not in range(1, 7)):
        raise BenchError('Fault or invalid Hall state: ' + line)


def validate_preflight(lines, maximum):
    for line in lines:
        reject_fault(line)
    text = '\n'.join(lines)
    config = re.search(r'cap (\d+)%\s+pole pairs (\d+)\s+mode (\d+)\s+learned (\d+)', text)
    if not config or config.groups()[1:] != ('4', '0', '1'):
        raise BenchError('Need 6x PWM, four pole pairs and an already validated Hall table')
    if maximum > int(config[1]):
        raise BenchError('Requested duty exceeds the reported firmware cap')
    if not re.search(r'PA22 [01] PB4 1; PB4 drives fault shutdown', text):
        raise BenchError('Missing healthy PB4 fault input; old PA22-only firmware is unsupported')
    statuses = [row for line in lines if (row := parse_status(line))]
    if not statuses or statuses[-1]['state'] != 'idle' or statuses[-1]['duty'] != 0:
        raise BenchError('No stopped-state acknowledgement')


def validate_run(lines, duty, direction, mode='6step'):
    for line in lines:
        reject_fault(line)
    rows = [row for line in lines if (row := parse_status(line))]
    if len(rows) < 2:
        raise BenchError('Missing sustained-run telemetry')
    tail = rows[-2:]
    for row in tail:
        if (row['state'] != 'run' or row['mode'] != mode or row['direction'] != direction
                or row['duty'] != duty or row['set'] != duty or row['rpm'] <= 0):
            raise BenchError('Run did not sustain the requested duty and rotation')
    if tail[1]['edges'] - tail[0]['edges'] < 24:
        raise BenchError('Less than one mechanical turn of Hall-edge progression between final samples')
    return tail[-1]


class SerialSession:
    def __init__(self, port, log):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.log = log
        self.pending = b''
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            tty.setraw(self.fd)
            settings = termios.tcgetattr(self.fd)
            settings[4] = settings[5] = termios.B115200
            termios.tcsetattr(self.fd, termios.TCSANOW, settings)
            termios.tcflush(self.fd, termios.TCIFLUSH)
        except BaseException:
            os.close(self.fd)
            raise

    def record(self, text):
        self.log.write(f'{time.monotonic():.6f} {text}\n')
        self.log.flush()
        print(text, flush=True)

    def exchange(self, keys, seconds, on_line=None):
        rows = []
        start = next_key = time.monotonic()
        index = 0
        last_rx = start
        while time.monotonic() - start < seconds:
            now = time.monotonic()
            if index < len(keys) and now >= next_key:
                os.write(self.fd, keys[index].encode('ascii'))
                self.record('TX ' + keys[index])
                index += 1
                next_key = now + 0.12
            if select.select([self.fd], [], [], 0.05)[0]:
                chunk = os.read(self.fd, 4096)
                if not chunk:
                    raise BenchError('Serial device disconnected')
                last_rx = time.monotonic()
                self.pending += chunk
                while b'\n' in self.pending:
                    raw, self.pending = self.pending.split(b'\n', 1)
                    line = raw.decode('ascii', errors='replace').strip('\r')
                    self.record('RX ' + line)
                    rows.append(line)
                    if on_line:
                        on_line(line)
            if on_line and time.monotonic() - last_rx > 2.5:
                raise BenchError('Run telemetry silent for more than 2.5 seconds')
        if index != len(keys):
            raise BenchError('Command sequence did not finish')
        return rows

    def close(self):
        try:
            os.write(self.fd, b's')
            self.record('TX s (final cleanup; acknowledgement not assumed)')
        finally:
            os.close(self.fd)


def run_point(session, duty, direction, duration, mode='6step'):
    try:
        session.exchange(('m' if mode == 'sine' else 'n') + 'u' + direction + str(duty // 10), 0.8)
        configured = session.exchange('v', 0.35)
        for line in configured:
            reject_fault(line)
        rows = [r for line in configured if (r := parse_status(line))]
        if (not rows or rows[-1]['set'] != duty or rows[-1]['state'] != 'idle'
                or rows[-1]['mode'] != mode or rows[-1]['direction'] != direction
                or rows[-1]['duty'] != 0):
            raise BenchError('Requested duty was not acknowledged before start')
        lines = session.exchange('g', duration, on_line=reject_fault)
        result = validate_run(lines, duty, direction, mode)
    finally:
        # This executes on failed starts, telemetry loss, Ctrl-C and SIGTERM.
        stopped = session.exchange('svF', 0.8)
    validate_pre_stop(stopped)
    return result


def validate_pre_stop(lines):
    for line in lines:
        reject_fault(line)
    statuses = [r for line in lines if (r := parse_status(line))]
    if not statuses or statuses[-1]['state'] != 'idle' or statuses[-1]['duty']:
        raise BenchError('Stop acknowledgement missing; sweep aborted')
    if not any('PB4 1; PB4 drives fault shutdown' in line for line in lines):
        raise BenchError('Healthy fault line not confirmed after stop')


def wait_coast(session):
    """Stopped rpm is forcibly zero in firmware; use continuing Hall-edge counts."""
    previous = None
    for _ in range(8):
        lines = session.exchange('v', 1.1)
        for line in lines:
            reject_fault(line)
        rows = [r for line in lines if (r := parse_status(line))]
        if not rows or rows[-1]['state'] != 'idle' or rows[-1]['duty']:
            raise BenchError('Missing stopped-state telemetry during coast')
        edges = rows[-1]['hall_changes']
        if edges == previous:
            return
        previous = edges
    raise BenchError('Hall edges continued during coast; no next start')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/cu.usbmodemAL1300011')
    parser.add_argument('--duties', nargs='+', type=int, default=[10, 20, 30, 40])
    parser.add_argument('--modes', nargs='+', choices=['6step', 'sine'], default=['6step'])
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--check-only', action='store_true', help='Read preflight; never send go')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.repeats < 1 or any(d not in (10, 20, 30, 40) for d in args.duties):
        parser.error('Use positive repeats and duties from 10, 20, 30, 40')
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    output = args.output or Path(__file__).parent / 'results' / ('sweep-' + stamp + '.txt')
    output.parent.mkdir(parents=True, exist_ok=True)
    session = None
    def interrupt(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupt)
    with output.open('x') as log:
        try:
            session = SerialSession(args.port, log)
            session.record('Duty sweep: ' + json.dumps({'modes': args.modes, 'duties': args.duties, 'repeats': args.repeats, 'check_only': args.check_only}))
            preflight = session.exchange('sun?F', 1.5)
            validate_preflight(preflight, max(args.duties))
            if args.check_only:
                session.record('PREFLIGHT PASS; no motor start requested; hardware current limit is not verified by this check')
                return 0
            for mode in args.modes:
                for direction in ('f', 'r'):
                    for duty in args.duties:
                        for repeat in range(1, args.repeats + 1):
                            session.record(f'BEGIN mode={mode} direction={direction} duty={duty} repeat={repeat}')
                            validate_preflight(session.exchange('s?F', 1.0), max(args.duties))
                            row = run_point(session, duty, direction, 4.5, mode)
                            session.record('PASS ' + json.dumps(dict(row, repeat=repeat)))
                            wait_coast(session)
            session.record('SWEEP COMPLETE')
            return 0
        except (BenchError, OSError, KeyboardInterrupt) as exc:
            log.write('ABORT ' + (str(exc) or 'interrupted') + '\n')
            print('Sweep aborted: ' + (str(exc) or 'interrupted'), flush=True)
            return 1
        finally:
            if session:
                session.close()


if __name__ == '__main__':
    raise SystemExit(main())
