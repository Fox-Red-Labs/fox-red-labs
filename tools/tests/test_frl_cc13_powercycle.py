"""The CC1354P10 power-cycle guard (Codex review 2026-10-25, finding 7), no board, no hub.

A hub port is switched only for the refused connect (-242), only when uhubctl shows this probe's serial on that
port, and both uhubctl commands must succeed; a port left off is said, never hidden.
"""
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import ccs13 as script  # noqa: E402
from core import board_route  # noqa: E402

STATUS = """Current status for hub 20-2 [05e3:0610 GenesysLogic USB2.1 Hub, USB 2.10, 4 ports, ppps]
  Port 3: 0103 power enable connect [0451:bef3 Texas Instruments XDS110 (03.00.00.43) Embed with CMSIS-DAP MG000001]
  Port 4: 0103 power enable connect [0451:bef3 Texas Instruments XDS110 (03.00.00.43) Embed with CMSIS-DAP LS5600RE]
"""
REFUSED = "IcePick_C: Error connecting to the target: (Error -242 @ 0x0)"


class Hub:
    """A fake uhubctl: returns STATUS for a status call, and the given exit codes for off and on."""
    def __init__(self, status=STATUS, off=0, on=0):
        self.status, self.codes, self.calls = status, {"off": off, "on": on}, []

    def __call__(self, args):
        self.calls.append(args)
        if "-a" not in args:
            return 0, self.status
        return self.codes[args[args.index("-a") + 1]], ""


class Guard(unittest.TestCase):
    def test_only_the_refused_connect_cycles(self):
        hub = Hub()
        ok, why = script.power_cycle("Error -1170: the probe is busy", "20-2:4", "LS5600RE", hub, sleep=lambda s: None)
        self.assertFalse(ok)
        self.assertIn("-242", why)
        self.assertEqual(hub.calls, [])

    def test_the_probe_must_be_on_that_port(self):
        hub = Hub()
        ok, why = script.power_cycle(REFUSED, "20-2:3", "LS5600RE", hub, sleep=lambda s: None)
        self.assertFalse(ok)
        self.assertIn("MG000001", why)
        self.assertFalse(any("-a" in c for c in hub.calls))

    def test_a_port_not_listed_is_not_switched(self):
        hub = Hub(status="")
        ok, why = script.power_cycle(REFUSED, "20-2:4", "LS5600RE", hub, sleep=lambda s: None)
        self.assertFalse(ok)
        self.assertFalse(any("-a" in c for c in hub.calls))

    def test_a_failed_off_stops_before_on(self):
        hub = Hub(off=1)
        ok, why = script.power_cycle(REFUSED, "20-2:4", "LS5600RE", hub, sleep=lambda s: None)
        self.assertFalse(ok)
        self.assertNotIn("on", [c[c.index("-a") + 1] for c in hub.calls if "-a" in c])

    def test_a_failed_on_says_the_port_is_off(self):
        hub = Hub(on=1)
        ok, why = script.power_cycle(REFUSED, "20-2:4", "LS5600RE", hub, sleep=lambda s: None)
        self.assertFalse(ok)
        self.assertIn("off", why)
        self.assertEqual(sum(1 for c in hub.calls if c[-1] == "on"), 2)     # tried twice

    def test_the_right_port_cycles(self):
        hub = Hub()
        ok, why = script.power_cycle(REFUSED, "20-2:4", "LS5600RE", hub, sleep=lambda s: None)
        self.assertTrue(ok, why)
        self.assertEqual([c[-1] for c in hub.calls if "-a" in c], ["off", "on"])
        self.assertIn(["-l", "20-2", "-p", "4", "-a", "off"], hub.calls)


class Wrapper(unittest.TestCase):
    """CCStudio prints the error number in its own log line; the exception the script sees says only "aborted"
    (LS5600RE, 2026-10-25). The wrapper reads the whole output."""
    OUT = ("fatal: IcePick_C: Error connecting to the target: (Error -242 @ 0x0)\n"
           "fatal: connect refused: Operation was aborted on 'Cortex_M33_0'\n")

    def test_a_refusal_with_the_number_in_the_log_cycles_and_retries(self):
        board = board_route("cc1354p10")
        runs = [(3, self.OUT), (0, "ID 1BB7802F 1287F000\n")]
        with mock.patch.object(script, "_once", side_effect=lambda *a: runs.pop(0)), \
             mock.patch.object(script, "power_cycle_port", return_value="20-2:4"), \
             mock.patch.object(script, "power_cycle", return_value=(True, "")) as pc, \
             mock.patch.object(script, "uhubctl", return_value=(0, STATUS)), mock.patch.object(script.time, "sleep"):
            code, out = script.run(board, "LS5600RE", ["identity"])
        self.assertEqual(code, 0)
        self.assertTrue(out.startswith("CYCLED 20-2:4"))
        self.assertIn("-242", pc.call_args[0][0])

    def test_a_refused_cycle_says_why_and_does_not_retry(self):
        board = board_route("cc1354p10")
        with mock.patch.object(script, "_once", return_value=(3, self.OUT)) as once, \
             mock.patch.object(script, "power_cycle_port", return_value="20-2:3"), \
             mock.patch.object(script, "uhubctl", return_value=(0, STATUS)):
            code, out = script.run(board, "LS5600RE", ["identity"])
        self.assertEqual(code, 3)
        self.assertIn("MG000001", out)
        self.assertEqual(once.call_count, 1)


if __name__ == "__main__":
    unittest.main()
