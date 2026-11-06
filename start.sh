#!/bin/bash
# One command from a clone to a checked result on a LaunchPad kit, with or without an AI assistant.
#
#   ./start.sh <board> <job> [--probe SERIAL] [--assistant none|claude|codex] [--pattern 150,150,150,900] [--dry-run]
#
# With no assistant (the default where the board has a guided route) it runs the shared local tool:
# check the board and tools, back up, build, flash, and check the result through the debugger.
# Full options: ./start.sh --help. This finds Python and runs start.py beside it, which fills a sparse clone
# and runs tools/frl/start.py (docs/frl.md). On Windows run  py start.py <board> <job>  instead.
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

py=""
for c in python3 python; do
  if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)' 2>/dev/null; then
    py="$c"; break
  fi
done
if [ -z "$py" ]; then
  echo "start.sh needs Python 3.9 or later, and none was found."
  echo "  macOS: run  xcode-select --install  (Apple's command line tools, which include python3 and make)"
  echo "  Ubuntu or Debian: sudo apt install python3 make git"
  echo "  Fedora: sudo dnf install python3 make git"
  echo "  Windows: install Python from python.org, then in PowerShell run  py start.py <board> <job>"
  exit 2
fi

exec "$py" "$here/start.py" "$@"
