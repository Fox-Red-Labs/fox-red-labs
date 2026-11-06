"""frl's connect ladder and write report (tools/frl/target.py), on OpenOCD and xdsdfu output recorded on our bench
2026-10-24, with no probe: run() and the probe tools are replaced, so the order of steps and what each records is
what is tested."""
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import core  # noqa: E402
import target  # noqa: E402

# LP-MSPM0C1104 in a boot loop, xPack OpenOCD with the XDS110 driver (2026-10-24)
C1104_LOOP = """adapter speed: 2500 kHz
Debug: 58 46 xds110.c:431 usb_connect(): XDS110: connected
Debug: 59 150 xds110.c:641 xds_execute(): XDS110: command 0x0f returned error -614
Error: XDS110: command 0x0f returned error -614
Error: init mode failed (unable to connect to the target)
"""
MSPM0_READ = "Info : SWD DPIDR 0x6ba02477\n0x41c40000: 1d01a0b6 2bb8802f 00000000 \n"
BUSY = "Error: LIBUSB_ERROR_BUSY\nError: unable to claim interface\n"
BAD_CMD = "Error: invalid command name \"mdx\"\n"
# The C1104 after its restore (2026-10-24) and the MSPM0 sweep log (MSPM0G3507 example sweep)
WRITE_PLAIN = "wrote 16384 bytes from file oob-flash.bin in 3.889733s (4.113 KiB/s)\nverified 16384 bytes in 0.065108s (245.746 KiB/s)\n"
WRITE_FALLBACK = ("** Programming Started **\n** Programming Finished **\n** Verify Started **\n"
                  "Error: [mspm0x.cpu] error executing cortex_m crc algorithm\n"
                  "Error: checksum mismatch - attempting binary compare\n** Verified OK **\n")
# `xdsdfu -e`, two of the bench probes; the second probe's version is lowered from 3.0.0.43 to 3.0.0.26 (the
# C1104 probe's shipped version) to test the minimum.
XDSDFU_E = """USB Device Firmware Upgrade Utility

Found 2 devices.

<<<< Device 0 >>>>

VID: 0x0451    PID: 0xbef3
Device Name:   XDS110 Embed with CMSIS-DAP
Version:       3.0.0.43
Manufacturer:  Texas Instruments
Serial Num:    MC110001
Mode:          Runtime
Configuration: Standard

<<<< Device 1 >>>>

VID: 0x0451    PID: 0xbef3
Device Name:   XDS110 Embed with CMSIS-DAP
Version:       3.0.0.26
Manufacturer:  Texas Instruments
Serial Num:    LS5600RE
Mode:          Runtime
Configuration: Standard
"""


def board(**target):
    t = {"speed_khz": 2500, "identity": {"addr": "0x41C40000", "words": 3, "fields": ["traceid", "deviceid", "userid"]}}
    t.update(target)
    return {"kit": "TEST-KIT", "target": t}


def read_ok(code, out):
    return code == 0 and "0x41c40000:" in out


class Ladder(unittest.TestCase):
    def setUp(self):
        target._SETTINGS.clear()
        target._LOG.clear()
        self.p_reset = mock.patch.object(target, "probe_firmware", return_value=None)
        self.p_usb = mock.patch.object(target, "usb_cycle", return_value=None)
        self.p_reset.start()
        self.p_usb.start()

    def tearDown(self):
        self.p_reset.stop()
        self.p_usb.stop()

    def runs(self, outputs):
        seen = []

        def fake(b, serial, cmds, timeout=120, halt_ok=True):
            seen.append(dict(target._SETTINGS.get(serial, {})))
            code, out = outputs.pop(0)
            return code, out
        return seen, mock.patch.object(target, "run", side_effect=fake)

    def test_recorded_failures_are_attach_failures_and_busy_is_not(self):
        self.assertTrue(target.attach_failed(C1104_LOOP))
        self.assertFalse(target.attach_failed(BUSY))
        self.assertFalse(target.attach_failed(BAD_CMD))

    def test_first_try_works_and_nothing_is_recorded(self):
        seen, p = self.runs([(0, MSPM0_READ)])
        with p:
            target.attach(board(), "MC110001", ["mdw 0x41C40000 3"], read_ok)
        self.assertEqual(len(seen), 1)
        self.assertIsNone(target.connect_note("MC110001"))

    def test_mspm0_skips_connect_under_reset_and_slows_the_clock(self):
        seen, p = self.runs([(1, C1104_LOOP), (0, MSPM0_READ)])
        with p:
            code, out = target.attach(board(connect_under_reset=False), "MC110001", ["mdw 0x41C40000 3"], read_ok)
        self.assertEqual(code, 0)
        note = target.connect_note("MC110001")
        self.assertEqual(note["worked"], "clock 1000 kHz")
        self.assertEqual([t["step"] for t in note["tried"]], ["as configured", "clock 1000 kHz"])
        self.assertIn("Error: init mode failed (unable to connect to the target)", note["tried"][0]["words"])
        self.assertEqual(seen[-1].get("speed_khz"), 1000)

    def test_order_is_under_reset_then_clocks_then_probe_then_usb(self):
        names = [n for n, _ in target.steps(board(speed_khz=4000), "X")]
        self.assertEqual(names, ["connect under reset", "clock 1000 kHz", "clock 100 kHz", "USB port power cycle"])
        names = [n for n, _ in target.steps(board(speed_khz=8000, connect_under_reset=False), "X")]
        self.assertEqual(names[:3], ["clock 4000 kHz", "clock 1000 kHz", "clock 100 kHz"])

    def test_under_reset_holds_nrst_and_starts_with_a_reset(self):
        target._SETTINGS["S1"] = {"under_reset": True}
        b = board(target_cfg="target/ti_mspm0.cfg", interface_cfg="interface/xds110.cfg")
        with mock.patch.object(target.toolfind, "find", return_value={"exe": "openocd", "scripts": str(ROOT)}), \
                mock.patch.object(target.Path, "is_file", return_value=True), \
                mock.patch.object(target.subprocess, "run") as sp:
            sp.return_value = mock.Mock(returncode=0, stdout="")
            target.run(b, "S1", ["mdw 0 1"])
            argv = sp.call_args[0][0]
            target.run(b, "S1", ["read"], halt_ok=False)
            argv2 = sp.call_args[0][0]
        self.assertIn("reset_config srst_only srst_nogate connect_assert_srst", argv)
        cs = [a for a in argv[argv.index("init") + 1:] if a != "-c"]
        self.assertEqual(cs[:4], ["adapter deassert srst", "sleep 10", "catch {[target current] arp_examine}", "halt 500"])
        self.assertEqual([a for a in argv2[argv2.index("init") + 1:] if a != "-c"][3], "sleep 100")

    def test_every_step_fails_and_the_stop_carries_the_record(self):
        seen, p = self.runs([(1, C1104_LOOP)] * 3)
        with p, self.assertRaises(core.Stop) as cm:
            code, out = target.attach(board(connect_under_reset=False, speed_khz=1000), "MC110001", ["mdw"], read_ok)
            target._attach_failure(out, "MC110001")
        data = cm.exception.result["data"]
        self.assertEqual([t["step"] for t in data["connect"]], ["as configured", "clock 100 kHz", "USB port power cycle"])
        self.assertEqual(data["connect"][-1]["ok"], None)
        self.assertIn("erases", cm.exception.result["next"])

    def test_a_busy_probe_never_climbs(self):
        seen, p = self.runs([(1, BUSY)])
        with p:
            target.attach(board(), "S2", ["mdw"], read_ok)
        self.assertEqual(len(seen), 1)

    def test_one_step_alone(self):
        seen, p = self.runs([(0, MSPM0_READ)])
        with p:
            target.attach(board(), "S3", ["mdw"], read_ok, start="clock 100 kHz")
        self.assertEqual(seen, [{"speed_khz": 100, "under_reset": False}])
        with self.assertRaises(core.Stop):
            target.attach(board(), "S3", ["mdw"], read_ok, start="mass erase")


class ProbeTools(unittest.TestCase):
    def test_xdsdfu_listing(self):
        d = target.parse_xdsdfu(XDSDFU_E)
        self.assertEqual(d[0], {"serial": "MC110001", "version": "3.0.0.43", "mode": "Runtime"})
        self.assertLess(target._ver(d[1]["version"]), target._ver(target.PROBE_FW_MIN))

    def test_firmware_below_the_minimum_is_named_in_the_stop(self):
        old = XDSDFU_E
        with mock.patch.object(target, "_xdsdfu", return_value=old):
            self.assertEqual(target.probe_firmware("LS5600RE"), {"version": "3.0.0.26", "minimum": "3.0.0.43", "below": True})
            self.assertFalse(target.probe_firmware("MC110001")["below"])
            target._LOG["LS5600RE"] = [{"step": "as configured", "ok": False, "words": []}]
            with self.assertRaises(core.Stop) as cm:
                target._attach_failure(C1104_LOOP, "LS5600RE")
        self.assertIn("below 3.0.0.43", cm.exception.result["next"])
        target._LOG.clear()

    def test_usb_cycle_only_on_a_port_with_a_measured_power_cut(self):
        rec = {"hubs": [{"location": "20-2", "ports": [{"port": 3, "board": "b1", "power_cut": "unknown"}]}]}
        with mock.patch("shutil.which", return_value="/usr/local/bin/uhubctl"):
            self.assertIsNone(target.usb_cycle({"id": "b1", "kit": "K"}, "S", recovery=rec))


class WriteReport(unittest.TestCase):
    def test_plain_crc_verify(self):
        r = target.write_report(WRITE_PLAIN)
        self.assertEqual((r["write_s"], r["verify_path"]), (3.889733, "crc"))

    def test_program_prints_only_verified_ok_when_the_crc_matches(self):
        self.assertEqual(target.write_report("** Programming Finished **\n** Verify Started **\n** Verified OK **\n")["verify_path"], "crc")

    def test_crc_failure_falls_back_to_binary_compare(self):
        r = target.write_report(WRITE_FALLBACK)
        self.assertEqual(r["verify_path"], "binary compare")
        self.assertIn("checksum mismatch - attempting binary compare", r["words"])


class DslitePath(unittest.TestCase):
    def route(self, bid):
        import json
        return json.loads((ROOT / "data" / "routes.json").read_text(encoding="utf-8"))["boards"][bid]

    def elf(self, segs):
        import struct
        import tempfile
        ph, data, off = b"", b"", 52 + 32 * len(segs)
        for addr, blob in segs:
            ph += struct.pack("<IIIIIIII", 1, off + len(data), addr, addr, len(blob), len(blob), 5, 4)
            data += blob
        hdr = b"\x7fELF\x01\x01\x01" + b"\x00" * 9 + struct.pack("<HHIIIIIHHHHHH", 2, 40, 1, 0, 52, 0, 0, 52, 32, len(segs), 0, 0, 0)
        p = Path(tempfile.mkdtemp(prefix="frl-dsl-")) / "a.out"
        p.write_bytes(hdr + ph + data)
        return str(p)

    def test_mspm0_erases_only_the_image_range(self):
        import dslite_path
        img = self.elf([(0x0, b"\x01" * 440), (0x800, b"\x02" * 16)])
        got = dslite_path.erase_settings(self.route("mspm0l1306"), img)
        self.assertIn("FlashEraseSelection=Erase MAIN memory sectors by range (specify below)", got)
        self.assertIn("FlashEraseSectorsStartAddress=0", got)
        self.assertIn("FlashEraseSectorsEndAddress=2063", got)

    def test_mspm33_range_counts_from_its_flash_alias(self):
        import dslite_path
        got = dslite_path.erase_settings(self.route("mspm33c321a"), self.elf([(0x10000000, b"\x01" * 800)]))
        self.assertEqual(got[1:], ["FlashEraseSectorsStartAddress=0", "FlashEraseSectorsEndAddress=799"])

    def test_no_route_erases_nonmain_or_everything(self):
        import json
        for bid, b in json.loads((ROOT / "data" / "routes.json").read_text(encoding="utf-8"))["boards"].items():
            for setting in b["target"].get("dslite_erase", []):
                self.assertNotIn("NONMAIN", setting, bid)
                self.assertNotIn("Entire Flash", setting, bid)

    def test_path_is_chosen_by_request_or_by_a_missing_tool(self):
        import dslite_path
        b = self.route("mspm0l1306")
        with mock.patch.dict("os.environ", {"FRL_WRITE_PATH": "dslite"}):
            self.assertTrue(dslite_path.chosen(b))
        with mock.patch.dict("os.environ", {}, clear=False), mock.patch("os.environ.get", return_value=None):
            with mock.patch.object(target.toolfind, "find", return_value={"exe": "openocd"}):
                self.assertFalse(dslite_path.chosen(b))

            def find(kind, *a, **k):
                if kind == "dslite":
                    return {"exe": "DSLite"}
                core.stop("refused", "FRL-E020", f"{kind} missing")
            with mock.patch.object(target.toolfind, "find", side_effect=find):
                self.assertTrue(dslite_path.chosen(b))
        self.assertFalse(dslite_path.chosen(self.route("cc2755p10")))


class DisplayMessage(unittest.TestCase):
    # LP-EM-CC2340R5 running TI's drivers/display with frl's message edit, read from the probe's UART (2026-10-24)
    RAW = (b"r\x1b[1;0H\x1b[2K\x1bE\x1b8> Hello from my LaunchPad 2\r\n[ 3 ] LED: \x1b[31;4mOff\x1b[0m\r\n\x1b7\x1b[10r"
           b"\x1b[2;0H\x1b[2K\x1bE\x1b8> Hello from my LaunchPad 4\r\n[ 5 ] LED: \x1b[32;1mOn\x1b[0m\r\n")

    def test_ansi_codes_do_not_hide_the_message(self):
        import verify
        self.assertTrue(verify.message_check(self.RAW, "Hello from my LaunchPad", "p")["ok"])
        bad = verify.message_check(self.RAW, "Other text", "p")
        self.assertFalse(bad["ok"])
        self.assertIn("> Hello from my LaunchPad 4", bad["message"])

    def test_display_edit_on_tis_example_text(self):
        import ops
        src = ("    unsigned int loopCount = 0;\n    sleep(6);\n        Display_printf(hSerial,\n"
               "                       ledPinValue ? 0 : 1, /* Line */\n                       0,\n"
               "                       \"LED: %s\",\n                       (ledPinValue == CONFIG_GPIO_LED_ON) ? serialLedOn : serialLedOff);\n")
        new, line = ops.display_message(src, "Hi there")
        self.assertIn('static const char frlMessage[] = "Hi there";', new)
        self.assertIn('"> %s %u", frlMessage, loopCount++', new)
        self.assertIn("sleep(1);", new)
        again, _ = ops.display_message(new, "Bye")
        self.assertEqual(again.count("frlMessage[] ="), 1)
        self.assertIsNone(ops.display_message("int main(void) {}", "x")[0])


if __name__ == "__main__":
    unittest.main()
