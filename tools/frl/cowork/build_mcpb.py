#!/usr/bin/env python3
"""Build the Claude Desktop extensions (.mcpb) for the Cowork capability experiment (docs/frl-desktop-apps.md).

  python3 tools/frl/cowork/build_mcpb.py <out folder>

Two bundles, built from the same server (tools/frl/mcp_server.py) in the person's own clone:
  frl-hostcheck.mcpb   only frl_host and frl_detect: says where the extension runs and whether it sees USB.
                       Install this first. It reads; it never touches a board.
  frl-board.mcpb       every frl tool. Install only after the host check passed in that app and mode.
Each bundle carries the tool itself (the non-document files in tools/frl/starter-files.txt), so installing
asks no questions: an earlier build asked for the repository folder, and a wrong pick left the server unable
to start, which Claude Desktop shows only as "cannot connect" (2026-10-14). Rebuild the bundles after the tool
changes. Python 3.9 or later must be on the computer (macOS: /usr/bin/python3).
"""
import json
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

# The bundle's own version, raised whenever its packaging changes so Claude Desktop replaces an installed copy
# (0.2.0: carries the tool inside and asks no folder; 0.1.0 asked for the repository folder).
BUNDLE_VERSION = "0.2.0"

LAUNCH = '''#!/usr/bin/env python3
"""Run the Fox Red Labs MCP server carried inside this extension."""
import os, sys
from pathlib import Path
server = Path(__file__).resolve().parent / "repo" / "tools" / "frl" / "mcp_server.py"
os.execv(sys.executable, [sys.executable, str(server)])
'''


ROOT = Path(__file__).resolve().parents[3]


def carried():
    """The tool's own files from the starter list, without documents and caches."""
    out = []
    for line in (ROOT / "tools" / "frl" / "starter-files.txt").read_text().splitlines():
        line = line.strip().strip("/")
        if not line or line.startswith("#") or line.endswith(".md"):
            continue
        p = ROOT / line
        files = [f for f in p.rglob("*") if f.is_file()] if p.is_dir() else [p]
        out += [f.relative_to(ROOT).as_posix() for f in files if "__pycache__" not in f.parts and f.suffix != ".mcpb"]
    return sorted(out)


def manifest(name, display, tools, description):
    return {
        "manifest_version": "0.3", "name": name, "display_name": display, "version": BUNDLE_VERSION,
        "description": description,
        "author": {"name": "Fox Red Labs"},
        "server": {"type": "python", "entry_point": "server/launch.py",
                   "mcp_config": {"command": "python3", "args": ["${__dirname}/server/launch.py"],
                                  "env": {"FRL_MCP_TOOLS": ",".join(tools)}}},
        "tools": [{"name": t} for t in tools] if tools else [],
        "tools_generated": not tools,
        "compatibility": {"platforms": ["darwin", "linux", "win32"], "runtimes": {"python": ">=3.9"}},
    }


def build(out, name, display, tools, description):
    p = out / f"{name}.mcpb"
    with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("manifest.json", json.dumps(manifest(name, display, tools, description), indent=1))
        info = zipfile.ZipInfo("server/launch.py")
        info.external_attr = 0o755 << 16
        z.writestr(info, LAUNCH)
        for rel in carried():
            z.write(ROOT / rel, f"server/repo/{rel}")
    return p


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ".").expanduser()
    out.mkdir(parents=True, exist_ok=True)
    print(build(out, "frl-hostcheck", "Fox Red Labs host check", ["frl_host", "frl_detect"],
                "Says which computer this extension runs on and whether it can see USB debug probes. Reads only."))
    print(build(out, "frl-board", "Fox Red Labs board tools", [],
                "Fox Red Labs board operations: identify, check tools, back up, build, flash with approval, and verify."))


if __name__ == "__main__":
    main()
