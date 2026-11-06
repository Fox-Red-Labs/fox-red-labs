"""The reader's tools on macOS, Linux and Windows: where frl looks for TI's tools, what it says when one is
missing, and the per-OS probe and serial-port messages. Windows is simulated with PureWindowsPath, since
no Windows machine runs these; .github/workflows/ci.yml in the reader copy runs them on windows-latest."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath, PureWindowsPath
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
import target  # noqa: E402
import toolfind  # noqa: E402

WIN_HOME = PureWindowsPath("C:/Profiles/Ana Lima")   # no Users folder here: the reader export rewrites home paths
LIN_HOME = PurePosixPath("/srv/u")
MAC_HOME = PurePosixPath("~")


class Roots(unittest.TestCase):
    def test_windows_defaults_are_c_ti_then_home(self):
        r = toolfind.default_roots("win32", WIN_HOME, {})
        self.assertEqual(r, ["C:\\ti", "C:\\Profiles\\Ana Lima\\ti"])

    def test_linux_defaults(self):
        self.assertEqual(toolfind.default_roots("linux", LIN_HOME, {}), ["/srv/u/ti", "/opt/ti", "/mnt/c/ti"])

    def test_mac_defaults(self):
        self.assertEqual(toolfind.default_roots("darwin", MAC_HOME, {}), ["/Applications/ti", "~/ti"])

    def test_override_comes_first_on_every_os(self):
        for plat, home in (("win32", WIN_HOME), ("linux", LIN_HOME), ("darwin", MAC_HOME)):
            r = toolfind.default_roots(plat, home, {"FRL_TI_ROOT": "D:\\tools\\ti" if plat == "win32" else "/data/ti"})
            self.assertIn(r[0], ("D:\\tools\\ti", "/data/ti"))

    def test_no_mac_path_off_the_mac(self):
        for plat, home in (("win32", WIN_HOME), ("linux", LIN_HOME)):
            self.assertFalse(any("Applications" in x for x in toolfind.default_roots(plat, home, {})))


class Patterns(unittest.TestCase):
    def roots(self):
        return [PureWindowsPath(r) for r in toolfind.default_roots("win32", WIN_HOME, {})]

    def test_windows_sdk_compiler_sysconfig(self):
        r = self.roots()
        self.assertIn("C:\\ti\\mspm0_sdk_*", toolfind.search_patterns("mspm0_sdk", r, WIN_HOME, "win32"))
        self.assertIn("C:\\ti\\ti-cgt-armllvm_*", toolfind.search_patterns("ticlang", r, WIN_HOME, "win32"))
        self.assertIn("C:\\ti\\ccs*\\ccs\\tools\\compiler\\ti-cgt-armllvm_*", toolfind.search_patterns("ticlang", r, WIN_HOME, "win32"))
        self.assertIn("C:\\ti\\sysconfig_*", toolfind.search_patterns("sysconfig", r, WIN_HOME, "win32"))

    def test_windows_openocd_where_the_readme_unpacks_it(self):
        p = toolfind.search_patterns("openocd", self.roots(), WIN_HOME, "win32")
        self.assertIn("C:\\Profiles\\Ana Lima\\xpack-openocd-*", p)
        self.assertFalse(any(x.startswith("/opt") for x in p))

    def test_windows_make_is_ccstudio_gmake(self):
        p = toolfind.search_patterns("make", self.roots(), WIN_HOME, "win32")
        self.assertIn("C:\\ti\\ccs*\\ccs\\utils\\bin\\gmake.exe", p)
        self.assertEqual(toolfind.search_patterns("make", [PurePosixPath("/srv/u/ti")], LIN_HOME, "linux"), [])

    def test_linux_openocd(self):
        p = toolfind.search_patterns("openocd", [PurePosixPath("/srv/u/ti")], LIN_HOME, "linux")
        self.assertIn("/srv/u/xpack-openocd-*", p)
        self.assertIn("/opt/xpack-openocd-*", p)

    def test_every_tool_kind_has_patterns_or_uses_path(self):
        for kind in toolfind.PROBE:
            pats = toolfind.search_patterns(kind, [PurePosixPath("/srv/u/ti")], LIN_HOME, "linux")
            self.assertTrue(pats or kind in ("make", "claude", "codex"), kind)


class XdsdfuDiscovery(unittest.TestCase):
    def test_installed_executable_is_discovered_on_each_os(self):
        """Exercise globbing as well as probing; a suffix fallback cannot repair a missed glob."""
        layouts = ("ccs2101/ccs/ccs_base/common/uscif/xds110",
                   "uniflash_9/deskdb/content/TICloudAgent/win/ccs_base/common/uscif/xds110")
        with tempfile.TemporaryDirectory(prefix="frl-toolfind-") as tmp:
            root = Path(tmp)
            for plat in ("win32", "linux", "darwin"):
                for layout in layouts:
                    with self.subTest(platform=plat, layout=layout):
                        install = root / plat / layout
                        install.mkdir(parents=True)
                        exe = install / ("xdsdfu.exe" if plat == "win32" else "xdsdfu")
                        exe.write_text("")
                        exe.chmod(0o755)
                        with patch.object(toolfind.sys, "platform", plat), \
                             patch.object(toolfind, "WIN", plat == "win32"), \
                             patch.object(toolfind, "ti_roots", return_value=[root / plat]):
                            found = toolfind.candidates("xdsdfu")
                        self.assertIn(str(exe), [c["exe"] for c in found])


class InstallLines(unittest.TestCase):
    def test_each_os_names_its_own_folder(self):
        self.assertIn("C:\\ti", toolfind.install_line("mspm0_sdk", "win32"))
        self.assertIn("~/ti", toolfind.install_line("mspm0_sdk", "linux"))
        self.assertIn("/Applications/ti", toolfind.install_line("mspm0_sdk", "darwin"))

    def test_openocd_and_make(self):
        self.assertIn("udev", toolfind.install_line("openocd", "linux"))
        self.assertIn("%USERPROFILE%", toolfind.install_line("openocd", "win32"))
        self.assertIn("xcode-select", toolfind.install_line("make", "darwin"))
        self.assertIn("apt install make", toolfind.install_line("make", "linux"))
        self.assertIn("gmake", toolfind.install_line("make", "win32"))


class ProbeAccess(unittest.TestCase):
    def test_linux_access_denied_names_the_udev_rule(self):
        n = target.denied_next("linux")
        self.assertIn("60-openocd.rules", n)
        self.assertIn("udevadm", n)

    def test_windows_access_names_the_driver(self):
        self.assertIn("UniFlash or CCStudio", target.denied_next("win32"))

    def test_denied_output_is_its_own_message(self):
        with self.assertRaises(Exception) as cm:
            target._attach_failure("Error: libusb_open() failed with LIBUSB_ERROR_ACCESS", "ML130001")
        r = cm.exception.result
        self.assertEqual(r["code"], "FRL-E006")
        self.assertIn("access denied", r["message"])


class WindowsComPorts(unittest.TestCase):
    UART = {"Name": "XDS110 Class Application/User UART (COM5)", "PNPDeviceID": "USB\\VID_0451&PID_BEF3&MI_00\\6&1A2B&0&0000"}
    AUX = {"Name": "XDS110 Class Auxiliary Data Port (COM6)", "PNPDeviceID": "USB\\VID_0451&PID_BEF3&MI_03\\6&1A2B&0&0003"}

    def test_one_probe_gives_its_uart(self):
        self.assertEqual(target.windows_com_ports("ML130001", [self.UART, self.AUX]), ["COM5"])

    def test_two_probes_without_serials_give_nothing(self):
        other = dict(self.UART, Name="XDS110 Class Application/User UART (COM9)")
        self.assertEqual(target.windows_com_ports("ML130001", [self.UART, other]), [])

    def test_serial_in_the_id_wins(self):
        mine = dict(self.UART, Name="XDS110 Class Application/User UART (COM9)", PNPDeviceID="USB\\X\\ML130001")
        self.assertEqual(target.windows_com_ports("ML130001", [self.UART, mine]), ["COM9"])

    def test_single_object_from_convertto_json(self):
        self.assertEqual(target.windows_com_ports("ML130001", self.UART), ["COM5"])


class Launcher(unittest.TestCase):
    def test_start_py_runs_without_bash(self):
        """The Windows command (py start.py ...) reaches the tool and reports a bad argument as FRL-E070."""
        p = subprocess.run([sys.executable, str(ROOT / "start.py"), "mspm0l1306", "blink", "--pattern", "1,x", "--dry-run"],
                           capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
        self.assertEqual(p.returncode, 2, p.stdout + p.stderr)
        self.assertIn("FRL-E070", p.stdout)

    def test_scripts_are_stored_with_lf(self):
        attrs = (ROOT / ".gitattributes").read_text()
        for line in ("*.sh    text eol=lf", "*.py    text eol=lf", "*.ps1   text eol=crlf"):
            self.assertIn(line, attrs)
        for f in ("start.sh", "start.py", "tools/run_tests.sh", "mac-setup/bootstrap.sh"):
            self.assertNotIn(b"\r\n", (ROOT / f).read_bytes(), f)


if __name__ == "__main__":
    unittest.main()
