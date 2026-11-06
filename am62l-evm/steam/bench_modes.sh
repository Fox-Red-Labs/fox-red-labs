#!/bin/sh
# Inside frl-steam-user, as uid 1000: run_user.sh once per mode, one after another, to the sign-in window.
# Usage: sh /s/bench_modes.sh [mode[.n]...]   (default: every mode below). Results: /data/runs/<mode>/summary.txt
#   trap       LSE trap only (every LSE instruction a signal), CEF sandbox on, default flags
#   patch      LSE_PATCH=1 hot-patching, CEF sandbox on, default flags
#   nosandbox  patch, -no-cef-sandbox
#   single     patch, -cef-single-process
#   nogpu      patch, -cef-disable-gpu -cef-disable-gpu-compositing
#   silent     patch, -silent
#   nofriends  patch, -nofriendsui -nochatui
#   bigpic     patch, -gamepadui (Big Picture)
#   nobrowser  patch, -no-browser
#   lean       patch, -cef-disable-gpu -cef-disable-gpu-compositing -nofriendsui -nochatui
IDLE=${IDLE:-30}; WAIT=${WAIT:-300}
[ $# -gt 0 ] || set -- trap patch nosandbox single nogpu silent nofriends bigpic nobrowser lean
for m in "$@"; do        # a tag may carry a repeat suffix: trap.2 runs mode trap into /data/runs/trap.2
  case ${m%%.*} in
    trap) LSE_PATCH=0 sh /s/run_user.sh $m $WAIT $IDLE ;;
    patch) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE ;;
    nosandbox) LSE_PATCH=1 CEF_SANDBOX=0 sh /s/run_user.sh $m $WAIT $IDLE ;;
    single) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -cef-single-process ;;
    nogpu) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -cef-disable-gpu -cef-disable-gpu-compositing ;;
    silent) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -silent ;;
    nofriends) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -nofriendsui -nochatui ;;
    bigpic) WINRE='"Sign in to Steam"|"Steam Big Picture Mode"' LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -gamepadui ;;
    nobrowser) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -no-browser ;;
    lean) LSE_PATCH=1 sh /s/run_user.sh $m $WAIT $IDLE -cef-disable-gpu -cef-disable-gpu-compositing -nofriendsui -nochatui ;;
    *) echo "unknown mode $m"; continue ;;
  esac
  sleep 5
done
