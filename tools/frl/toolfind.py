"""Find the tools a route needs, by discovery plus explicit overrides, and pick a version on purpose.

Order for each tool: an environment override (FRL_<KIND>, plus OCD for OpenOCD, which the bench scripts
already use), then <data dir>/tools.json, then TI's default install roots and PATH. A folder that exists
without its executable does not count. When nothing was overridden, the route's validated version wins;
several validated copies, or only unvalidated ones, stop with the exact choices instead of taking the
first folder in alphabetical order.
"""
import glob
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from core import data_dir, stop

WIN = os.name == "nt"
HOME = Path.home()
DOWNLOAD = {
    "mspm0_sdk": "MSPM0 SDK 2.11.00.07 from https://www.ti.com/tool/MSPM0-SDK (free, myTI login; 0.4 GB download, 2.2 GB installed, measured in mac-setup/README.md)",
    "mspm33_sdk": "MSPM33 SDK 1.04.00.00 from https://www.ti.com/tool/MSPM33-SDK (free)",
    "am13_sdk": "AM13E230x SDK 26.01.00.03, linked from the LP-AM13E230 page https://www.ti.com/tool/LP-AM13E230 (free)",
    "wifi_sdk": "SimpleLink Wi-Fi SDK 10.20.00.39 from https://www.ti.com/tool/SIMPLELINK-WIFI-SDK (free, myTI login)",
    "wifi_toolbox": "SimpleLink Wi-Fi Toolbox 4.3.23 from https://www.ti.com/tool/SIMPLELINK-WIFI-TOOLBOX (free, myTI login): it signs and programs CC35xx images",
    "f2_sdk": "SimpleLink CC13xx CC26xx SDK 8.33.00.16 from https://www.ti.com/tool/SIMPLELINK-CC13XX-CC26XX-SDK (free, myTI login)",
    "f3_sdk": "SimpleLink Low Power F3 SDK 9.21.00.36 from https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK (free, myTI login; 0.55 GB download, 2.0 GB installed)",
    "sysconfig": "SysConfig from https://www.ti.com/tool/SYSCONFIG (free; about 0.15 GB download, 0.96 GB installed): the MSPM0 routes were validated with 1.26.3, the AM13 and MSPM33 builds with 1.28.0",
    "ticlang": "TI Arm Clang 5.1.1.LTS from https://www.ti.com/tool/ARM-CGT (free; also inside CCStudio 21.0.1); Apple Silicon and Linux builds exist",
    "openocd": "xPack OpenOCD 0.12.0-7 (it reports 0.12.0+dev-02228; a newer release is used only if you name it with FRL_OPENOCD) from https://github.com/xpack-dev-tools/openocd-xpack/releases (the first with target/ti_mspm0.cfg; about 10 MB, no login)",
    "ti_openocd": "TI's OpenOCD (CCSTUDIO-OPENOCD 1.5.0, built from github.com/TexasInstruments/ti-openocd) from https://www.ti.com/tool/download/CCSTUDIO-OPENOCD/1.5.0 (it has target/ti_mspm33.cfg, ti_am13e230x.cfg, ti_cc23xx.cfg and ti_cc27xx.cfg, which xPack OpenOCD 0.12.0-7 lacks); unpack it to ~/ti/ti-openocd",
    "c2000ware": "C2000Ware 26.02.00.00 from https://www.ti.com/tool/C2000WARE (free)",
    "c2000_cgt": "TI's C2000 compiler (ti-cgt-c2000), inside CCStudio or from https://www.ti.com/tool/C2000-CGT (free)",
    "ccs_scripting": "CCStudio 20 or later (https://www.ti.com/tool/CCSTUDIO, free): its scripting/run.sh runs the C2000 debug steps",
    "dslite": "TI's DSLite, the command-line flasher inside UniFlash (https://www.ti.com/tool/UNIFLASH, free) or CCStudio (ccs_base/DebugServer/bin)",
    "make": "make (macOS: xcode-select --install; Linux: your package manager; Windows: the gmake inside CCStudio's utils/bin)",
    "xdsdfu": "TI's xdsdfu, inside CCStudio (ccs_base/common/uscif/xds110) or UniFlash; optional, used only to list probes",
    "claude": "Claude Code (https://docs.claude.com/en/docs/claude-code), then run claude once to sign in",
    "codex": "Codex CLI (https://developers.openai.com/codex), then run codex once to sign in",
}
ENV = {"mspm0_sdk": "FRL_MSPM0_SDK", "mspm33_sdk": "FRL_MSPM33_SDK", "f3_sdk": "FRL_F3_SDK", "f2_sdk": "FRL_F2_SDK", "wifi_sdk": "FRL_WIFI_SDK", "wifi_toolbox": "FRL_WIFI_TOOLBOX", "am13_sdk": "FRL_AM13_SDK", "sysconfig": "FRL_SYSCONFIG", "ticlang": "FRL_TICLANG", "openocd": "FRL_OPENOCD", "ti_openocd": "FRL_TI_OPENOCD", "dslite": "FRL_DSLITE", "c2000ware": "FRL_C2000WARE", "c2000_cgt": "FRL_C2000_CGT", "ccs_scripting": "FRL_CCS_SCRIPTING",
       "make": "FRL_MAKE", "xdsdfu": "FRL_XDSDFU", "claude": "FRL_CLAUDE", "codex": "FRL_CODEX"}


def default_roots(plat=None, home=None, env=None):
    """TI's default install folders for one OS, as strings, overrides first. Pure: no disk access, so the
    Windows and Linux lists can be checked from any computer (tools/tests/test_toolfind_os.py)."""
    plat = plat or sys.platform
    env = os.environ if env is None else env
    home = home if home is not None else HOME
    roots = [env.get("TI_ROOT_OVERRIDE", ""), env.get("FRL_TI_ROOT", "")]
    if plat == "darwin":
        roots += ["/Applications/ti", str(home / "ti")]
    elif plat == "win32":
        roots += ["C:\\ti", str(home / "ti")]
    else:
        roots += [str(home / "ti"), "/opt/ti", "/mnt/c/ti"]
    return [r for r in roots if r]


def ti_roots():
    seen, out = set(), []
    for r in default_roots():
        try:   # a folder the user may not read (macOS privacy, a sandbox) raises EPERM here; treat it as not installed
            ok = r not in seen and Path(r).is_dir()
        except OSError:
            ok = False
        if ok:
            seen.add(r)
            out.append(Path(r))
    return out


def install_line(kind, plat=None):
    """Where to get the tool, plus where it lands on this OS, so a missing tool names one next step."""
    plat = plat or sys.platform
    where = {"darwin": "/Applications/ti or ~/ti", "win32": "C:\\ti", "linux": "~/ti"}.get(plat, "~/ti")
    line = DOWNLOAD[kind]
    if kind in ("mspm0_sdk", "mspm33_sdk", "f2_sdk", "f3_sdk", "am13_sdk", "sysconfig", "ticlang"):
        line += f"; keep the installer's default folder ({where}), or point {ENV[kind]} at another one"
    elif kind == "openocd":
        line += {"win32": "; unpack the Windows .zip to %USERPROFILE%\\xpack-openocd-0.12.0-7",
                 "darwin": "; unpack the darwin .tar.gz to ~/xpack-openocd-0.12.0-7",
                 }.get(plat, "; unpack the linux .tar.gz to ~/xpack-openocd-0.12.0-7, then install its udev rule "
                             "(docs/install-routes.md, \"Linux: let your user open the probe\")")
    elif kind == "make":
        line = {"darwin": "make (run xcode-select --install)",
                "win32": "make (CCStudio includes gmake in C:\\ti\\ccs2101\\ccs\\utils\\bin; or any GNU make)",
                }.get(plat, "make (sudo apt install make on Ubuntu or Debian, or your distribution's package)")
    return line


def saved():
    try:
        return json.loads((data_dir() / "tools.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def _exe(p):
    p = Path(p)
    if WIN and not p.suffix and p.with_suffix(".exe").exists():
        p = p.with_suffix(".exe")
    return p if p.is_file() and os.access(p, os.X_OK) else None


def _run_version(cmd, rx):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired):
        return None
    m = re.search(rx, r.stdout + r.stderr)
    return m.group(1) if m else None


# Each probe(path) -> candidate dict or None. 'path' is what the build passes; 'exe' is what must run.
def _sdk_probe(prefix):
    def f(p):
        p = Path(p)
        m = re.search(prefix + r"_sdk_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
        if not (p / "source" / "ti" / "driverlib").is_dir() or not m:
            return None
        return {"path": str(p), "exe": str(p / "source" / "ti" / "driverlib"), "version": ".".join(m.groups())}
    return f


_sdk = _sdk_probe("mspm0")
_sdk33 = _sdk_probe("mspm33")


def _c2000ware(p):
    p = Path(p)
    m = re.search(r"c2000ware_core_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
    if not ((p / "driverlib").is_dir() and (p / "device_support").is_dir()) or not m:
        return None
    return {"path": str(p), "exe": str(p / "driverlib"), "version": ".".join(m.groups())}


def _c2000_cgt(p):
    p = Path(p)
    m = re.search(r"ti-cgt-c2000_(\S+)", p.name)
    e = _exe(p / "bin" / ("cl2000.exe" if WIN else "cl2000"))
    return {"path": str(p), "exe": e, "version": m.group(1) if m else "unknown"} if e else None


def _ccs_scripting(p):
    p = Path(p)
    return {"path": str(p), "exe": str(p), "version": "unknown"} if p.is_file() else None


def _sdk_am13(p):
    """TI's AM13E230x SDK: its driverlib sits in source/driverlib/am13e230x, not source/ti/driverlib."""
    p = Path(p)
    m = re.search(r"am13e230x_sdk_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
    if not (p / "source" / "driverlib" / "am13e230x").is_dir() or not m:
        return None
    return {"path": str(p), "exe": str(p / "source" / "driverlib" / "am13e230x"), "version": ".".join(m.groups())}


def _wifi_sdk(p):
    """TI's SimpleLink Wi-Fi SDK (CC35xx): the folder name carries the version."""
    p = Path(p)
    m = re.search(r"simplelink_wifi_sdk_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
    if not (p / "examples" / "rtos" / "LP_EM_CC35X1").is_dir() or not m:
        return None
    return {"path": str(p), "exe": str(p / "source"), "version": ".".join(m.groups())}


def _wifi_toolbox(p):
    """TI's SimpleLink Wi-Fi Toolbox: the folder holds the simplelink-wifi-toolbox program and scripts/makefile."""
    p = Path(p)
    m = re.search(r"simplelink_wifi_toolbox_\w+?_(\d+)_(\d+)_(\d+)", p.name)
    exe = p / ("simplelink-wifi-toolbox.exe" if WIN else "simplelink-wifi-toolbox")
    if not exe.is_file() or not (p / "scripts" / "makefile").is_file():
        return None
    return {"path": str(p), "exe": str(exe), "version": ".".join(m.groups()) if m else "unknown"}


def _sdk_f2(p):
    """TI's SimpleLink CC13xx CC26xx SDK (CC13x4, CC26x4): the folder name carries the version."""
    p = Path(p)
    m = re.search(r"simplelink_cc13xx_cc26xx_sdk_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
    if not (p / "source" / "ti" / "devices" / "cc13x4_cc26x4").is_dir() or not m:
        return None
    return {"path": str(p), "exe": str(p / "source" / "ti" / "drivers"), "version": ".".join(m.groups())}


def _sdk_f3(p):
    """TI's SimpleLink Low Power F3 SDK (CC23xx, CC27xx): the folder name carries the version and an _LTS tail."""
    p = Path(p)
    m = re.search(r"simplelink_lowpower_f3_sdk_(\d+)_(\d+)_(\d+)_(\d+)", p.name)
    if not (p / "source" / "ti" / "drivers").is_dir() or not m:
        return None
    return {"path": str(p), "exe": str(p / "source" / "ti" / "drivers"), "version": ".".join(m.groups())}


def _sysconfig(p):
    p = Path(p)
    if p.is_file():
        p = p.parent
    cli = p / ("sysconfig_cli.bat" if WIN else "sysconfig_cli.sh")
    if not cli.is_file():
        return None
    m = re.search(r"sysconfig_(\d+\.\d+\.\d+)", p.name)
    return {"path": str(cli), "exe": str(cli), "version": m.group(1) if m else "unknown"}


def _ticlang(p):
    p = Path(p)
    e = _exe(p / "bin" / "tiarmclang")
    if not e:
        return None
    v = _run_version([str(e), "--version"], r"Compiler (\d+\.\d+\.\d+\.\w+)")
    return {"path": str(p), "exe": str(e), "version": v or "unknown"}


def _openocd(p):
    p = Path(p)
    e = _exe(p) if p.is_file() else _exe(p / "bin" / "openocd")
    if not e:
        return None
    scripts = None
    for s in (e.parent.parent / "openocd" / "scripts", e.parent.parent / "share" / "openocd" / "scripts",
              e.parent.parent / "scripts"):
        if (s / "target").is_dir():
            scripts = s
            break
    v = _run_version([str(e), "--version"], r"Debugger (\S+?)(?:-g[0-9a-f]+|-dirty|\s)")
    return {"path": str(e), "exe": str(e), "version": v or "unknown", "scripts": str(scripts) if scripts else None}


def _plain(name, rx=None):
    def f(p):
        e = _exe(p)
        if not e:
            return None
        v = _run_version([str(e), "--version"], rx) if rx else None
        return {"path": str(e), "exe": str(e), "version": v or "unknown"}
    return f


PROBE = {"mspm0_sdk": _sdk, "mspm33_sdk": _sdk33, "f3_sdk": _sdk_f3, "f2_sdk": _sdk_f2, "wifi_sdk": _wifi_sdk, "wifi_toolbox": _wifi_toolbox, "am13_sdk": _sdk_am13, "c2000ware": _c2000ware, "c2000_cgt": _c2000_cgt, "ccs_scripting": _ccs_scripting, "sysconfig": _sysconfig, "ticlang": _ticlang, "openocd": _openocd, "ti_openocd": _openocd,
         "make": _plain("make", r"Make (\S+)"), "dslite": _plain("DSLite", r"DSLite version (\S+)"), "xdsdfu": _plain("xdsdfu"),
         "claude": _plain("claude", r"(\d+\.\d+\.\d+)"), "codex": _plain("codex", r"codex-cli (\S+)")}


def search_patterns(kind, roots, home=None, plat=None):
    """Glob patterns for one tool under these roots. Pure, like default_roots."""
    home = home if home is not None else HOME
    plat = plat or sys.platform
    if kind == "mspm0_sdk":
        return [str(r / "mspm0_sdk_*") for r in roots]
    if kind == "mspm33_sdk":
        return [str(r / "mspm33_sdk_*") for r in roots]
    if kind == "c2000ware":
        return [str(r / "c2000ware_core_*") for r in roots] + [str(r / "c2000" / "c2000ware_core_*") for r in roots]
    if kind == "c2000_cgt":
        return [str(r / "ti-cgt-c2000_*") for r in roots] + [str(r / "ccs*" / "ccs" / "tools" / "compiler" / "ti-cgt-c2000_*") for r in roots]
    if kind == "ccs_scripting":
        return [str(r / "ccs*" / "ccs" / "scripting" / ("run.bat" if plat == "win32" else "run.sh")) for r in roots]
    if kind == "am13_sdk":
        return [str(r / "am13e230x_sdk_*") for r in roots]
    if kind == "f3_sdk":
        return [str(r / "simplelink_lowpower_f3_sdk_*") for r in roots]
    if kind == "f2_sdk":
        return [str(r / "simplelink_cc13xx_cc26xx_sdk_*") for r in roots]
    if kind == "wifi_sdk":
        return [str(r / "simplelink_wifi_sdk_*") for r in roots]
    if kind == "wifi_toolbox":
        return [str(r / "simplelink_wifi_toolbox_*") for r in roots]
    if kind == "sysconfig":
        return [str(r / "sysconfig_*") for r in roots] + [str(r / "ccs*" / "ccs" / "utils" / "sysconfig_*") for r in roots]
    if kind == "ticlang":
        return [str(r / "ti-cgt-armllvm_*") for r in roots] + [str(r / "ccs*" / "ccs" / "tools" / "compiler" / "ti-cgt-armllvm_*") for r in roots]
    if kind == "openocd":
        pats = [str(home / "zephyr-toolchain" / "xpack-openocd-*"), str(home / ".local" / "xPacks" / "@xpack-dev-tools" / "openocd" / "*" / ".content"),
                str(home / "xpack-openocd-*")]
        pats += [str(r / "xpack-openocd-*") for r in roots]
        if plat == "win32":
            pats += [str(home / "AppData" / "Roaming" / "xPacks" / "@xpack-dev-tools" / "openocd" / "*" / ".content")]
        else:
            pats += ["/opt/xpack-openocd-*"]
        return pats
    if kind == "ti_openocd":
        return [str(r / p) for r in [home / "ti", *roots] for p in ("ti-openocd*", "ccstudio-openocd*", "CCSTUDIO-OPENOCD*")]
    if kind == "make" and plat == "win32":
        return [str(r / "ccs*" / "ccs" / "utils" / "bin" / "gmake.exe") for r in roots]
    if kind == "dslite":
        exe = "DSLite.exe" if plat == "win32" else "DSLite"
        return [str(r / "uniflash_*" / "deskdb" / "content" / "TICloudAgent" / "*" / "ccs_base" / "DebugServer" / "bin" / exe) for r in roots] + \
               [str(r / "ccs*" / "ccs" / "ccs_base" / "DebugServer" / "bin" / exe) for r in roots]
    if kind == "xdsdfu":
        exe = "xdsdfu.exe" if plat == "win32" else "xdsdfu"
        return [str(r / "ccs*" / "ccs" / "ccs_base" / "common" / "uscif" / "xds110" / exe) for r in roots] + \
               [str(r / "uniflash_*" / "deskdb" / "content" / "TICloudAgent" / "*" / "ccs_base" / "common" / "uscif" / "xds110" / exe) for r in roots]
    return []


def _search(kind):
    found = [x for pat in search_patterns(kind, ti_roots()) for x in sorted(glob.glob(pat))]
    if kind in ("openocd", "make", "claude", "codex"):
        w = shutil.which(kind) or (shutil.which("gmake") if kind == "make" and WIN else None)
        if w:
            found.append(w)
        if kind == "claude":
            found.append(str(HOME / ".local" / "bin" / "claude"))
    return found


def candidates(kind):
    seen, out = set(), []
    for p in _search(kind):
        c = PROBE[kind](p)
        if c and c["exe"] not in seen:
            seen.add(c["exe"])
            out.append(c)
    return out


def override(kind):
    v = os.environ.get(ENV[kind]) or (os.environ.get("OCD") if kind == "openocd" else None) or saved().get(kind)
    return v


def find(kind, validated=None, required=True):
    """The tool to use, as a candidate dict with 'source' (override or discovered) and 'validated' (bool).

    Stops with FRL-E020/E021/E022 when the choice is not safe to make silently; returns None when the
    tool is optional and absent."""
    ov = override(kind)
    if ov:
        c = PROBE[kind](ov)
        if not c:
            stop("refused", "FRL-E020", f"{ENV[kind]} (or saved setting) points at {ov}, which has no working {kind}",
                 f"fix the path, or unset it to search the default places. Get it: {install_line(kind)}", tool=kind)
        c.update(source="override", validated=(not validated) or c["version"] in validated)
        return c
    cands = candidates(kind)
    if not cands:
        if not required:
            return None
        want = f"{kind} {validated[0]}: " if validated else ""
        stop("refused", "FRL-E020", f"{kind} not found", f"install {want}{install_line(kind)}" + ("" if kind in ("mspm0_sdk", "mspm33_sdk", "f2_sdk", "f3_sdk", "sysconfig", "ticlang") else f"; or set {ENV[kind]} to where it is"),
             tool=kind, searched=[str(r) for r in ti_roots()])
    if not validated:
        c = dict(cands[0], source="discovered", validated=True, others=[x["path"] for x in cands[1:]])
        return c
    good = [x for x in cands if x["version"] in validated]
    if len(good) == 1 or (good and len({x["version"] for x in good}) == 1 and len(good) > 1 and _same_install(good)):
        return dict(good[0], source="discovered", validated=True,
                    others=[f"{x['path']} ({x['version']})" for x in cands if x is not good[0]])
    if len(good) > 1:
        stop("refused", "FRL-E022", f"{len(good)} validated copies of {kind}: " + "; ".join(x["path"] for x in good),
             f"pick one: export {ENV[kind]}=<path>", tool=kind, choices=good)
    stop("refused", "FRL-E021",
         f"{kind} {', '.join(x['version'] for x in cands)} installed; this route was validated with {', '.join(validated)}",
         f"install {kind} {validated[0]} ({install_line(kind)}), or use yours anyway with export {ENV[kind]}={cands[0]['path']} "
         "(the run is then recorded as unvalidated)", tool=kind, choices=cands)


def _same_install(good):
    """Two discovered copies of one version are the same install when they resolve to one file."""
    return len({os.path.realpath(x["exe"]) for x in good}) == 1
