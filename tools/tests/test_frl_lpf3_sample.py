"""CC23xx blink check (tools/frl/lpf3.py sample, verify.pattern_check on an untimed sample), with no board.

OpenOCD's attach parks these parts in the boot ROM, so a pin read through it always shows the output disabled
(LP-EM-CC2340R5, 2026-11-02). The check must read through DSLite after a real reset instead."""
import struct
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import lpf3  # noqa: E402
import target  # noqa: E402
import verify  # noqa: E402
from core import board_route  # noqa: E402

BOARD = board_route("cc2340r5")
LED = BOARD["leds"][0]


def fake_read(doe, douts):
    it = iter(douts)

    def read(board, serial, addr, nbytes, out):
        words = bytearray(nbytes)
        struct.pack_into("<I", words, 0x500 - 0x200, doe)
        struct.pack_into("<I", words, 0, next(it))
        return bytes(words)
    return read


class LpfSample(unittest.TestCase):
    def sample(self, doe, douts):
        with mock.patch.object(lpf3, "start") as st, mock.patch.object(lpf3, "read", fake_read(doe, douts)), \
                mock.patch.object(lpf3.time, "time", side_effect=[i * 4.0 for i in range(40)]), \
                mock.patch.object(lpf3.time, "sleep") as sl:
            s = target.sample(BOARD, "LS470GTU", LED, 4.0)
        st.assert_called_once()          # the program is reset and running before any pin is read
        s["sleeps"] = [c.args[0] for c in sl.call_args_list]
        return s

    def test_toggling_pin_with_output_enabled_passes(self):
        s = self.sample(1 << 14, [0, 0, 1 << 14])
        self.assertEqual(s["output_enabled"], 1)
        res = verify.pattern_check(s, [150, 150, 150, 900], 40, LED["label"])
        self.assertTrue(res["ok"], res["message"])
        self.assertIn("timing", res["message"])

    def test_reads_are_spaced_unevenly_so_they_do_not_alias_with_the_pattern(self):
        s = self.sample(1 << 14, [0] * 16)
        self.assertEqual(s["samples"], 16)
        self.assertEqual(len(s["sleeps"]), 15)
        self.assertGreater(len(set(round(x, 3) for x in s["sleeps"])), 10)
        self.assertTrue(all(0 <= x <= 1.4 for x in s["sleeps"]))

    def test_output_disabled_fails(self):
        s = self.sample(0, [0, 0, 0, 0, 0, 0, 0, 0])
        self.assertFalse(verify.pattern_check(s, [150, 150, 150, 900], 40, LED["label"])["ok"])

    def test_pin_that_never_moves_fails(self):
        s = self.sample(1 << 14, [0] * 16)
        res = verify.pattern_check(s, [150, 150, 150, 900], 40, LED["label"])
        self.assertFalse(res["ok"])
        self.assertEqual(res["code"], "FRL-E040")


if __name__ == "__main__":
    unittest.main()
