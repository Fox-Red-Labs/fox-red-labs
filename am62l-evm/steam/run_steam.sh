#!/bin/sh
# Inside the frl-steam container (setup.sh; then docker exec frl-steam sh /s/run_steam.sh):
# a virtual X screen, Valve's bootstrap unpacked, then each try under Box64 with a time limit. No account is used;
# a login window is the stop point. Writes /s/work/try-*.log and a screenshot of the X screen after each try.
set -u
W=/s/work; cd /s
sh /s/fetch_steam.sh $W > $W.fetch.log 2>&1 || { cat $W.fetch.log; exit 1; }
cat $W.fetch.log
box64 --version 2>&1 | head -2
Xvfb :1 -screen 0 1280x720x24 >/dev/null 2>&1 & sleep 2
export DISPLAY=:1 LIBGL_ALWAYS_SOFTWARE=1 BOX64_LOG=1 HOME=/s/home
mkdir -p $HOME
glxinfo -B 2>&1 | grep -E "renderer|OpenGL version" | head -2
find $W/boot -maxdepth 2 -type f \( -name 'steam' -o -name '*.so' -o -name 'steam.sh' \) | head
try() {  # name seconds command...
  n=$1; t=$2; shift 2; s=$(date +%s)
  timeout "$t" "$@" > $W/try-$n.log 2>&1; rc=$?
  echo "== try $n rc=$rc after $(( $(date +%s)-s )) s; peak RSS of the run unknown; free now: $(free -m | awk '/Mem/{print $3" MB used"}')"
  grep -iE "error|unsupported|32.?bit|elf|cannot|not found|failed|wrong|login|box64" $W/try-$n.log | sort | uniq -c | sort -rn | head -8
  import -window root $W/shot-$n.png 2>/dev/null
}
try bootstrap32 120 box64 $W/boot/ubuntu12_32/steam
try steamsh 600 sh -c "cd $W/boot && BOX64_EMULATED_LIBS=1 STEAM_RUNTIME=1 ./steam.sh -no-cef-sandbox"
ls $W/home 2>/dev/null; du -sh $HOME/.steam $HOME/.local/share/Steam 2>/dev/null
