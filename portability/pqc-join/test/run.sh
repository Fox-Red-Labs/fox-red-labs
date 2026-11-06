#!/bin/bash
# Host test for the post-quantum join: builds the board side for the Mac, checks the symmetric code
# against OpenSSL, runs the NIST known-answer tests (ML-KEM-768 and ML-DSA-44), then runs the whole exchange against
# tools/pqc_join/mac_side.py through a pipe. Needs: cc, python3 with the cryptography package, openssl 3.5+.
# Prints "pqc-join host test: OK" or fails with the first broken step. Touches no board.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/pqcjoin.XXXXXX")"; trap 'rm -rf "$OUT"' EXIT
"$HERE/build_host.sh" "$OUT"
python3 -I "$HERE/check_sym.py" "$OUT/unit_sym"
"$OUT/board_host" kat | tail -2
"$OUT/board_host" dsa_kat | tail -2
"$OUT/board_host" dsa_timing | grep -v '^R,'
python3 -I "$ROOT/tools/pqc_join/mac_side.py" --exec "$OUT/board_host" join --timeout 60 | tail -22
echo "pqc-join host test: OK"
