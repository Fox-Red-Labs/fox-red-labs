#!/bin/sh
# Inside frl-steam-user, as uid 1000: run a Windows game through Valve's arm64 chain (FEX -> Steam Linux Runtime 4
# -> Proton) by hand, the same chain the client builds, with Proton's log on. Needs python3 in the container and an
# x86-64 rootfs at /usr/share/guestos/fex-mesa (FEX's ArchLinux image unpacked to /data/fex-rootfs, linked there).
# Usage: sh /s/run_proton.sh APPID "GAME DIR NAME" EXE [seconds=600]
# Env: VERB=run joins a Wine session that is already up (default waitforexitandrun waits for it to end). EXE may be
#      a Windows path (C:\\...) to start a built-in program such as notepad.exe. Warm start: under FEX, Wine's desktop
#      takes over 30 s to come up and the first program gives up on it ("no driver could be loaded"); start
#      notepad first, wait for its window, then start the game with VERB=run.
# Log: /data/runs/proton/steam-APPID.log (Proton) and /data/runs/proton/stdout-APPID.txt (everything else).
set -u
APP=${1:?appid}; DIR=${2:?game dir}; EXE=${3:?exe}; T=${4:-600}
S=/data/home/.local/share/Steam; C=$S/steamapps/common; G="$C/$DIR"
export HOME=/data/home DISPLAY=${DISPLAY:-:1} SteamAppId=$APP SteamGameId=$APP STEAM_COMPAT_APP_ID=$APP
export STEAM_COMPAT_DATA_PATH=$S/steamapps/compatdata/$APP STEAM_COMPAT_CLIENT_INSTALL_PATH=$S
export STEAM_COMPAT_INSTALL_PATH="$G" PROTON_LOG=1 PROTON_LOG_DIR=/data/runs/proton XDG_RUNTIME_DIR=/tmp/xdg-$(id -u)
export LD_PRELOAD=/opt/frl/lse_trap.so
# LDDBG=1: glibc loader log per process in /data/runs/proton/ld.<pid> (shows why a dlopen fails)
[ "${LDDBG:-0}" = 1 ] && export LD_DEBUG=files LD_DEBUG_OUTPUT=/data/runs/proton/ld
mkdir -p "$XDG_RUNTIME_DIR" /data/runs/proton; chmod 700 "$XDG_RUNTIME_DIR"
cd "$G" || exit 1
case "$EXE" in [A-Za-z]:*|/*) X="$EXE";; *) X="$G/$EXE";; esac
V=${VERB:-waitforexitandrun}; TAG=${TAG:-$APP}
s0=$(date +%s)
timeout "$T" python3 "$C/FEX-Emu/fex-compat-tool" $V -- \
  "$C/SteamLinuxRuntime_4/_v2-entry-point" --verb=$V -- \
  "$C/Proton - Experimental/proton" $V "$X" > /data/runs/proton/stdout-$TAG.txt 2>&1
echo "rc=$? after $(( $(date +%s) - s0 )) s" | tee -a /data/runs/proton/stdout-$TAG.txt
