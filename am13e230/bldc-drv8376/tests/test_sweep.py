"""Host-only regressions. No serial device is opened."""
import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'sweep.py'


def status(state='run', duty=20, rpm=900, edges=100, fault=0, target=20):
    return f'{state} 6step duty={duty} set={target} fwd lead=0 hall=3 rpm={rpm} edges={edges} hall_changes={edges} vm_mV=12400 fault={fault}'


class SweepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if MODULE.exists():
            spec = importlib.util.spec_from_file_location('sweep', MODULE)
            cls.sweep = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.sweep)

    def setUp(self):
        self.assertTrue(MODULE.exists(), 'validated sweep runner has not been implemented')

    def test_fault_after_good_speed_invalidates_run(self):
        with self.assertRaises(self.sweep.BenchError):
            self.sweep.validate_run([status(), status(edges=200), 'DRIVER FAULT (PB4 nFAULT low): outputs off.'], 20, 'f')

    def test_stall_after_good_speed_invalidates_run(self):
        with self.assertRaises(self.sweep.BenchError):
            self.sweep.validate_run([status(), status(edges=200), 'no Hall edge for 1.5 s: stopped'], 20, 'f')

    def test_cap_cannot_be_mislabeled(self):
        with self.assertRaises(self.sweep.BenchError):
            self.sweep.validate_run([status(duty=40, target=40), status(duty=40, target=40, edges=300)], 60, 'f')

    def test_one_old_positive_speed_sample_is_not_success(self):
        for samples in ([status()], [status(), status(rpm=0, edges=100)], [status(), status(edges=101)]):
            with self.subTest(samples=samples), self.assertRaises(self.sweep.BenchError):
                self.sweep.validate_run(samples, 20, 'f')

    def test_clean_progress_at_requested_duty(self):
        result = self.sweep.validate_run([status(duty=1, rpm=0, edges=0), status(), status(edges=300)], 20, 'f')
        self.assertEqual(result['rpm'], 900)

    def test_old_fault_input_and_unlearned_table_rejected(self):
        good = ['cap 40%  pole pairs 4  mode 0  learned 1', status('idle', duty=0, rpm=0, edges=0),
                'fault inputs raw (0=low): PA22 1 PB4 1; PB4 drives fault shutdown']
        self.sweep.validate_preflight(good, 40)
        for bad in (good[:-1], [x.replace('learned 1', 'learned 0') for x in good],
                    [x.replace('PB4 1', 'PB4 0') for x in good]):
            with self.subTest(bad=bad), self.assertRaises(self.sweep.BenchError):
                self.sweep.validate_preflight(bad, 40)

    def test_fault_during_run_sends_stop_and_no_restart(self):
        sweep = self.sweep
        class FaultSession:
            def __init__(self): self.commands = []
            def exchange(self, keys, seconds, on_line=None):
                self.commands.append(keys)
                if keys == 'g':
                    on_line('DRIVER FAULT (PB4 nFAULT low): outputs off.')
                return [status('idle', duty=0, rpm=0)]
        session = FaultSession()
        with self.assertRaises(sweep.BenchError):
            sweep.run_point(session, 20, 'f', 4.5)
        self.assertEqual(session.commands[-1], 'svF')
        self.assertEqual(session.commands.count('g'), 1)
        self.assertNotIn('c', session.commands)

    def test_keyboard_interrupt_sends_stop(self):
        sweep = self.sweep
        class InterruptedSession:
            def __init__(self): self.commands = []
            def exchange(self, keys, seconds, on_line=None):
                self.commands.append(keys)
                if keys == 'g': raise KeyboardInterrupt
                return [status('idle', duty=0, rpm=0)]
        session = InterruptedSession()
        with self.assertRaises(KeyboardInterrupt): sweep.run_point(session, 20, 'f', 4.5)
        self.assertEqual(session.commands[-1], 'svF')

    def test_coast_uses_all_hall_changes_even_with_zero_rpm(self):
        class CoastSession:
            def __init__(self): self.calls = 0
            def exchange(self, keys, seconds, on_line=None):
                self.calls += 1
                # No complete sectors, zero displayed rpm, but still rocking.
                return [status('idle', duty=0, rpm=0, edges=0).replace('hall_changes=0', f'hall_changes={self.calls}')]
        session = CoastSession()
        with self.assertRaises(self.sweep.BenchError): self.sweep.wait_coast(session)
        self.assertEqual(session.calls, 8)

    def test_coast_stops_waiting_when_hall_changes_stop(self):
        class CoastSession:
            def __init__(self): self.calls = 0
            def exchange(self, keys, seconds, on_line=None):
                self.calls += 1
                return [status('idle', duty=0, rpm=0, edges=min(self.calls, 3))]
        session = CoastSession()
        self.sweep.wait_coast(session)
        self.assertEqual(session.calls, 4)

if __name__ == '__main__':
    unittest.main()
