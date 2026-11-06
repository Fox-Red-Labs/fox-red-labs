#!/bin/sh
# Inside frl-steam: download Valve's public-beta arm64 Linux client (bins_linuxarm64) and lay it out the way
# the community install scripts do (UbuntuAsahi/steam-arm64). No account is used.
# Usage: sh /s/fetch_arm64.sh  -> /s/arm64/home/.local/share/Steam
set -eu
D=${STEAM_DATA:-/s/arm64}; export HOME=$D/home; R=$HOME/.local/share/Steam
mkdir -p "$R/package" "$HOME/.steam" $D/dl
M=$(curl -fsSL https://client-update.steamstatic.com/steam_client_publicbeta_linuxarm64)
echo "$M" > $D/dl/manifest.vdf
Z=$(echo "$M" | grep -E '"file"[[:space:]]+"bins_linuxarm64_linuxarm64.zip' | cut -d '"' -f4)
echo "manifest version: $(echo "$M" | grep -m1 '"version"' | cut -d '"' -f4)  zip: $Z"
[ -s $D/dl/bins.zip ] || curl -fsSL -o $D/dl/bins.zip "https://client-update.steamstatic.com/$Z"
ls -l $D/dl/bins.zip
cd "$R" && unzip -o -qq $D/dl/bins.zip
echo publicbeta > "$R/package/beta"
chmod -R u+rwx "$R/steamrtarm64"
ln -sfn "$R/steamrtarm64" "$R/steamrt64"; touch "$R/.steam-enable-steamrt64-client"
ln -sfn "$R" "$HOME/.steam/steam"; ln -sfn "$R" "$HOME/.steam/root"; ln -sfn "$R/linuxarm64" "$HOME/.steam/sdkarm64"
du -sm "$R"; file "$R/steamrtarm64/steam" "$R/steamrtarm64/steamwebhelper" 2>/dev/null | sed "s|$R/||"
echo "missing libraries:"; LD_LIBRARY_PATH=$R/steamrtarm64 ldd "$R/steamrtarm64/steam" "$R"/steamrtarm64/*.so 2>/dev/null | grep "not found" | sort -u
