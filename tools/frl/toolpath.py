#!/usr/bin/env python3
"""One tool's location for a build or flash script, found the way frl finds it, so no script hard-codes a Mac path.

  python3 tools/frl/toolpath.py <kind>            the thing to run (DSLite, tiarmclang, sysconfig_cli.sh, xdsdfu)
  python3 tools/frl/toolpath.py <kind> --path     the install folder (an SDK root, the compiler folder)
  python3 tools/frl/toolpath.py <kind> --optional exit 0 with no output when it is absent, instead of exit 2
  python3 tools/frl/toolpath.py <kind> --version 1.28.0   prefer that version when several are installed (else any)

kind is one of toolfind's: am13_sdk mspm0_sdk mspm33_sdk f2_sdk f3_sdk wifi_sdk wifi_toolbox sysconfig ticlang dslite
openocd ti_openocd xdsdfu c2000ware c2000_cgt make. The FRL_<KIND> variable wins; then the TI install folders
(/Applications/ti, ~/ti, C:\\ti, or FRL_TI_ROOT). When the tool is absent it prints, on stderr, its name, where to get it
and the variable to set, and exits 2, so a script can stop with that message instead of a path error.

Shell use:   DSLITE="$(python3 "$REPO/tools/frl/toolpath.py" dslite)" || exit 2
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolfind  # noqa: E402
from core import Stop  # noqa: E402


def main():
    argv = sys.argv[1:]
    versions = [argv[i + 1] for i, a in enumerate(argv) if a == "--version" and i + 1 < len(argv)]
    args = [a for i, a in enumerate(argv) if not a.startswith("--") and not (i > 0 and argv[i - 1] == "--version")]
    if len(args) != 1 or args[0] not in toolfind.ENV:
        sys.exit(__doc__)
    kind = args[0]
    want_path = "--path" in argv
    try:
        c = toolfind.find(kind, versions or None, required=False)
        if versions and c and c.get("version") not in versions:   # an unvalidated version: use it, say so once
            print(f"note: {kind} {c.get('version')} found; this build was checked with {', '.join(versions)}", file=sys.stderr)
    except Stop as e:
        choices = (e.result.get("data") or {}).get("choices") or e.result.get("choices")
        if e.result.get("code") == "FRL-E021" and choices:   # only other versions installed: use the newest, say so
            c = sorted(choices, key=lambda x: x.get("version", ""))[-1]
            print(f"note: {kind} {c.get('version')} found; this build was checked with {', '.join(versions)}", file=sys.stderr)
        else:                 # two validated copies, or an override that points at nothing: say so, do not guess
            print(f"{e.result.get('message', e)}\n  {e.result.get('next', '')}", file=sys.stderr)
            return 2
    if not c:
        if "--optional" in sys.argv:
            return 0
        print(f"Not installed: {kind}\n  get it: {toolfind.install_line(kind)}\n  or set {toolfind.ENV[kind]} to where it is", file=sys.stderr)
        return 2
    print(c["path"] if want_path else c["exe"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
