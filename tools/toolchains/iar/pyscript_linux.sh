#!/bin/sh
# Linux stand-in for SimpleLink SDK tools that ship as macOS binaries but also ship their Python source beside them
# (tools/ble/oad/commands_runner, msoad_prebuild, msoad_postbuild; standard library only). The harness mounts this file
# over each binary; it runs <name>.py from the same folder.
exec python3 "$(dirname "$0")/$(basename "$0").py" "$@"
