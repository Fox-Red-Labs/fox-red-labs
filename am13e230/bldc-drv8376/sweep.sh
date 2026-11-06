#!/bin/bash
# All arguments pass to the validated sweep runner; --check-only never starts the motor.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$HERE/sweep.py" "$@"
