"""Build and sign TI's SimpleLink Wi-Fi SDK example for frl (CC35xx): make in freertos/ticlang, then the Wi-Fi
Toolbox image builder, which signs the images with the SDK example key and writes toolbox/tool_settings.json beside
the .out. Host side only: it burns nothing and opens no probe. The same steps as cc35x1/AGENTS.md, with paths from
toolfind; the build folder is absolute, because tool_settings.json takes its paths from it.
"""
import subprocess
from pathlib import Path

from core import stop


def build(proj, t, r, log):
    """Returns the .out; toolbox/tool_settings.json beside it is what the Toolbox programs."""
    d = (proj / r["build_dir"]).resolve()
    out = d / Path(r["image"]).name
    sdk, tb = t["sdk"]["path"], t["toolbox"]["path"]
    lines = []

    def step(args, what):
        p = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=900, cwd=d)
        lines.append(" ".join(map(str, args)) + "\n" + p.stdout + p.stderr)
        log.write_text("\n".join(lines), encoding="utf-8")
        if p.returncode:
            tail = (p.stdout + p.stderr).strip().splitlines()[-12:]
            stop("failed", "FRL-E030", f"the {what} failed: " + (tail[-1] if tail else "no output"),
                 f"read the first error in {log}; if the disk is nearly full, free some space and build again "
                 "(the Toolbox and the linker stop without saying why)", log=str(log), tail=tail)

    step([t["make"]["exe"], f"SIMPLELINK_WIFI_SDK_INSTALL_DIR={sdk}", f"SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR={tb}",
          f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['compiler']['path']}", "GCC_ARMCOMPILER="], "build")
    if not out.is_file():
        stop("failed", "FRL-E030", f"the build ended without {out.name}", f"read {log}", log=str(log))
    step([t["make"]["exe"], "-s", "-f", Path(tb) / "scripts" / "makefile", "all", f"SDK_DIR={sdk}", f"SYSCONFIG_ARTIFACT={d}",
          f"BUILD_DIR={d}", f"BUILD_ARTIFACT={out}", f"TOOLBOX_DIR={tb}"], "signing")
    if not (d / "toolbox" / "tool_settings.json").is_file():
        stop("failed", "FRL-E030", "signing ended without toolbox/tool_settings.json", f"read {log}", log=str(log))
    return out
