"""CC1354P10 write boundary and pin check (Codex review 2026-10-25, findings 5 and 6), no board.

The write covers only the 2 KB sectors the image's segments touch, each written at its own address, and the
preflight names those sectors; the LED check needs DIO6's own output-enable bit and the pin's output register to
agree with the program's log, and is not verified when they cannot be read.
"""
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
sys.path.insert(0, str(ROOT / "tools" / "tests"))
import ccs13  # noqa: E402
import target  # noqa: E402
import verify  # noqa: E402
from core import Stop, board_route  # noqa: E402
from test_frl_cc13 import elf, write  # noqa: E402

BOARD = board_route("cc1354p10")
LED = BOARD["leds"][0]
SYMS = {"frl_log_n": 0x20000100, "frl_log_ms": 0x20000000}

spec = importlib.util.spec_from_file_location("frl_cc13x4", ROOT / "tools" / "frl" / "ccs" / "frl_cc13x4.py")
script = importlib.util.module_from_spec(spec)
spec.loader.exec_module(script)


class Sectors(unittest.TestCase):                          # finding 6
    def test_a_sparse_image_writes_only_its_sectors(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\x01" * 0x10), (0x80000, b"\x02" * 0x900), (0x50000000, b"\x55" * 16)], SYMS))
            blocks = ccs13.write_blocks(img)
        self.assertEqual([(a, len(b)) for a, b in blocks], [(0x0, 0x800), (0x80000, 0x1000)])
        self.assertEqual(blocks[1][1][:0x900], b"\x02" * 0x900)
        self.assertEqual(blocks[1][1][0x900:], b"\xff" * 0x700)

    def test_segments_in_one_sector_share_a_block(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\x01" * 0x10), (0x400, b"\x02" * 0x10), (0x800, b"\x03" * 4)], SYMS))
            blocks = ccs13.write_blocks(img)
        self.assertEqual([(a, len(b)) for a, b in blocks], [(0x0, 0x1000)])

    def test_the_preflight_names_the_erased_sectors(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\x01" * 0x10), (0x80000, b"\x02" * 0x900)], SYMS))
            segs = target.check_writes(BOARD, str(img))
        self.assertEqual(segs[0]["erases"], "0x00000000-0x00000800")
        self.assertEqual(segs[1]["erases"], "0x00080000-0x00081000")

    def test_program_writes_each_block_at_its_address_in_one_session(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\x01" * 0x10), (0x80000, b"\x02" * 0x900)], SYMS))
            with mock.patch.object(ccs13, "run", return_value=(0, "OK programmed\n")) as r:
                ccs13.program(BOARD, "LS5600RE", str(img))
        args = r.call_args[0][2]
        self.assertEqual(args[0], "program")
        self.assertEqual([args[2], args[4]], ["0x0", "0x80000"])
        self.assertEqual(r.call_count, 1)


class PinCheck(unittest.TestCase):                         # finding 5
    def reader(self, doe, dout, fail=()):
        def read(addr):
            if addr in fail:
                raise RuntimeError("peripheral off")
            return {script.GPIO_DOE: doe, script.GPIO_DOUT: dout}[addr]
        return read

    def test_another_pin_enabled_is_not_dio6(self):
        self.assertEqual(script.pin_lines(self.reader(1 << 7, 0), 6), ["E 0", "O 0"])
        self.assertEqual(script.pin_lines(self.reader(1 << 6, 1 << 6), 6), ["E 1", "O 1"])

    def test_an_unreadable_register_is_unknown(self):
        self.assertEqual(script.pin_lines(self.reader(0, 0, fail=(script.GPIO_DOE, script.GPIO_DOUT)), 6), ["E ?", "O ?"])

    def run_sample(self, n, tail):
        t, times = 0, []
        for k in range(max(0, n - 64), n):
            times.append(t)
            t += (150, 150, 150, 900)[k % 4]
        out = f"N {n} 5003\n" + "".join(f"T {x}\n" for x in times) + tail
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\0" * 8)], SYMS))
            with mock.patch.object(ccs13, "run", return_value=(0, out)) as r:
                s = ccs13.sample(BOARD, "LS5600RE", LED, 5, str(img))
        self.assertIn("6", [str(a) for a in r.call_args[0][2]])      # the LED's own DIO number goes to the script
        return verify.pattern_check(s, [150, 150, 150, 900], 20, LED["label"], 100)

    def test_unreadable_output_enable_is_not_verified(self):
        res = self.run_sample(70, "E ?\nO ?\n")
        self.assertEqual(res["outcome"], "unverified", res["message"])

    def test_no_enable_line_is_not_verified(self):
        res = self.run_sample(70, "")
        self.assertEqual(res["outcome"], "unverified", res["message"])

    def test_dio6_not_an_output_fails(self):
        res = self.run_sample(70, "E 0\nO 0\n")
        self.assertEqual(res["outcome"], "failed")

    def test_pin_level_must_follow_the_log(self):
        # 70 changes: the last, change 69, is odd: LED off, so the pin's output register must read 0
        self.assertTrue(self.run_sample(70, "E 1\nO 0\n")["ok"])
        res = self.run_sample(70, "E 1\nO 1\n")
        self.assertEqual(res["outcome"], "failed")
        self.assertIn("output register", res["message"])


if __name__ == "__main__":
    unittest.main()
