#!/bin/bash
# Build and run the portable motor core's host test on the Mac in every number-format build. Needs only cc.
# Prints OK on success. The last build is a negative control (a halved current-loop gain) and must fail a bound.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; CORE="$HERE/../core"; T="${TMPDIR:-/tmp}/frl_motor_host_test.$$"
CFLAGS="-std=gnu99 -O1 -Wall -Wextra -Werror -I$CORE"
rc=0
build_run() {   # name, defines...
	local name="$1"; shift
	cc $CFLAGS "$@" -o "$T" "$HERE/host_test.c" "$CORE/guard.c" -lm || { echo "BUILD FAILED: $name"; rc=1; return 1; }
	"$T" || { echo "FAILED: $name"; rc=1; return 1; }
}
build_run float+libm   -DFRL_NUM=0 -DFRL_TRIG=0 -DFRL_TIER=2
build_run float+table  -DFRL_NUM=0 -DFRL_TRIG=1 -DFRL_TIER=2
build_run iq24+libm    -DFRL_NUM=2 -DFRL_TRIG=0 -DFRL_TIER=2
build_run iq24+table   -DFRL_NUM=2 -DFRL_TRIG=1 -DFRL_TIER=2
build_run q15+table    -DFRL_NUM=1 -DFRL_TRIG=1 -DFRL_TIER=2
# negative control: must fail
cc $CFLAGS -DFRL_NUM=0 -DFRL_TRIG=0 -DFRL_TIER=2 -DFRL_TEST_BREAK -o "$T" "$HERE/host_test.c" "$CORE/guard.c" -lm || { echo "BUILD FAILED: negative control"; rc=1; }
if [ -x "$T" ] && "$T" >/dev/null 2>&1; then echo "NEGATIVE CONTROL PASSED (it must fail): the test would not catch a broken loop"; rc=1; elif [ -x "$T" ]; then echo "negative control: the halved-gain build fails a bound, as it must"; fi
# impossible configurations must be refused at compile time
for combo in "-DFRL_TIER=1 -DFRL_F_FOC=1" "-DFRL_TIER=2 -DFRL_F_HALL_TRACK=0" "-DFRL_TIER=1 -DFRL_F_FW=1" "-DFRL_TIER=2 -DFRL_F_BRUSHED=1" "-DFRL_TIER=2 -DFRL_F_IPD=1" "-DFRL_TIER=7"; do
	if cc -std=gnu99 -I"$CORE" $combo -fsyntax-only -x c - <<<'#include "frl_cfg.h"' 2>/dev/null; then echo "NOT REFUSED: $combo"; rc=1; fi
done
echo "impossible flag combinations refused at compile time"
rm -f "$T"
[ $rc -eq 0 ] && echo "OK" || echo "FAILED"
exit $rc
