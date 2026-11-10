"""Regression tests for the local setup tool (tools/frl) and the backup it calls, with fake hardware.

Each test builds a private FRL_HOME and a fake OpenOCD and xdsdfu (tools/tests/fixtures/frl), so nothing
touches a board, the real per-user data folder or the bench's settings. The cases are the blueprint's P0
items (reviews/2026-10-14-customer-experience-blueprint.md, CX01 to CX10).

  python3 -m unittest tools/tests/test_frl.py
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
FIX = Path(__file__).resolve().parent / "fixtures" / "frl"
FRL = [sys.executable, str(ROOT / "tools" / "frl" / "frl.py")]
START = [sys.executable, str(ROOT / "tools" / "frl" / "start.py")]
L1306 = "0x1BB8202F"


def elf(path, segs):
    """Write a minimal 32-bit little-endian ELF with PT_LOAD segments [(addr, bytes)]."""
    phoff, n = 52, len(segs)
    data_off = phoff + 32 * n
    hdr = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9) + struct.pack("<HHIIIIIHHHHHH", 2, 40, 1, 0, phoff, 0, 0, 52, 32, n, 40, 0, 0)
    ph, body, off = b"", b"", data_off
    for addr, blob in segs:
        ph += struct.pack("<IIIIIIII", 1, off, addr, addr, len(blob), len(blob), 5, 4)
        body += blob
        off += len(blob)
    Path(path).write_bytes(hdr + ph + body)
    return path


def validated_mspm0_tools():
    """True when the SDK, SysConfig and TI Arm Clang versions the routes were validated with are installed (toolenv exits 0)."""
    return subprocess.run([sys.executable, str(ROOT / "tools" / "frl" / "toolenv.py"), "mspm0"], capture_output=True).returncode == 0


class Env:
    def __init__(self, probes, boards):
        if os.name == "nt":   # the fake probe tools are POSIX scripts run by their shebang; on Windows frl is checked against real probes
            raise unittest.SkipTest("fake probe and OpenOCD tools are POSIX scripts (tools/tests/fixtures/frl); not run on Windows")
        self.tmp = Path(tempfile.mkdtemp(prefix="frl-test-"))
        self.fake = self.tmp / "fake"
        ocd = self.tmp / "ocd"
        (ocd / "bin").mkdir(parents=True)
        for sub in ("target", "interface"):
            (ocd / "openocd" / "scripts" / sub).mkdir(parents=True)
        (ocd / "openocd" / "scripts" / "target" / "ti_mspm0.cfg").write_text("")
        (ocd / "openocd" / "scripts" / "interface" / "xds110.cfg").write_text("")
        shutil.copy(FIX / "fake_openocd.py", ocd / "bin" / "openocd")
        shutil.copy(FIX / "fake_xdsdfu.py", self.tmp / "xdsdfu")
        self.fake.mkdir()
        (self.fake / "boards.json").write_text(json.dumps(boards))
        self.env = {k: v for k, v in os.environ.items() if not k.startswith(("FRL_", "PROBE", "OCD", "FAKE_", "XDSDFU"))}
        self.env.update(FRL_HOME=str(self.tmp / "home"), FAKE_DIR=str(self.fake), FRL_OPENOCD=str(ocd / "bin" / "openocd"),
                        OCD=str(ocd), FRL_XDSDFU=str(self.tmp / "xdsdfu"), XDSDFU=str(self.tmp / "xdsdfu"),
                        FAKE_PROBES=",".join(probes), FRL_PROJECTS=str(self.tmp / "projects"))

    def run(self, argv, extra=None, timeout=60):
        return subprocess.run(argv, capture_output=True, text=True, env={**self.env, **(extra or {})}, timeout=timeout, cwd=ROOT)

    def frl(self, *args, extra=None):
        p = self.run(FRL + ["--json", *args], extra)
        try:
            return p.returncode, json.loads(p.stdout)
        except ValueError:
            raise AssertionError(f"not JSON (exit {p.returncode}): {p.stdout[-800:]} {p.stderr[-800:]}")

    def calls(self):
        f = self.fake / "calls.jsonl"
        return [json.loads(x) for x in f.read_text().splitlines()] if f.exists() else []

    def close(self):
        shutil.rmtree(self.tmp, ignore_errors=True)


def board(traceid="0x0000026C", deviceid=L1306, pattern=None, busy=False):
    return {"traceid": traceid, "deviceid": deviceid, "pattern": pattern, "busy": busy}


class ProbeSelection(unittest.TestCase):          # CX01
    def setUp(self):
        self.e = Env(["AAA111", "BBB222"], {"AAA111": board(), "BBB222": board("0x00000999")})

    def tearDown(self):
        self.e.close()

    def test_two_probes_no_selection_stops_before_target(self):
        img = elf(self.e.tmp / "a.out", [(0, b"\x01" * 64)])
        code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img))
        self.assertEqual((code, r["code"]), (2, "FRL-E002"))
        self.assertEqual(self.e.calls(), [], "no OpenOCD call may happen before a probe is chosen")

    def test_selected_serial_used_for_every_call_whatever_the_order(self):
        img = elf(self.e.tmp / "a.out", [(0, b"\x01" * 64)])
        for order in (["AAA111", "BBB222"], ["BBB222", "AAA111"]):
            code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img), "--probe", "BBB222",
                                 extra={"FAKE_PROBES": ",".join(order), "PROBE": "AAA111"})
            self.assertEqual(code, 0, r)
        self.assertTrue(self.e.calls())
        self.assertEqual({c["serial"] for c in self.e.calls()}, {"BBB222"})

    def test_absent_serial_refused(self):
        code, r = self.e.frl("identify", "--board", "mspm0l1306", "--probe", "ZZZ999")
        self.assertEqual((code, r["code"]), (2, "FRL-E003"))

    def test_ocd_sh_has_no_bench_default(self):
        env = dict(self.e.env)
        p = subprocess.run(["bash", str(ROOT / "mspm0l1306" / "tools" / "ocd.sh"), "mdw 0 1"], capture_output=True, text=True,
                           env={**env, "HOME": str(self.e.tmp)}, cwd=self.e.tmp)
        # the bench's own lab_probes file may exist on the lab machine; elsewhere there must be no default
        if not (ROOT / "tools" / "benchmark" / "settings.local.json").exists():
            self.assertEqual(p.returncode, 3)
            self.assertIn("Set PROBE", p.stdout)
        self.assertNotIn("ML130001", (ROOT / "mspm0l1306" / "tools" / "ocd.sh").read_text())


class Backup(unittest.TestCase):                  # CX02, CX03
    def setUp(self):
        self.e = Env(["STRANGER123", "OTHER456"], {"STRANGER123": board("0x00000111"), "OTHER456": board("0x00000222")})
        (self.e.fake / "flash-STRANGER123.bin").write_bytes(bytes(range(256)) * 256)

    def tearDown(self):
        self.e.close()

    def save(self):
        code, r = self.e.frl("backup", "--board", "mspm0l1306", "--probe", "STRANGER123", extra={"PROBE": "OTHER456"})
        self.assertEqual(code, 0, r)
        return Path(r["data"]["backup"])

    def test_backup_reads_only_the_selected_probe(self):
        folder = self.save()
        man = json.loads((folder / "backup.json").read_text())
        self.assertEqual(man["probe"], "STRANGER123")
        self.assertEqual(man["identity"]["traceid"], "0x00000111")
        reads = [c for c in self.e.calls() if any("dump_image" in x for x in c["cmds"])]
        self.assertEqual({c["serial"] for c in reads}, {"STRANGER123"})
        self.assertEqual({c["PROBE_env"] for c in reads}, {"STRANGER123"})
        self.assertFalse(str(folder).startswith(str(ROOT)), "backups live outside the clone")

    def test_restore_to_same_stranger_device(self):
        folder = self.save()
        (self.e.fake / "flash-STRANGER123.bin").write_bytes(bytes(65536))
        code, r = self.e.frl("restore", str(folder), "--probe", "STRANGER123")
        self.assertEqual(code, 0, r)
        self.assertEqual((self.e.fake / "flash-STRANGER123.bin").read_bytes(), bytes(range(256)) * 256)

    def test_substituted_device_refused(self):
        folder = self.save()
        boards = json.loads((self.e.fake / "boards.json").read_text())
        boards["STRANGER123"]["traceid"] = "0x00000333"    # same serial, another chip behind it
        (self.e.fake / "boards.json").write_text(json.dumps(boards))
        n = len(self.e.calls())
        code, r = self.e.frl("restore", str(folder), "--probe", "STRANGER123")
        self.assertEqual((code, r["code"]), (2, "FRL-E051"), r)
        self.assertFalse(any("program" in " ".join(c["cmds"]) for c in self.e.calls()[n:]))

    def test_other_probe_refused(self):
        folder = self.save()
        code, r = self.e.frl("restore", str(folder), "--probe", "OTHER456")
        self.assertEqual(code, 2, r)
        self.assertEqual(r["code"], "FRL-E051")

    def test_edited_manifest_refused(self):
        folder = self.save()
        man = json.loads((folder / "backup.json").read_text())
        man["regions"][0]["restore"] = True
        man["made"] = "2000-01-01T00:00:00"
        (folder / "backup.json").write_text(json.dumps(man))
        code, r = self.e.frl("restore", str(folder), "--probe", "STRANGER123")
        self.assertEqual((code, r["code"]), (2, "FRL-E052"), r)

    def test_corrupted_image_refused(self):
        folder = self.save()
        f = folder / "main-flash.bin"
        os.chmod(f, 0o600)
        f.write_bytes(b"\xff" + f.read_bytes()[1:])
        code, r = self.e.frl("restore", str(folder), "--probe", "STRANGER123")
        self.assertEqual(code, 2, r)
        self.assertEqual(r["code"], "FRL-E052")


class Locks(unittest.TestCase):                   # CX04
    def setUp(self):
        self.e = Env(["AAA111"], {"AAA111": board(pattern=[150, 150, 150, 900])})

    def tearDown(self):
        self.e.close()

    def test_second_run_cannot_attach(self):
        p = subprocess.Popen([sys.executable, str(ROOT / "tools" / "frl" / "lock.py"), "exec", "--serial", "AAA111", "--op", "holder",
                              "--", sys.executable, "-c", "import time; time.sleep(4)"], env=self.e.env)
        time.sleep(1.0)
        code, r = self.e.frl("verify", "--board", "mspm0l1306", "--probe", "AAA111")
        p.wait()
        self.assertEqual((code, r["code"]), (2, "FRL-E004"), r)
        self.assertEqual(r["data"]["owner"]["op"], "holder")
        code, r = self.e.frl("verify", "--board", "mspm0l1306", "--probe", "AAA111")
        self.assertEqual(code, 0, r)

    def test_lock_is_shared_across_clones(self):
        a = self.e.run([sys.executable, "-c", "import sys; sys.path.insert(0,'tools/frl'); import core; print(core.data_dir())"])
        b = self.e.run([sys.executable, "-c", "import sys; sys.path.insert(0,'tools/frl'); import core; print(core.data_dir())"],
                       extra={"PWD": "/"})
        self.assertEqual(a.stdout, b.stdout)
        self.assertIn(self.e.env["FRL_HOME"], a.stdout)

    def test_crash_during_write_is_reported(self):
        locks = Path(self.e.env["FRL_HOME"]) / "locks"
        locks.mkdir(parents=True)
        (locks / "AAA111.owner.json").write_text(json.dumps({"pid": 1, "op": "flash", "state": "writing", "run_id": "old"}))
        img = elf(self.e.tmp / "a.out", [(0, b"\x01" * 64)])
        code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img), "--probe", "AAA111")
        self.assertEqual(code, 0, r)
        self.assertIn("stopped while writing", r["data"]["warning"])


class Runner(unittest.TestCase):                  # CX05, CX06, CX09
    def setUp(self):
        self.e = Env(["AAA111"], {"AAA111": board(pattern=[500, 500])})
        bindir = self.e.tmp / "bin"
        bindir.mkdir()
        self.claude = bindir / "claude"

    def tearDown(self):
        self.e.close()

    def fake_claude(self, code=0, sleep=0):
        self.claude.write_text(f"#!/bin/sh\nprintf '%s' \"$3\" > \"$FAKE_DIR/prompt-seen.txt\"\nsleep {sleep}\n"
                               f"echo '{{\"result\": \"All done, it blinks!\"}}'\nexit {code}\n")
        self.claude.chmod(0o755)
        return {"FRL_CLAUDE": str(self.claude)}

    def test_job_text_round_trips_as_data(self):
        job = "make it say it's \"SOS\"\n$(touch /tmp/frl-pwned) é — `id`"
        p = self.e.run(START + ["mspm0l1306", job, "--probe", "AAA111", "--assistant", "claude", "--dry-run"], self.fake_claude())
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn(job, p.stdout)
        self.assertFalse(Path("/tmp/frl-pwned").exists())

    def test_missing_option_value_fails_before_work(self):
        p = self.e.run(START + ["mspm0l1306", "blink", "--probe"])
        self.assertEqual(p.returncode, 2)
        self.assertEqual(self.e.calls(), [])

    @unittest.skipUnless(validated_mspm0_tools(), "needs the validated MSPM0 tools (SysConfig 1.26.3); this test builds the blink for real")
    def test_assistant_success_but_check_fails_is_a_failure(self):
        # the board still runs a 500/500 blink; the assistant claims success
        p = self.e.run(START + ["mspm0l1306", "blink", "--probe", "AAA111", "--assistant", "claude"], self.fake_claude(0))
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn("FRL-E040", p.stdout)
        self.assertIn("All done", p.stdout)   # shown, labelled as the assistant's words

    def test_no_checker_is_not_verified(self):
        p = self.e.run(START + ["mspm0l1306", "speed-test", "--probe", "AAA111", "--assistant", "claude"], self.fake_claude(0))
        self.assertEqual(p.returncode, 3, p.stdout + p.stderr)
        self.assertIn("FRL-E041", p.stdout)

    def test_assistant_timeout_is_a_failure(self):
        p = self.e.run(START + ["mspm0l1306", "speed-test", "--probe", "AAA111", "--assistant", "claude", "--timeout", "0.02"],
                       self.fake_claude(0, sleep=5))
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn("FRL-E061", p.stdout)

    def test_unsupported_project_refused_before_model_call(self):
        p = self.e.run(START + ["mspm0l1306", "matter-device", "--probe", "AAA111", "--assistant", "claude"], self.fake_claude())
        self.assertEqual(p.returncode, 2)
        self.assertIn("FRL-E011", p.stdout)
        self.assertFalse((self.e.fake / "prompt-seen.txt").exists())


class Checks(unittest.TestCase):                  # CX07
    def test_factory_blink_fails_changed_pattern(self):
        sys.path.insert(0, str(ROOT / "tools" / "frl"))
        import verify as v
        trans = [(0, 1)] + [(500 * i, i % 2) for i in range(1, 9)]
        r = v.pattern_check({"transitions": trans, "samples": 16000, "span_ms": 4000, "output_enabled": 1}, [150, 150, 150, 900], 40, "LED")
        self.assertEqual(r["code"], "FRL-E040")

    def test_pattern_matches_any_phase(self):
        import verify as v
        pat = [150, 150, 150, 900]
        t, trans, lvl, i = 70, [(0, 0)], 1, 2
        while t < 4000:
            trans.append((t, lvl))
            t += pat[i % 4]
            i += 1
            lvl = 1 - lvl
        r = v.pattern_check({"transitions": trans, "samples": 16000, "span_ms": 4000, "output_enabled": 1}, pat, 40, "LED")
        self.assertTrue(r["ok"], r)

    def test_slow_sampling_is_inconclusive(self):
        import verify as v
        r = v.pattern_check({"transitions": [(0, 1)], "samples": 50, "span_ms": 4000, "output_enabled": 1}, [150, 150], 40, "LED")
        self.assertEqual((r["outcome"], r["code"]), ("unverified", "FRL-E042"))

    def test_message_check(self):
        import verify as v
        good = b"\r\n==== MSPM0 Console Test ====\r\n\x00> Hi there 0\r\n\x00> Hi there 1\r\n\x00"
        self.assertTrue(v.message_check(good, "Hi there", "port")["ok"])
        self.assertEqual(v.message_check(b"> Testing...1\r\n> Testing...2\r\n", "Hi there", "port")["code"], "FRL-E040")
        self.assertEqual(v.message_check(b"> Hi there 0\r\n", "Hi there", "port")["code"], "FRL-E040")   # once is not counting
        self.assertEqual(v.message_check(b"", "Hi there", "port")["code"], "FRL-E040")
        for bad in ("", "x" * 41, 'say "hi"', "back\\slash", "tab\there"):
            self.assertIsNone(v.parse_message(bad)[0], bad)

    def test_bad_patterns_refused(self):
        import verify as v
        for bad in ("150", "150,abc", "10,10", "6000,10", "1,2,3"):
            self.assertIsNone(v.parse_pattern(bad)[0], bad)


class Writes(unittest.TestCase):                  # write boundary, wrong board (CX01, CX10 groundwork)
    def setUp(self):
        self.e = Env(["AAA111", "G35"], {"AAA111": board(), "G35": board(deviceid="0x1BB8802F")})

    def tearDown(self):
        self.e.close()

    def test_image_outside_main_flash_refused(self):
        img = elf(self.e.tmp / "nonmain.out", [(0x41C00000, b"\x00" * 16)])
        code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img), "--probe", "AAA111")
        self.assertEqual((code, r["code"]), (2, "FRL-E034"))
        self.assertEqual(self.e.calls(), [])

    def test_wrong_model_refused_before_write(self):
        img = elf(self.e.tmp / "a.out", [(0, b"\x01" * 64)])
        code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img), "--probe", "G35")
        self.assertEqual((code, r["code"]), (2, "FRL-E005"))
        self.assertFalse(any("program" in " ".join(c["cmds"]) for c in self.e.calls()))

    def test_interrupted_write_is_unknown_not_failed_over(self):
        img = elf(self.e.tmp / "a.out", [(0, b"\x01" * 64)])
        code, r = self.e.frl("flash", "--board", "mspm0l1306", "--image", str(img), "--probe", "AAA111", extra={"FAKE_PROGRAM_HANG": "1"})
        self.assertEqual((code, r["code"]), (1, "FRL-E033"))
        self.assertIn("verify-image", r["next"])


class Journal(unittest.TestCase):                 # CX10, CX18, CX28
    def test_runs_are_separate_and_resume_refuses_after_interrupted_write(self):
        e = Env(["AAA111"], {"AAA111": board()})
        try:
            code = (f"import sys; sys.path.insert(0,'tools/frl'); import journal; "
                    f"a=journal.Run.new(board='mspm0l1306', recipe='blink-pattern'); b=journal.Run.new(board='mspm0l1306', recipe='blink-pattern'); "
                    f"a.set('programming', probe='AAA111', image='x.out'); print(a.id, b.id)")
            p = e.run([sys.executable, "-c", code])
            a, b = p.stdout.split()
            self.assertNotEqual(a, b)
            self.assertTrue((Path(e.env["FRL_HOME"]) / "runs" / a / "journal.jsonl").exists())
            st = json.loads((Path(e.env["FRL_HOME"]) / "runs" / a / "state.json").read_text())
            self.assertIsNone(st["timing"]["hands_on_s"], "unknown hands-on time is null, never zero")
            code2, r = e.frl("run", "--board", "mspm0l1306", "--resume", a, "--probe", "AAA111")
            self.assertEqual((code2, r["code"]), (1, "FRL-E033"))
            p = e.run(FRL + ["status", a, "--handoff"])
            self.assertIn("interrupted", p.stdout)
            self.assertIn("verify-image", p.stdout)
        finally:
            e.close()


class Doctor(unittest.TestCase):                  # CX08, CX13
    def test_no_assistant_route_needs_no_assistant_and_dir_without_exe_is_missing(self):
        e = Env(["AAA111"], {"AAA111": board()})
        try:
            fake_cfg = e.tmp / "sysconfig_1.26.3"
            fake_cfg.mkdir()          # the folder is there, the executable is not
            code, r = e.frl("doctor", "--board", "mspm0l1306", extra={"FRL_SYSCONFIG": str(fake_cfg), "FRL_CLAUDE": "/nonexistent"})
            self.assertEqual(code, 2)
            self.assertIn("sysconfig", r["message"])
            self.assertNotIn("claude", json.dumps(r["data"]["tools"]))
            code, r = e.frl("doctor", "--board", "mspm0l1306", "--route", "claude", extra={"FRL_CLAUDE": "/nonexistent"})
            self.assertEqual(code, 2)
        finally:
            e.close()

    def test_unvalidated_only_stops_with_choices(self):
        sys.path.insert(0, str(ROOT / "tools" / "frl"))
        import toolfind
        from core import Stop
        orig = toolfind.candidates
        toolfind.candidates = lambda kind: [{"path": "/x/sysconfig_1.28.0/sysconfig_cli.sh", "exe": "/x", "version": "1.28.0"}]
        old = {k: os.environ.pop(k) for k in list(os.environ) if k in ("FRL_SYSCONFIG",)}
        try:
            with self.assertRaises(Stop) as cm:
                toolfind.find("sysconfig", ["1.26.3"])
            self.assertEqual(cm.exception.result["code"], "FRL-E021")
            toolfind.candidates = lambda kind: [{"path": "/a", "exe": "/a", "version": "1.26.3"}, {"path": "/b", "exe": "/b", "version": "1.26.3"}]
            with self.assertRaises(Stop) as cm:
                toolfind.find("sysconfig", ["1.26.3"])
            self.assertEqual(cm.exception.result["code"], "FRL-E022")
        finally:
            toolfind.candidates = orig
            os.environ.update(old)


if __name__ == "__main__":
    unittest.main()


class StarterClosure(unittest.TestCase):         # CX14
    def test_route_runs_from_only_the_starter_files(self):
        """Copy exactly tools/frl/starter-files.txt into an empty folder and run the whole route there.
        The build is real when TI's tools are installed; the probe is the fake one."""
        e = Env(["AAA111"], {"AAA111": board()})
        try:
            dest = e.tmp / "a folder with spaces" / "fox-red-labs"
            for line in (ROOT / "tools" / "frl" / "starter-files.txt").read_text().splitlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                src = ROOT / line.strip("/")
                tgt = dest / line.strip("/")
                if not src.exists():   # a notebook-only file the reader copy leaves out
                    continue
                if src.is_dir():
                    shutil.copytree(src, tgt, ignore=shutil.ignore_patterns("__pycache__"))
                else:
                    tgt.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(src, tgt)
            frl = [sys.executable, str(dest / "tools" / "frl" / "frl.py"), "--json"]
            p = subprocess.run(frl + ["doctor", "--board", "mspm0l1306"], capture_output=True, text=True, env=e.env, cwd=e.tmp)
            r = json.loads(p.stdout)
            if r["code"] in ("FRL-E020", "FRL-E021", "FRL-E022"):
                self.skipTest("TI's tools are not installed here: " + r["message"])
            p = subprocess.run(frl + ["run", "--board", "mspm0l1306", "--probe", "AAA111", "--pattern", "150,150,150,900"],
                               capture_output=True, text=True, cwd=e.tmp, timeout=600,
                               env={**e.env, "FAKE_IMAGE_PATTERN": "[150, 150, 150, 900]"})
            r = json.loads(p.stdout)
            self.assertEqual(r["code"], "FRL-OK", r["message"] + p.stderr[-800:])
            self.assertTrue(r["data"]["project"].startswith(e.env["FRL_PROJECTS"]))
        finally:
            e.close()

    def test_every_error_code_has_a_faq_entry(self):
        from core import ERRORS
        text = (ROOT / "docs" / "frl-errors.md").read_text()
        for code in ERRORS:
            if code != "FRL-OK":
                self.assertIn(f'<a id="{code.lower()}"></a>', text)
                self.assertIn(f"## {code}: ", text)

    def test_start_sh_from_the_start_md_sparse_clone(self):
        """START.md's clone holds only top-level files; start.sh must add the tool's folders, then list the
        missing tools before looking for a board (found 2026-10-14: it stopped with "can't open file")."""
        if not shutil.which("git"):
            self.skipTest("git not installed")
        if os.name == "nt":
            self.skipTest("start.sh is the macOS and Linux entry; on Windows the reader runs py start.py, which the dry-run step covers")
        tmp = Path(tempfile.mkdtemp())
        try:
            src = tmp / "src"
            for line in (ROOT / "tools" / "frl" / "starter-files.txt").read_text().splitlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                s, t = ROOT / line.strip("/"), src / line.strip("/")
                if not s.exists():   # a notebook-only file the reader copy leaves out (docs/frl-desktop-apps.md)
                    continue
                if s.is_dir():
                    shutil.copytree(s, t, ignore=shutil.ignore_patterns("__pycache__"))
                else:
                    t.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(s, t)
            g = ["git", "-c", "user.name=t", "-c", "user.email=t@example.com", "-c", "init.defaultBranch=main"]
            subprocess.run(g + ["init", "-q"], cwd=src, check=True)
            subprocess.run(g + ["add", "-A"], cwd=src, check=True)
            subprocess.run(g + ["commit", "-qm", "starter"], cwd=src, check=True)
            subprocess.run(["git", "clone", "-q", "--sparse", src.as_uri(), "clone"], cwd=tmp, check=True)
            clone = tmp / "clone"
            self.assertFalse((clone / "tools" / "frl").exists())
            env = {k: v for k, v in os.environ.items() if not k.startswith("FRL_")}
            env.update(FRL_HOME=str(tmp / "home"), FRL_MSPM0_SDK=str(tmp / "none"))
            p = subprocess.run(["bash", str(clone / "start.sh"), "mspm0l1306", "blink"], capture_output=True,
                               text=True, cwd=clone, env=env, stdin=subprocess.DEVNULL, timeout=120)
            self.assertIn("Adding the folders the tool needs", p.stdout)
            self.assertTrue((clone / "tools" / "frl" / "start.py").exists())
            self.assertTrue((clone / "mspm0l1306" / "tools" / "ocd.sh").exists())
            self.assertIn("FRL-E020", p.stdout, p.stdout + p.stderr)    # tools first, before any probe search
            self.assertIn("Together the four tools are", p.stdout)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class Guide(unittest.TestCase):                   # CX31
    def test_declining_writes_nothing(self):
        e = Env(["AAA111"], {"AAA111": board()})
        try:
            p = subprocess.run([sys.executable, str(ROOT / "tools" / "frl" / "guide.py")], input="", capture_output=True,
                               text=True, env=e.env, cwd=ROOT, timeout=60)   # no answers at all: stops, writes nothing
            self.assertNotEqual(p.returncode, 0)
            # answers: a board in hand, the first kit, return after plugging in, the first job (the L1306 offers two since
            # its print-message route became eligible), the first pattern, then "No, stop here"
            p = subprocess.run([sys.executable, str(ROOT / "tools" / "frl" / "guide.py")], input="1\n1\n\n\n\n2\n",
                               capture_output=True, text=True, env=e.env, cwd=ROOT, timeout=120)
            if "FRL-E02" in p.stdout:
                self.skipTest("TI's tools are not installed here")
            self.assertIn("Stopped. Nothing was written.", p.stdout)
            self.assertFalse(any("program" in " ".join(c["cmds"]) for c in e.calls()))
        finally:
            e.close()


class NondefaultPaths(unittest.TestCase):         # CX13
    def test_build_with_every_tool_at_a_nondefault_path(self):
        """Point FRL_* at tools under an unusual folder (symlinks to the real installs) and build."""
        e = Env(["AAA111"], {"AAA111": board()})
        try:
            sys.path.insert(0, str(ROOT / "tools" / "frl"))
            import toolfind
            from core import Stop
            try:
                real = {k: toolfind.find(kind, val) for k, kind, val in
                        (("sdk", "mspm0_sdk", ["2.11.00.07"]), ("sc", "sysconfig", ["1.26.3"]), ("cc", "ticlang", ["5.1.1.LTS"]))}
            except Stop:
                self.skipTest("TI's tools are not installed here")
            odd = e.tmp / "odd-place"
            odd.mkdir()
            (odd / "mspm0_sdk_2_11_00_07").symlink_to(real["sdk"]["path"])
            (odd / "sysconfig_1.26.3").symlink_to(Path(real["sc"]["path"]).parent)
            (odd / "ti-cgt-armllvm_5.1.1.LTS").symlink_to(real["cc"]["path"])
            env = {"FRL_MSPM0_SDK": str(odd / "mspm0_sdk_2_11_00_07"), "FRL_SYSCONFIG": str(odd / "sysconfig_1.26.3"),
                   "FRL_TICLANG": str(odd / "ti-cgt-armllvm_5.1.1.LTS"), "TI_ROOT_OVERRIDE": str(odd)}
            code, r = e.frl("prepare", "--board", "mspm0l1306", extra=env)
            self.assertEqual(code, 0, r)
            code, r = e.frl("build", "--board", "mspm0l1306", extra=env)
            self.assertEqual(code, 0, r)
            self.assertEqual({v["source"] for v in r["data"]["tools"].values() if v["source"] == "override"}, {"override"})
        finally:
            e.close()


class Reports(unittest.TestCase):                 # CX30
    def test_report_becomes_a_scrubbed_case_with_consent(self):
        sys.path.insert(0, str(ROOT / "tools" / "community"))
        import report_to_case as rc
        row = {"id": "issue-9", "date": "2026-11-01", "kind": "broke", "board": "mspm0l1306", "project": "blink",
               "os": "macOS 15 Apple Silicon", "tools": "SDK 2.11.00.07", "step": "flash",
               "error": "FRL-E004 REFUSED: probe XY123456 is in use; log ~/run.txt from 203.0.113.20 on /dev/cu.usbmodemXY1234561",
               "text": "ran ./start.sh --probe XY123456", "status": "reported", "link": "https://example.invalid/9", "credit": ""}
        case = rc.case_for(row)
        blob = json.dumps(case)
        for secret in ("alice", "203.0.113.20", "XY123456"):
            self.assertNotIn(secret, blob)
        self.assertEqual(case["frl_code"], "FRL-E004")
        self.assertEqual(case["faq"], "docs/frl-errors.md#frl-e004")
        self.assertTrue(case["consent"]["name"].startswith("no"))
        self.assertEqual(case["config"]["os"], "macOS 15 Apple Silicon")
        self.assertIn("Not reproduced", rc.faq_draft(case))


class RadioParts(unittest.TestCase):              # CC23xx and CC27xx: identity by flash probe, write blocked, driver-style pattern
    def route(self, bid):
        return json.loads((ROOT / "data" / "routes.json").read_text())["boards"][bid]

    def test_flash_probe_line_parsed(self):
        import target
        out = "Info : DEVICESTATUS: x\nInfo : Device: CC2340R52E0RKPR, Flash: 512kb, RAM: 36kb\nInfo : Exit SACI and Halt\n"
        self.assertEqual(target.parse_flash_probe(out), {"part": "CC2340R52E0RKPR", "flash_kb": "512", "ram_kb": "36"})
        self.assertIsNone(target.parse_flash_probe("Error: Error connecting DP: cannot read IDR"))

    def test_part_checked_by_prefix(self):
        import target
        from core import Stop
        b = self.route("cc2340r5")
        real = target.identity
        try:
            target.identity = lambda board, serial: {"part": "CC2340R52E0RKPR", "flash_kb": "512", "ram_kb": "36"}
            self.assertEqual(target.check_identity(b, "X1")["part"], "CC2340R52E0RKPR")
            target.identity = lambda board, serial: {"part": "CC2755P10", "flash_kb": "1024", "ram_kb": "256"}
            with self.assertRaises(Stop) as cm:
                target.check_identity(b, "X1")
            self.assertEqual(cm.exception.result["code"], "FRL-E005")
        finally:
            target.identity = real

    def test_deviceid_any(self):
        import target
        from core import Stop
        b = {"kit": "K", "printed": "p", "target": {"identity": {"deviceid": "0x0BBE002F", "deviceid_any": ["0x0000002F"], "deviceid_mask": "0x0FFFFFFF"}}}
        real = target.identity
        try:
            for dev in ("0x0BBE002F", "0x1BBE002F", "0x0000002F"):
                target.identity = lambda board, serial, d=dev: {"deviceid": d}
                self.assertEqual(target.check_identity(b, "X1")["deviceid"], dev)
            target.identity = lambda board, serial: {"deviceid": "0x0BB8202F"}
            with self.assertRaises(Stop) as cm:
                target.check_identity(b, "X1")
            self.assertIn("0x0BBE002F or 0x0000002F", cm.exception.result["message"])
        finally:
            target.identity = real

    def test_write_blocked_before_probe_is_opened(self):
        import target
        from core import Stop
        img = elf(Path(tempfile.mkdtemp(prefix="frl-radio-")) / "a.out", [(0, b"\x01" * 64)])
        blocked = json.loads(json.dumps(self.route("cc2755p10")))
        blocked["target"].pop("write", None)
        blocked["target"]["write_blocked"] = "the flash driver would erase more than main flash"
        with self.assertRaises(Stop) as cm:
            target.program(blocked, "X1", str(img))
        self.assertEqual(cm.exception.result["code"], "FRL-E035")

    def test_cc2755p10_writes_through_dslite_and_needs_an_scfg(self):
        """The CC2755P10 route writes with DSLite's chip erase, which clears the SCFG: an image without one is refused."""
        import lpf3
        import target
        from core import Stop
        from unittest import mock
        d = Path(tempfile.mkdtemp(prefix="frl-cc27-"))
        board = self.route("cc2755p10")
        self.assertEqual(board["target"]["write"], "dslite_lpf3")
        bare = elf(d / "bare.out", [(0, b"\x01" * 64), (0x4E020000, b"\x02" * 64)])
        with mock.patch.object(lpf3, "dslite", side_effect=AssertionError("must refuse before DSLite")):
            with self.assertRaises(Stop) as cm:
                target.program(board, "X1", str(bare))
        self.assertEqual(cm.exception.result["code"], "FRL-E036")
        full = elf(d / "full.out", [(0, b"\x01" * 64), (0x4E020000, b"\x02" * 64), (0x4E040000, b"\x03" * 64)])
        with mock.patch.object(lpf3, "dslite", return_value=(0, "Success")) as ds, \
                mock.patch.object(target, "run", side_effect=AssertionError("OpenOCD must not write")):
            target.program(board, "LS5600RE", str(full))
        argv = [str(a) for a in ds.call_args[0][0]]
        self.assertNotIn("FlashEraseSetting=Chip Erase", argv)      # the CC27xx loader has no such property
        self.assertIn("RetainMainSectors=true", argv)
        self.assertIn("LS5600RE", Path(argv[argv.index("-c") + 1]).read_text())

    def test_cc2340r5_writes_through_dslite_chip_erase(self):
        """TI's own tools chip-erase these parts; frl uses DSLite that way and never TI's OpenOCD write."""
        import lpf3
        import target
        from unittest import mock
        img = elf(Path(tempfile.mkdtemp(prefix="frl-radio-")) / "a.out", [(0, b"\x01" * 64)])
        with mock.patch.object(lpf3, "dslite", return_value=(0, "Success")) as ds, \
                mock.patch.object(target, "run", side_effect=AssertionError("OpenOCD must not write")):
            target.program(self.route("cc2340r5"), "X1", str(img))
        argv = [str(a) for a in ds.call_args[0][0]]
        self.assertIn("FlashEraseSetting=Chip Erase", argv)
        self.assertIn("X1", Path(argv[argv.index("-c") + 1]).read_text())
        regions = [(0x0, b"\x02" * 20), (0x4E020000, b"\x03" * 4)]
        hexed = lpf3.ihex(regions)
        self.assertIn(":020000044E02AA", hexed)
        self.assertTrue(hexed.endswith(":00000001FF\n"))

    def test_driver_pattern_template(self):
        import ops
        r = self.route("cc2340r5")["recipes"]["blink-pattern"]
        text = ops.driver_main(r)
        self.assertIn("GPIO_write(CONFIG_GPIO_LED_0, (i % 2 == 0) ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF);", text)
        self.assertIn("usleep(pattern_ms[i] * 1000u);", text)
        self.assertNotIn("DL_GPIO", text)

    def test_mspm0_pattern_text_unchanged(self):
        import ops
        r = {"led": "x"}
        self.assertEqual(ops.pattern_main(r), ops.PATTERN_MAIN)


class TextEncoding(unittest.TestCase):
    """Every text read and write on the reader path names utf-8 (2026-10-24): on Windows the default is cp1252, and
    the dry run died decoding site/data/projects.json on the test repository's Windows runner."""

    def test_reader_path_reads_and_writes_name_utf8(self):
        import re
        root = Path(__file__).resolve().parents[2]
        bad = []
        for f in [root / "start.py"] + sorted((root / "tools" / "frl").glob("*.py")):
            for n, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
                if re.search(r"\.read_text\(\)|\.write_text\([^()]*\)\s*$|\bopen\([^()]*\"(a\+?|w|r)\"\)|\.open\(\) as ", line) and "encoding=" not in line:
                    bad.append(f"{f.relative_to(root)}:{n}")
        self.assertEqual(bad, [])
