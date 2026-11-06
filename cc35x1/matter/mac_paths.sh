#!/usr/bin/env bash
# Make tool_settings.mac.json with absolute paths to the copied build's signed components.
# Refuse missing artifacts before writing settings; this does not build or program anything.
#   cc35x1/matter/mac_paths.sh /path/to/copied/toolbox
# (fetch_image.sh makes the same edit for our own thermostat build.)
set -euo pipefail
TB="${1:?path of the copied toolbox folder}"
python3 - "$TB" <<'PY'
import json
import sys
from pathlib import Path

tb = Path(sys.argv[1]).resolve()
source = tb / "tool_settings.json"
section = "programming_debug_and_ota_signed_components"

def rebase(value):
    if isinstance(value, dict):
        return {key: rebase(path) for key, path in value.items()}
    if value is None or value == "":
        return value
    if not isinstance(value, str):
        raise ValueError("signed component paths must be strings or null")
    path = Path(value)
    # Builds copied from the Linux container retain its old .../toolbox/ prefix.
    # Bare filenames and relative toolbox paths are also relative to this copy.
    if "toolbox" in path.parts:
        index = len(path.parts) - 1 - path.parts[::-1].index("toolbox")
        path = Path(*path.parts[index + 1:])
    path = (tb / path).resolve()
    if not path.is_file():
        raise ValueError(f"signed component file not found: {path}")
    return str(path)

try:
    settings = json.loads(source.read_text())
    if not isinstance(settings, dict) or not isinstance(settings.get(section), dict) or not settings[section]:
        raise ValueError(f"expected a nonempty {section} object in {source}")
    settings[section] = rebase(settings[section])
    output = tb / "tool_settings.mac.json"
    output.write_text(json.dumps(settings, indent=4) + "\n")
except (OSError, ValueError) as error:
    sys.exit(f"Cannot prepare Matter Toolbox paths: {error}")
print(f"wrote {output}")
PY
