#!/bin/sh
# Inside frl-steamcmd (Debian trixie arm64): Steam's download path with an anonymous login, natively on arm64,
# using DepotDownloader 3.4.0 (SteamKit2; Valve's steamcmd is 32-bit x86 only). No account.
# Usage: sh /g/src/depot_anon.sh [appid=1007]   (1007: Steamworks SDK Redist, anonymous; 896660: Valheim server)
set -u
APP=${1:-1007}; D=/g/depot; mkdir -p $D/out/$APP /g/logs
cd $D; [ -x DepotDownloader ] || { curl -fsSL -o dd.zip https://github.com/SteamRE/DepotDownloader/releases/download/DepotDownloader_3.4.0/DepotDownloader-linux-arm64.zip && unzip -oq dd.zip && chmod +x DepotDownloader; }
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 ./DepotDownloader -app $APP -os linux -osarch 64 -dir out/$APP -max-downloads 4 > /g/logs/depot-$APP.log 2>&1 &
P=$!; s=$(date +%s.%N)
wait $P; rc=$?; e=$(date +%s.%N)
mb=$(du -sm out/$APP | cut -f1)
awk -v s=$s -v e=$e -v mb=$mb -v rc=$rc -v app=$APP 'BEGIN{printf "app %s rc=%d %d MB in %.1f s = %.1f MB/s\n", app, rc, mb, e-s, mb/(e-s)}'
grep -iE "anonymous|logged|depot|total|error|fail" /g/logs/depot-$APP.log | tail -8
