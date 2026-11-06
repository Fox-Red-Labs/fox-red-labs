"""frl identify with no --board (tools/frl/discover.py): parsing and matching, on OpenOCD output recorded on our
bench 2026-10-24, with no probe."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import discover  # noqa: E402

MEM, JTAG = discover.tables()

SWD_G3519_ASLEEP = """Info : SWD DPIDR 0x6ba02477
Info : [chip.cpu] Examination succeed
0x41c40004: 00000000
Info : SWD DPIDR 0x6ba02477
Error: Failed to read memory at 0x80111004
"""
JTAG_CC1354 = "Info : JTAG tap: probe.tap tap/device found: 0x2bb7802f (mfg: 0x017 (Texas Instruments), part: 0xbb78, ver: 0x2)"
JTAG_GARBAGE = "Info : JTAG tap: probe.tap tap/device found: 0x55557fff (mfg: 0x7ff (<invalid>), part: 0x5557, ver: 0x5)"


class Registry(unittest.TestCase):
    def test_tables_carry_the_bench_families(self):
        for addr in (0x41C40004, 0x80111004, 0x60111004):
            self.assertIn(addr, MEM)
        self.assertTrue(all(a >= 0x10000 for a in MEM), "MSP430's 16-bit IDs are not SWD reads")
        self.assertIn(0x0BB7802F, JTAG)


class Parsing(unittest.TestCase):
    def test_a_dark_register_is_readable_and_a_missing_one_is_not(self):
        r = discover.readable(SWD_G3519_ASLEEP)
        self.assertEqual(r, {0x41C40004: 0})
        self.assertEqual(discover.parse_dpidr(SWD_G3519_ASLEEP), "0x6BA02477")

    def test_taps(self):
        self.assertEqual(discover.parse_taps(JTAG_CC1354), [0x2BB7802F])


class Matching(unittest.TestCase):
    def test_mspm0g3519_deviceid(self):
        hit = discover.match_mem({0x41C40004: 0x0BBA902F}, MEM)[0]
        self.assertEqual(hit["families"], ["MSPM0G"])
        self.assertIn("MSPM0G3519", hit["parts"])

    def test_am13_prototype_names_the_family_without_a_part(self):
        hit = discover.match_mem({0x60111004: 0x0000002F}, MEM)[0]
        self.assertTrue(any("AM13" in f for f in hit["families"]))
        self.assertFalse(hit["exact"])

    def test_a_non_ti_word_is_not_a_deviceid(self):
        self.assertEqual(discover.match_mem({0x41C40004: 0x12345678}, MEM), [])

    def test_cc1354_idcode_version_masked(self):
        tap = discover.match_jtag([0x2BB7802F], JTAG)[0]
        self.assertTrue(any("CC13x4" in f for f in tap["families"]))
        self.assertIn("CC1354P10", tap["parts"])

    def test_cc35_configuration_port_id(self):
        self.assertIn("CC3551E", discover.match_jtag([0x1BB9602F], JTAG)[0]["parts"])

    def test_c2000_partidh(self):
        self.assertIn(("F2800x (F280013x)", "TMS320F2800137"), discover.c28_ids()[0x06FF0500])

    def test_garbage_scan_is_not_ti(self):
        tap = discover.match_jtag(discover.parse_taps(JTAG_GARBAGE), JTAG)[0]
        self.assertFalse(tap["ti"])
        self.assertEqual(tap["families"], [])


if __name__ == "__main__":
    unittest.main()
