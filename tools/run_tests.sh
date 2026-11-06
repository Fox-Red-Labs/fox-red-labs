#!/usr/bin/env bash
# Run the whole test suite with a Python that can: 3.12 or newer, with pytest (requirements.txt).
#   bash tools/run_tests.sh                 everything under tools/tests (about 10 minutes)
#   bash tools/run_tests.sh -k recipe       any pytest arguments pass through
#   FRL_PYTHON=/path/to/python bash tools/run_tests.sh     force one interpreter
# Tests that need an optional package or a tool (node, Playwright, Chrome) skip when it is missing.
set -u
cd "$(dirname "$0")/.." || exit 2

ok() { "$1" -c 'import sys, pytest; sys.exit(0 if sys.version_info >= (3, 12) else 1)' 2>/dev/null; }

PY="${FRL_PYTHON:-}"
if [ -z "$PY" ]; then
  for c in python3.14 python3.13 python3.12 python3; do
    if command -v "$c" >/dev/null 2>&1 && ok "$(command -v "$c")"; then PY="$(command -v "$c")"; break; fi
  done
fi
if [ -z "$PY" ] || ! ok "$PY"; then
  echo "No Python 3.12 or newer with pytest found. Install one, then: python3.14 -m pip install -r requirements.txt" >&2
  echo "(or set FRL_PYTHON to the interpreter to use)" >&2
  exit 2
fi
echo "running tools/tests with $PY ($("$PY" -V 2>&1))"
exec "$PY" -m pytest -q tools/tests "$@"
