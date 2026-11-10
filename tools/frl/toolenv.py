#!/usr/bin/env python3
"""Print the make arguments for building an MSPM0 SDK example with discovered tools, NUL-separated.

  python3 tools/frl/toolenv.py mspm0          (used by mspm0g3507/mcu-bench/sdk-common/sdkmake.sh)
  python3 tools/frl/toolenv.py mspm33-cc      the MSPM33 SDK (FRL_MSPM33_SDK) and compiler, same shape as mspm0-cc
  python3 tools/frl/toolenv.py sysconfig      SysConfig alone, for a script that regenerates its files only when stale
                                              (portability/arduino/build.sh)
  python3 tools/frl/toolenv.py mspm0-cc       the SDK and compiler only, for builds with generated files checked in
                                              (mspm0l1306/bench/coremark/build.sh)

Same discovery and overrides as frl (FRL_MSPM0_SDK, FRL_SYSCONFIG, FRL_TICLANG): the validated versions win,
and several or only unvalidated copies stop with the choices on stderr (exit 2) instead of a silent pick.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolfind  # noqa: E402
from core import Stop  # noqa: E402

FAMILIES = {"mspm0": {"sdk": ("mspm0_sdk", ["2.11.00.07"]), "sysconfig": ("sysconfig", ["1.26.3", "1.28.0"]),
                      "ticlang": ("ticlang", ["5.1.1.LTS"])},
            "mspm0-cc": {"sdk": ("mspm0_sdk", ["2.11.00.07"]), "ticlang": ("ticlang", ["5.1.1.LTS"])},
            "mspm33-cc": {"sdk": ("mspm33_sdk", ["1.04.00.00"]), "ticlang": ("ticlang", ["5.1.1.LTS"])},
            "sysconfig": {"sysconfig": ("sysconfig", ["1.26.3", "1.28.0"])}}
MAKE_VAR_33 = {"sdk": "MSPM33_SDK_INSTALL_DIR"}
MAKE_VAR = {"sdk": "MSPM0_SDK_INSTALL_DIR", "sysconfig": "SYSCONFIG_TOOL", "ticlang": "TICLANG_ARMCOMPILER"}


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "mspm0"
    fam = FAMILIES[name]
    try:
        t = {k: toolfind.find(kind, val) for k, (kind, val) in fam.items()}
    except Stop as s:
        sys.stderr.write(s.result.text() + "\n")
        return 2
    for k, v in t.items():
        if not v["validated"]:
            sys.stderr.write(f"note: {k} {v['version']} at {v['path']} is not the validated version\n")
    mv = dict(MAKE_VAR, **(MAKE_VAR_33 if name.startswith("mspm33") else {}))
    args = [f"{mv[k]}={v['path']}" for k, v in t.items()] + ([] if name == "sysconfig" else ["GCC_ARMCOMPILER="])
    sys.stdout.write("\0".join(args) + "\0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
