"""The CC1354P10 route's own pieces (tools/frl/ccs13.py, the not_written CCFG in check_writes), with no board."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import ccs13  # noqa: E402
import target  # noqa: E402
import verify  # noqa: E402
from core import Stop, board_route  # noqa: E402


def elf(segments, symbols):
    """A minimal 32-bit little-endian ELF: PT_LOAD segments [(addr, bytes)] and a symbol table {name: value}."""
    strtab = b"\0" + b"".join(n.encode() + b"\0" for n in symbols)
    syms = b"\0" * 16
    off = 1
    for n, v in symbols.items():
        syms += struct.pack("<IIIBBH", off, v, 4, 0x11, 0, 1)
        off += len(n) + 1
    ph_off, ph_n = 52, len(segments)
    data_off = ph_off + 32 * ph_n
    body, phs = b"", b""
    for a, b in segments:
        phs += struct.pack("<IIIIIIII", 1, data_off + len(body), a, a, len(b), len(b), 5, 4)
        body += b
    sym_off = data_off + len(body)
    str_off = sym_off + len(syms)
    sh_off = str_off + len(strtab)
    shs = b"\0" * 40
    shs += struct.pack("<IIIIIIIIII", 0, 2, 0, 0, sym_off, len(syms), 2, 1, 4, 16)   # .symtab, link to section 2
    shs += struct.pack("<IIIIIIIIII", 0, 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0)  # .strtab
    hdr = b"\x7fELF\x01\x01\x01" + b"\0" * 9 + struct.pack("<HHIIIIIHHHHHH", 2, 40, 1, 0, ph_off, sh_off, 0, 52, 32, ph_n, 40, 3, 0)
    return hdr + phs + body + syms + strtab + shs


def write(tmp, blob):
    p = Path(tmp) / "img.out"
    p.write_bytes(blob)
    return p


BOARD = board_route("cc1354p10")


class Image(unittest.TestCase):
    def test_main_flash_sectors_without_the_ccfg(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\x00\x00\x04\x20\x01\x02\x00\x00"), (0x100, b"\xaa" * 4), (0x50000000, b"\x55" * 16)], {}))
            (a, b), = ccs13.write_blocks(img)
        self.assertEqual((a, len(b)), (0x0, 0x800))         # the one 2 KB sector the image touches
        self.assertEqual(b[:8], b"\x00\x00\x04\x20\x01\x02\x00\x00")
        self.assertEqual(b[8:0x100], b"\xff" * (0x100 - 8))
        self.assertEqual(b[0x100:0x104], b"\xaa" * 4)
        self.assertNotIn(b"\x55" * 16, b)

    def test_symbols_from_the_elf(self):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\0" * 8)], {"frl_log_n": 0x20000100, "frl_log_ms": 0x20000000}))
            self.assertEqual(ccs13.symbol(img, "frl_log_n"), 0x20000100)
            self.assertEqual(ccs13.symbol(img, "frl_log_ms"), 0x20000000)
            with self.assertRaises(Stop):
                ccs13.symbol(img, "blink_count")

    def test_check_writes_keeps_the_ccfg_out_and_refuses_elsewhere(self):
        with tempfile.TemporaryDirectory() as d:
            ok = write(d, elf([(0x0, b"\0" * 8), (0x50000000, b"\0" * 0x800)], {}))
            self.assertEqual(target.check_writes(BOARD, str(ok)), [{"addr": "0x00000000", "bytes": 8, "erases": "0x00000000-0x00000800"}])
            bad = Path(d) / "bad.out"
            bad.write_bytes(elf([(0x50000800, b"\0" * 4)], {}))
            with self.assertRaises(Stop):
                target.check_writes(BOARD, str(bad))


class Sample(unittest.TestCase):
    def out(self, n, times):
        return f"N {n} 5003\n" + "".join(f"T {t}\n" for t in times) + f"E 1\nO {1 if (n - 1) % 2 == 0 else 0}\n"

    def run_sample(self, n, times):
        with tempfile.TemporaryDirectory() as d:
            img = write(d, elf([(0x0, b"\0" * 8)], {"frl_log_n": 0x20000100, "frl_log_ms": 0x20000000}))
            with mock.patch.object(ccs13, "run", return_value=(0, self.out(n, times))):
                return ccs13.sample(BOARD, "LS5600RE", BOARD["leds"][0], 5, str(img))

    def test_levels_follow_the_log_parity(self):
        # 70 changes: the ring of 64 holds changes 6..69, so the first logged level is change 6, even, lit
        t, times = 0, []
        for k in range(6, 70):
            times.append(t)
            t += (150, 150, 150, 900)[k % 4]
        s = self.run_sample(70, times)
        self.assertEqual(s["transitions"][0], (0, 1))
        self.assertEqual(s["transitions"][1], (150, 0))
        res = verify.pattern_check(s, [150, 150, 150, 900], 20, "red LED (DIO6)", 100)
        self.assertTrue(res["ok"], res["message"])
        self.assertFalse(verify.pattern_check(s, [300, 300], 20, "red LED (DIO6)", 100)["ok"])

    def test_no_image_is_not_verified(self):
        with self.assertRaises(Stop) as e:
            ccs13.sample(BOARD, "LS5600RE", BOARD["leds"][0], 5, None)
        self.assertEqual(e.exception.result["outcome"], "unverified")


class Connect(unittest.TestCase):
    def test_refused_without_a_switchable_port_says_unplug(self):
        with mock.patch.object(ccs13, "power_cycle_port", return_value=""):
            with self.assertRaises(Stop) as e:
                ccs13._failed("fatal: connect refused: Error -242", "LS5600RE", "read")
        self.assertIn("unplug", e.exception.result["next"])

    def test_identity_reports_a_power_cycle(self):
        with mock.patch.object(ccs13, "run", return_value=(0, "connect refused (x); cycling\nCYCLED 20-2:4\nID 1BB7802F 1287F000\n")):
            ident = ccs13.check_identity(BOARD, "LS5600RE")
        self.assertEqual(ident["power_cycled"], "20-2:4")
        self.assertEqual(ident["deviceid"], "0x1BB7802F")

    def test_another_chip_is_refused(self):
        with mock.patch.object(ccs13, "run", return_value=(0, "ID 0BB8402F 00000000\n")):
            with self.assertRaises(Stop) as e:
                ccs13.check_identity(BOARD, "LS5600RE")
        self.assertEqual(e.exception.result["outcome"], "refused")


if __name__ == "__main__":
    unittest.main()
