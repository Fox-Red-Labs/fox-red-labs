#!/usr/bin/env python3
"""Call one CCStudio MCP tool from a terminal. CCStudio must be open with an AI-enabled workspace.

  ccs_mcp.py <server> <tool|--list [name...]> [json-args] [timeout]

CCStudio's Node and MCP proxy are found from the newest CCStudio install (tools/benchmark/common.py, or
"ccs" in tools/benchmark/settings.local.json); CCS_WORKSPACE names the workspace (default
~/workspace_ccstheia). Exit status: 0 the call succeeded, 1 the tool returned isError or a JSON-RPC error,
2 CCStudio's server did not start or answer (stderr shows why), 3 the call timed out.
"""
import json
import os
import select
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "benchmark"))
import common as c  # noqa: E402


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    srv, tool = sys.argv[1], sys.argv[2]
    ccs = c.find_ccs()
    proxy, node = c.find_mcp_proxy(ccs)
    if not proxy or not node:
        print(f"CCStudio's MCP proxy or its Node.js not found (CCStudio at {ccs}); install CCStudio 20.1 or later", file=sys.stderr)
        return 2
    ws = Path(os.environ.get("CCS_WORKSPACE", Path.home() / "workspace_ccstheia"))
    env = dict(os.environ)
    env.pop("ELECTRON_RUN_AS_NODE", None)     # set by Electron hosts; it changes how CCStudio's Node starts
    p = subprocess.Popen([str(node), str(proxy), srv], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True, env=env, cwd=str(ws) if ws.is_dir() else None)

    def send(o):
        p.stdin.write(json.dumps(o) + "\n")
        p.stdin.flush()

    def recv(i, timeout):
        end = time.time() + timeout
        while time.time() < end:
            if p.poll() is not None:
                return {"error": {"message": "server exited: " + p.stderr.read()[-600:]}, "_exit": True}
            r, _, _ = select.select([p.stdout], [], [], 1)
            if r:
                line = p.stdout.readline()
                if not line:
                    break
                try:
                    d = json.loads(line)
                except ValueError:
                    continue
                if d.get("id") == i:
                    return d
        return {"error": {"message": "timeout"}, "_timeout": True}

    try:
        send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2024-11-05", "capabilities": {},
                                                                               "clientInfo": {"name": "frl", "version": "1"}}})
        init = recv(1, 20)
        if "error" in init:
            print(f"CCStudio's {srv} server did not answer initialize ({init['error'].get('message')}). Is CCStudio open with an AI-enabled workspace?", file=sys.stderr)
            return 2
        send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        if tool == "--list":
            send({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
            d = recv(2, 20)
            if "error" in d:
                print(json.dumps(d["error"]), file=sys.stderr)
                return 3 if d.get("_timeout") else 1
            for t in d["result"]["tools"]:
                if len(sys.argv) > 3 and t["name"] not in sys.argv[3:]:
                    continue
                print("###", t["name"])
                print(t.get("description", "")[:900])
                print(json.dumps(t.get("inputSchema", {}).get("properties", {}))[:900])
                print("required:", t.get("inputSchema", {}).get("required"))
            return 0
        args = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        to = int(sys.argv[4]) if len(sys.argv) > 4 else 120
        send({"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": tool, "arguments": args}})
        d = recv(3, to)
        if "error" in d:
            print(f"[error] {d['error'].get('message')}", file=sys.stderr)
            return 3 if d.get("_timeout") else 1
        r = d["result"]
        for item in r.get("content", []):
            print(item.get("text", item))
        if r.get("isError"):
            print("[isError]", file=sys.stderr)
            return 1
        return 0
    finally:
        p.kill()


if __name__ == "__main__":
    sys.exit(main())
