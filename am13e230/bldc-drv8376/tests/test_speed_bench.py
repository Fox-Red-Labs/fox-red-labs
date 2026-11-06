"""Speed bench checks use synthetic telemetry, never a serial device."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import speed_bench as bench
from sweep import BenchError


def trace(target=1000, initial=0, stuck=False, cap=40):
    lines = [f'trace target={target} mode=6step dir=f count=401']
    for ms in range(0, 4001, 10):
        rpm = initial + (target - initial) * min(ms, 1000) // 1000
        lines.append(f'sample us={ms*1000} rpm={rpm} duty={cap if stuck else 20} edges={0 if stuck else ms//10*6}')
    return lines + ['trace end']


class MetricsTests(unittest.TestCase):
    def test_up_and_down_steps_use_actual_timestamps(self):
        for target, initial in [(1000, 0), (500, 1500)]:
            result = bench.analyze_trace(trace(target, initial), target, '6step', 'f', 40)
            self.assertEqual(result['rise_10_90_ms'], 800)
            self.assertEqual(result['overshoot_pct_of_step'], 0)
            self.assertEqual(result['steady_rpm'], target)
            self.assertIsNotNone(result['settling_5pct_ms'])

    def test_sinusoidal_duty_runs_cannot_be_mislabeled(self):
        from sweep import validate_run
        from test_sweep import status
        lines = [status(edges=100).replace('6step', 'sine'), status(edges=300).replace('6step', 'sine')]
        self.assertEqual(validate_run(lines, 20, 'f', 'sine')['mode'], 'sine')
        with self.assertRaises(BenchError): validate_run(lines, 20, 'f', '6step')

    def test_missing_or_truncated_trace_is_rejected(self):
        for lines in [trace()[:-1], trace()[:40] + ['trace end']]:
            with self.assertRaises(BenchError):
                bench.analyze_trace(lines, 1000, '6step', 'f', 40)

    def test_stale_rpm_without_rotation_rejected(self):
        with self.assertRaises(BenchError):
            bench.analyze_trace(trace(stuck=True), 1000, '6step', 'f', 40)

    def test_unreachable_target_is_not_reported_settled(self):
        lines = trace(target=500)
        lines[0] = lines[0].replace('target=500', 'target=1000')
        lines = [s.replace('duty=20', 'duty=40') for s in lines]
        result = bench.analyze_trace(lines, 1000, '6step', 'f', 40)
        self.assertIsNone(result['settling_5pct_ms'])
        self.assertIsNone(result['rise_10_90_ms'])
        self.assertEqual(result['saturation_fraction'], 1)
        self.assertFalse(result['target_reached'])

    def test_wrong_mode_target_or_direction_rejected(self):
        for target, mode, direction in [(500, '6step', 'f'), (1000, 'sine', 'f'), (1000, '6step', 'r')]:
            with self.assertRaises(BenchError):
                bench.analyze_trace(trace(), target, mode, direction, 40)

    def test_nonmonotonic_time_and_sparse_samples_rejected(self):
        for replacement in ['sample us=0 rpm=500 duty=20 edges=30', 'sample us=1000000 rpm=500 duty=20 edges=30']:
            lines = trace(); lines[5] = replacement
            with self.assertRaises(BenchError):
                bench.analyze_trace(lines, 1000, '6step', 'f', 40)

    def test_settling_requires_half_second_of_evidence(self):
        lines = trace()
        lines[-3] = lines[-3].replace('rpm=1000', 'rpm=1200')
        result = bench.analyze_trace(lines, 1000, '6step', 'f', 40)
        self.assertIsNone(result['settling_5pct_ms'])

    def test_successful_capture_stops_before_dump_and_returns_metrics(self):
        from test_sweep import status
        class GoodSession:
            def __init__(self): self.commands = []
            def exchange(self, keys, seconds, on_line=None):
                self.commands.append(keys)
                if keys == 'g':
                    rows = [status(rpm=1000, edges=e)+' speed_mode=1 target_rpm=1000' for e in (100, 500)]
                    for line in rows: on_line(line)
                    return rows
                if keys == 'svF':
                    return [status('idle', duty=0), 'PA22 1 PB4 1; PB4 drives fault shutdown']
                if keys == 'E': return trace()
                raise AssertionError(keys)
        session = GoodSession()
        result = bench.capture_step(session, 1000, '6step', 'f', 40, start=True)
        self.assertTrue(result['target_reached'])
        self.assertEqual(session.commands, ['g', 'svF', 'E'])

    def test_fault_during_speed_step_stops_and_does_not_dump_running(self):
        class FaultSession:
            commands = []
            def exchange(self, keys, seconds, on_line=None):
                self.commands.append(keys)
                if keys == '2': on_line('DRIVER FAULT (PB4 nFAULT low): outputs off.')
                return []
        session = FaultSession()
        with self.assertRaises(BenchError):
            bench.capture_step(session, 1000, '6step', 'f', 40, start=False)
        self.assertEqual(session.commands[-1], 'svF')
        self.assertNotIn('E', session.commands)

if __name__ == '__main__': unittest.main()
