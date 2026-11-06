"""CC35X1 write and restore integrity (Codex review 2026-10-25, findings 1 to 4 and 9), no board.

What the board cannot give (a read-back), the files must: the signed bundle is copied and hashed, it must hold the
bytes of the .out it claims to be, the backup is bound to the chip's unique ID, a changed copy or backup.json is
refused, a different flash layout is refused, and a failed reset is not reported as a started program.
"""
import json
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
sys.path.insert(0, str(ROOT / "tools" / "benchmark"))
import ops  # noqa: E402
import toolbox  # noqa: E402
from core import Stop, board_route  # noqa: E402

BOARD = board_route("cc35x1")
UDID = "120E1660-6A65B1A5"
SEGS = [(0x14000000, b"\x11" * 64), (0x14000400, b"\x22" * 32)]
LAYOUT = {"internal": {"ota": True}, "inputs": {"vendor_app_slot1_size": 2688, "boot_sector_size": 8}}


def elf(segs):
    """A 32-bit little-endian Arm ELF with one PT_LOAD per segment."""
    ph = 52
    data_at = ph + 32 * len(segs)
    hdr = bytearray(b"\x7fELF\x01\x01\x01" + b"\0" * 9)
    hdr += struct.pack("<HHIIIIIHHHHHH", 2, 40, 1, 0, ph, 0, 0, 52, 32, len(segs), 0, 0, 0)
    body, phs, off = b"", b"", data_at
    for addr, blob in segs:
        phs += struct.pack("<IIIIIIII", 1, off, addr, addr, len(blob), len(blob), 5, 4)
        body += blob
        off += len(blob)
    return bytes(hdr) + phs + body


def make_build(d, segs=SEGS, layout=LAYOUT, vendor=None):
    """A build folder like build_cc35.py leaves: the .out, external_memory_configurator.json, toolbox/ with the
    signed images and tool_settings.json naming them by absolute path."""
    d = Path(d)
    (d / "toolbox").mkdir(parents=True, exist_ok=True)
    (d / "uart2echo.out").write_bytes(elf(segs))
    if layout is not None:
        (d / "external_memory_configurator.json").write_text(json.dumps(layout))
    if vendor is None:                                   # header, then each segment at its offset from the first
        base = segs[0][0]
        img = bytearray(b"\xAA" * 16 + b"\xFF" * (segs[-1][0] - base + len(segs[-1][1])))
        for addr, blob in segs:
            img[16 + addr - base:16 + addr - base + len(blob)] = blob
        vendor = bytes(img)
    files = {"primary_vendor_image": "primary_vendor_image.sign.bin", "boot_sector": "boot_sector_image.bin",
             "programming_instructions": "programming_instructions_image.sign.bin"}
    for name in files.values():
        (d / "toolbox" / name).write_bytes(vendor if name.startswith("primary_vendor") else name.encode())
    ts = {"programming_debug_and_ota_signed_components": {
        "tbl_container_programming": None, "secondary_vendor_image": "",
        "programming_instructions": str(d / "toolbox" / files["programming_instructions"]),
        "actions_req_paths": {"programming": str(d / "toolbox" / files["boot_sector"])},
        "primary_vendor_image": str(d / "toolbox" / files["primary_vendor_image"]),
        "boot_sector": str(d / "toolbox" / files["boot_sector"])}}
    (d / "toolbox" / "tool_settings.json").write_text(json.dumps(ts))
    return d / "uart2echo.out"


class Env(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home = Path(self.tmp.name) / "home"
        self.home.mkdir()
        p = mock.patch.dict(os.environ, {"FRL_HOME": str(self.home)})
        p.start()
        self.addCleanup(p.stop)
        self.addCleanup(self.tmp.cleanup)
        import backup as bk
        self.bk = bk
        b = mock.patch.object(bk, "BACKUPS", self.home / "backups")
        b.start()
        self.addCleanup(b.stop)


class BundleIsTheBuild(Env):                              # finding 1
    def test_the_signed_image_must_hold_the_out(self):
        img = make_build(Path(self.tmp.name) / "b")
        toolbox.check_from_build(img, toolbox.settings_for(img))   # no Stop
        other = make_build(Path(self.tmp.name) / "c", segs=[(0x14000000, b"\x33" * 64), (0x14000400, b"\x22" * 32)])
        (other.parent / "toolbox" / "primary_vendor_image.sign.bin").write_bytes(
            (img.parent / "toolbox" / "primary_vendor_image.sign.bin").read_bytes())      # a stale bundle beside a new .out
        with self.assertRaises(Stop) as e:
            toolbox.check_from_build(other, toolbox.settings_for(other))
        self.assertEqual(e.exception.result["code"], "FRL-E031")

    def test_a_missing_referenced_file_is_refused(self):
        img = make_build(Path(self.tmp.name) / "b")
        (img.parent / "toolbox" / "boot_sector_image.bin").unlink()
        with self.assertRaises(Stop) as e:
            toolbox.copy_bundle(toolbox.settings_for(img), Path(self.tmp.name) / "copy")
        self.assertEqual(e.exception.result["code"], "FRL-E031")

    def test_the_copy_names_only_its_own_files_and_survives_a_rebuild(self):
        img = make_build(Path(self.tmp.name) / "b")
        man = toolbox.copy_bundle(toolbox.settings_for(img), Path(self.tmp.name) / "copy")
        ts = json.loads((Path(self.tmp.name) / "copy" / "tool_settings.json").read_text())
        for p in toolbox.referenced(ts):
            self.assertEqual(Path(p).parent, (Path(self.tmp.name) / "copy").resolve())
        (img.parent / "toolbox" / "primary_vendor_image.sign.bin").write_bytes(b"rebuilt")   # the build folder moves on
        toolbox.check_copy(Path(self.tmp.name) / "copy", man)                                 # the copy does not
        self.assertEqual(man["layout"]["ota"], True)

    def test_flash_programs_the_checked_copy_not_the_build_folder(self):
        img = make_build(Path(self.tmp.name) / "b")
        seen = {}

        def prog(board, serial, settings):
            seen["settings"] = settings
            seen["vendor"] = Path(json.loads(Path(settings).read_text())["programming_debug_and_ota_signed_components"]["primary_vendor_image"]).read_bytes()
            return "Writing binary size of 96 bytes within 0.1 sec"
        good = (img.parent / "toolbox" / "primary_vendor_image.sign.bin").read_bytes()
        toolbox.record_layout(UDID, toolbox.layout_of(img.parent))
        with mock.patch("probes.select", return_value="E10000FQ"), mock.patch.object(ops, "_lab_board_lock"), \
             mock.patch.object(toolbox, "check_identity", return_value={"part": "CC3551E", "udid": UDID}), \
             mock.patch.object(toolbox, "program", side_effect=prog):
            res = ops.flash("cc35x1", str(img), probe="E10000FQ")
        self.assertTrue(res["ok"], res)
        self.assertNotEqual(Path(seen["settings"]).resolve().parent, (img.parent / "toolbox").resolve())
        self.assertEqual(seen["vendor"], good)
        self.assertEqual(res["data"]["bundle"]["sha256"], toolbox.copy_bundle(toolbox.settings_for(img), Path(self.tmp.name) / "x")["sha256"])


class Layout(Env):                                         # finding 4
    def test_unknown_layout_is_refused_until_stated(self):
        img = make_build(Path(self.tmp.name) / "b")
        with self.assertRaises(Stop) as e:
            toolbox.check_layout(UDID, toolbox.layout_of(img.parent))
        self.assertEqual(e.exception.result["code"], "FRL-E036")
        with mock.patch.dict(os.environ, {"FRL_CC35_LAYOUT": "ota"}):
            toolbox.check_layout(UDID, toolbox.layout_of(img.parent))

    def test_a_different_layout_is_refused_and_erased_lets_it_through(self):
        toolbox.record_layout(UDID, toolbox.layout_of(make_build(Path(self.tmp.name) / "b").parent))
        single = make_build(Path(self.tmp.name) / "s", layout={"internal": {"ota": False}, "inputs": {"vendor_app_slot1_size": 6248}})
        with self.assertRaises(Stop) as e:
            toolbox.check_layout(UDID, toolbox.layout_of(single.parent))
        self.assertEqual(e.exception.result["code"], "FRL-E036")
        self.assertIn("full_flash_erase", e.exception.result["next"])
        with mock.patch.dict(os.environ, {"FRL_CC35_LAYOUT": "erased"}):
            toolbox.check_layout(UDID, toolbox.layout_of(single.parent))
        with mock.patch.dict(os.environ, {"FRL_CC35_LAYOUT": "ota"}):          # a statement that contradicts the record
            with self.assertRaises(Stop):
                toolbox.check_layout(UDID, toolbox.layout_of(single.parent))

    def test_a_build_without_a_layout_file_is_refused(self):
        img = make_build(Path(self.tmp.name) / "b", layout=None)
        with self.assertRaises(Stop) as e:
            toolbox.check_layout(UDID, toolbox.layout_of(img.parent))
        self.assertEqual(e.exception.result["code"], "FRL-E036")


class Reset(unittest.TestCase):                            # finding 9
    def test_a_failed_reset_is_not_a_started_program(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "tool_settings.json").write_text("{}")
            with mock.patch.object(toolbox, "run", return_value=(0, "Writing binary size of 9 bytes within 0.1 sec")), \
                 mock.patch.object(toolbox, "start", return_value=(1, "XDS110 reset failed")):
                with self.assertRaises(Stop) as e:
                    toolbox.program(BOARD, "E10000FQ", str(Path(d) / "tool_settings.json"))
        self.assertEqual(e.exception.result["code"], "FRL-E042")
        self.assertIn("written", e.exception.result["message"])

    def test_program_takes_only_a_settings_file(self):
        with self.assertRaises(Stop):
            toolbox.program(BOARD, "E10000FQ", "/x/uart2echo.out")


class Restore(Env):                                        # findings 2 and 3
    def backup(self):
        img = make_build(Path(self.tmp.name) / "b")
        toolbox.record_layout(UDID, toolbox.layout_of(img.parent))
        with mock.patch.dict(os.environ, {"FRL_RESTORE_IMAGE": str(img)}), \
             mock.patch("probes.select", return_value="E10000FQ"), mock.patch.object(ops, "_lab_board_lock"), \
             mock.patch.object(toolbox, "check_identity", return_value={"part": "CC3551E", "udid": UDID}), \
             mock.patch.object(toolbox, "fuse_data", return_value="fuse report"), \
             mock.patch("target.console_port", return_value="/dev/null"), mock.patch("target.start"), \
             mock.patch("verify.read_serial", return_value=b"\r\nCC35xx OTA Example Application FRL 0.0.1.0\r\n"):
            res = ops.backup("cc35x1", probe="E10000FQ")
        self.assertTrue(res["ok"], res)
        return img, Path(res["data"]["backup"])

    def restore(self, folder, udid=UDID, console=b"CC35xx OTA Example Application FRL 0.0.1.0\r\n", seen=None):
        def prog(board, serial, settings):
            if seen is not None:
                seen.append(settings)
            return "Writing binary size of 96 bytes within 0.1 sec"
        with mock.patch("probes.select", return_value="E10000FQ"), mock.patch.object(ops, "_lab_board_lock"), \
             mock.patch.object(toolbox, "check_identity", return_value={"part": "CC3551E", "udid": udid}), \
             mock.patch.object(toolbox, "program", side_effect=prog), \
             mock.patch("target.console_port", return_value="/dev/null"), \
             mock.patch("verify.read_serial", return_value=console):
            return ops.restore(str(folder), probe="E10000FQ")

    def test_backup_keeps_a_hashed_copy_and_the_chip_id(self):
        img, folder = self.backup()
        man = json.loads((folder / "backup.json").read_text())
        self.assertEqual(man["udid"], UDID)
        self.assertTrue((folder / "bundle" / "tool_settings.json").is_file())
        self.assertIn(folder.name, self.bk.read_index(folder.parent))

    def test_restore_programs_the_copy_after_the_build_folder_changed(self):
        img, folder = self.backup()
        (img.parent / "toolbox" / "primary_vendor_image.sign.bin").write_bytes(b"something else")
        seen = []
        res = self.restore(folder, seen=seen)
        self.assertTrue(res["ok"], res)
        self.assertEqual(Path(seen[0]).resolve(), (folder / "bundle" / "tool_settings.json").resolve())

    def test_a_changed_copy_is_refused(self):
        img, folder = self.backup()
        (folder / "bundle" / "primary_vendor_image.sign.bin").write_bytes(b"tampered")
        res = self.restore(folder)
        self.assertEqual(res["code"], "FRL-E052", res)

    def test_an_edited_backup_json_is_refused(self):
        img, folder = self.backup()
        man = json.loads((folder / "backup.json").read_text())
        man["banner"] = "x"
        (folder / "backup.json").write_text(json.dumps(man))
        res = self.restore(folder)
        self.assertEqual(res["code"], "FRL-E052", res)

    def test_another_chip_on_the_same_probe_is_refused(self):
        img, folder = self.backup()
        seen = []
        res = self.restore(folder, udid="00000000-11111111", seen=seen)
        self.assertEqual(res["code"], "FRL-E051", res)
        self.assertEqual(seen, [])

    def test_the_banner_must_be_a_whole_line(self):
        img, folder = self.backup()
        res = self.restore(folder, console=b"boot: CC35xx OTA Example Application FRL 0.0.1.0 (old)\r\n")
        self.assertEqual(res["code"], "FRL-E040", res)

    def test_a_backup_without_a_named_image_is_refused(self):
        folder = self.home / "backups" / "cc35x1-E10000FQ-20261026-000000"
        folder.mkdir(parents=True)
        raw = json.dumps({"board": "cc35x1", "probe": "E10000FQ", "kind": "reprogram", "udid": UDID, "banner": "x", "bundle": None}).encode()
        (folder / "backup.json").write_bytes(raw)
        (self.home / "backups" / "index.json").write_text(json.dumps({folder.name: self.bk.sha(raw)}))
        res = self.restore(folder)
        self.assertEqual(res["code"], "FRL-E053", res)

    def test_an_older_backup_without_the_chip_id_is_refused(self):
        folder = self.home / "backups" / "cc35x1-E10000FQ-20261025-081925"
        folder.mkdir(parents=True)
        raw = json.dumps({"board": "cc35x1", "probe": "E10000FQ", "kind": "reprogram", "banner": "x",
                          "restore_settings": "/somewhere/tool_settings.json"}).encode()
        (folder / "backup.json").write_bytes(raw)
        res = self.restore(folder)
        self.assertIn(res["code"], ("FRL-E051", "FRL-E052"), res)
        self.assertFalse(res["ok"])


if __name__ == "__main__":
    unittest.main()
