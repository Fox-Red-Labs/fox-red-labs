#!/bin/bash
# Host test for portability/iot-chain/core (credential record, command parser, topic and telemetry helpers).
# Builds with the Mac's cc into a temp folder and runs. No board needed.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; CORE="$HERE/../core"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -std=c99 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -I"$CORE" \
	"$HERE/host_test.c" "$CORE/cred_record.c" "$CORE/cmd_parse.c" -o "$OUT/host_test"
"$OUT/host_test"
