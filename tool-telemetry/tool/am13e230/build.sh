#!/usr/bin/env bash
# The LP-AM13E230 as the tool MCU: the six-step BLDC firmware (am13e230/bldc-drv8376) with tool telemetry added.
# Copies that folder outside the repo, applies tt_tool.inc with three hooks, and builds with its own build.sh.
#   tool-telemetry/tool/am13e230/build.sh             build only
#   tool-telemetry/tool/am13e230/build.sh --flash     build and flash (probe AL130001; the board must be free)
# Output: out/tool-telemetry/tool-am13e230/bldc-drv8376/bldc.out
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
RUN="$PWD/out/tool-telemetry/tool-am13e230"
rm -rf "$RUN"; mkdir -p "$RUN/am13e230"
cp -R "$REPO/am13e230/bldc-drv8376" "$RUN/am13e230/bldc-drv8376"
cp "$REPO"/am13e230/AM13E23019_*.ccxml "$RUN/am13e230/"
python3 - "$RUN/am13e230/bldc-drv8376/main.c" "$HERE/tt_tool.inc" <<'PY'
import sys
p, inc = sys.argv[1], open(sys.argv[2]).read()
t = open(p).read()
def rep(a, b):
    global t
    if t.count(a) != 1:
        sys.exit("patch point missing or not unique: " + a[:60])
    t = t.replace(a, b)
rep("int main(void)\n{", inc + "int main(void)\n{")
rep("        uint32_t now = cyc(), h = hall();\n", "        uint32_t now = cyc(), h = hall();\n        tt_tick(now);\n")
rep("            g_fault = 1; stop(); drv_off(true);\n", "            g_fault = 1; tt_fault(); stop(); drv_off(true);\n")
rep("{ stop(); puts_(\"no Hall edge for 1.5 s", "{ tt_stall(); stop(); puts_(\"no Hall edge for 1.5 s")
rep("        c = getc_();\n        if (c < 0) continue;\n", "        c = getc_();\n        if (c < 0) continue;\n        if (tt_rx(c)) continue;\n")
rep("            case 'g': if (!learned)", "            case 'g': if (tt_locked) { puts_(\"locked by the fleet\\r\\n\"); break; } if (!learned)")
open(p, "w").write(t)
print("patched", p)
PY
OUT="$RUN" bash "$RUN/am13e230/bldc-drv8376/build.sh" "$@"
