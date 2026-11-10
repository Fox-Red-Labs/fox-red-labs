#!/bin/bash
# Build TI's ipc_notify_echo for the LP-AM243 with the Fox Red Labs timing patch (am243x/bench/ipc/ipc_notify_echo.c, copied over the SDK's
# source for the build; the SDK original is kept as ipc_notify_echo.c.orig in the arrival run folder). Images land in $I.
#   ipc_build.sh   -> $I/core0_{all,nom4,one}.out and $I/core{1,2,3}.out, $I/m4.out. Needs env.sh of the run folder sourced.
I=${I:-$HOME/tmp/_agent/runs/arrivals/am243x/ipc}; E=$SDK/examples/drivers/ipc/ipc_notify_echo/am243x-lp; mkdir -p "$I"
cp "$(dirname "$0")/../bench/ipc/ipc_notify_echo.c" "$SDK/examples/drivers/ipc/ipc_notify_echo/ipc_notify_echo.c"
make -C $E/system_freertos_nortos syscfg $MAKE_ARGS DEVICE=am243x PROFILE=release > "$I/syscfg.log" 2>&1 || { echo "syscfg failed"; exit 1; }
b() { # dir out extra
  ( cd "$E/$1/ti-arm-clang" && rm -rf obj *.out && make -j2 ipc_notify_echo.release.out SYSTEM_FLAG=true DEVICE=am243x PROFILE=release $MAKE_ARGS "DEFINES_common=-DSOC_AM243X -DOS_$2 $4" > build.log 2>&1 && cp ipc_notify_echo.release.out "$I/$3.out" && echo "built $3" ) || echo "FAIL $3"
}
b r5fss0-0_freertos FREERTOS core0_all ""
b r5fss0-0_freertos FREERTOS core0_nom4 "-DFRL_NO_M4"
b r5fss0-0_freertos FREERTOS core0_one "-DFRL_ONE_REMOTE"
b r5fss0-1_nortos NORTOS core1 ""
b r5fss1-0_nortos NORTOS core2 ""
b r5fss1-1_nortos NORTOS core3 ""
b m4fss0-0_nortos NORTOS m4 ""
