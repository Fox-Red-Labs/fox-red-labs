#!/bin/bash
# One command to get a Mac ready for the Fox Red Labs kit.
# You log in to TI twice in the browser. The script does the rest.
# This one file is all you need. Copy it to another Mac and run it there.
#
#   ./bootstrap.sh --check      look at this Mac and say whether it will work. Changes nothing. Start here.
#   ./bootstrap.sh              Tier 1: CCStudio and the MSPM0 SDK (what a first blink needs)
#   ./bootstrap.sh --all        Tier 1 plus the six Tier 2 packages
#   ./bootstrap.sh --zephyr     also install Zephyr and build its blink sample (no flashing)
#   ./bootstrap.sh --dry-run    print what would happen, change nothing
#   Flags combine: ./bootstrap.sh --all --zephyr
#
# Status, 2026-10-05: every step below was run by hand or by an agent on one Intel
# MacBook (macOS 15.7.9). The Tier 2 downloads, checksums and no-click installs are
# exactly what was run. The CCStudio no-click install uses TI's documented flags but
# has not been run from this script on a clean Mac yet. Treat a first run as a test.
# Apple Silicon is allowed but has never been tried: TI's tools are Intel programs and
# need Rosetta 2. The Zephyr step is Intel only.
#
# Everything printed is also saved to ~/frl-bootstrap-<date>.log with a time on each step,
# so a test run on another Mac can be brought back and recorded.
#
# Versions are pinned, and every installer is checked against the SHA-256 on TI's download
# page before it runs. TI ships often; if a download or checksum fails, the version moved on.
# Needs: about 12 GB free for Tier 1, 25 GB for --all. Uses no Homebrew and no sudo.
set -euo pipefail

ALL=0; ZEPHYR=0; DRY=0; CHECK=0
for a in "$@"; do case "$a" in
  --all) ALL=1 ;; --zephyr) ZEPHYR=1 ;; --dry-run) DRY=1 ;; --check) CHECK=1; DRY=1 ;;
  -h|--help) sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "Unknown option: $a"; exit 2 ;; esac; done

# This script installs on macOS only (sw_vers, sysctl, the .dmg installers). Elsewhere, say what to run instead
# and stop before any Mac command: on Linux and Windows, frl doctor checks the same tools by TI's default folders.
if [ "$(uname -s)" != "Darwin" ]; then
  echo "bootstrap.sh is the macOS installer; this is $(uname -s). Nothing was changed."
  echo "Check this computer's tools instead (names each missing one, its version and where it installs):"
  echo "  python3 tools/frl/frl.py doctor --board mspm0l1306"
  echo "Per-OS install folders, the Linux udev rule and the Windows drivers: docs/install-routes.md (\"Your computer\")."
  [ "$CHECK" = 1 ] && exit 0 || exit 2
fi

# Where TI's tools go. /Applications/ti is TI's default on macOS and needs an admin account.
# Without one, the script falls back to ~/ti. Override with TI_DIR=<folder>.
TI="${TI_DIR:-/Applications/ti}"
if [ -z "${TI_DIR:-}" ] && ! { [ -w /Applications/ti ] || { [ ! -e /Applications/ti ] && [ -w /Applications ]; }; }; then TI="$HOME/ti"; fi
LOG="$HOME/frl-bootstrap-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee -a "$LOG") 2>&1
DL="$HOME/Downloads"
WORK="$HOME/Downloads/frl-bootstrap"
HERE="$(cd "$(dirname "$0")" && pwd)"
T0=$(date +%s)

say()  { printf '\n== [%s, %s in] %s\n' "$(date +%H:%M:%S)" "$(elapsed)" "$1"; }
run()  { if [ "$DRY" = 1 ]; then echo "   [dry-run] $*"; else "$@"; fi; }
# Free space where the tools go ($TI, or its nearest existing parent), so an install folder on another drive counts that drive
free_gb() { local d="${1:-$TI}"; while [ ! -e "$d" ]; do d=$(dirname "$d"); done; df -g "$d" | tail -1 | awk '{print $4}'; }
need_gb() { local f; f=$(free_gb); if [ "$f" -lt "$1" ]; then echo "Only ${f} GB free in $TI, need $1 GB. Clear space or restart the Mac (a restart frees swap), then re-run."; [ "$DRY" = 1 ] || exit 1; fi; }
elapsed() { local s=$(( $(date +%s) - T0 )); printf '%dm %02ds' $((s/60)) $((s%60)); }

# ---------------------------------------------------------------- 1. host check
say "1. Host check"
sw_vers | sed 's/^/   /'
echo "   CPU: $(sysctl -n machdep.cpu.brand_string) ($(uname -m)), RAM: $(( $(sysctl -n hw.memsize) / 1073741824 )) GB, free in the install folder: $(free_gb) GB, in Downloads: $(free_gb "$HOME/Downloads") GB (the installers need about 3 GB there)"
PROBLEMS=0
echo "   Install folder: $TI   Admin account: $(id -Gn | tr ' ' '\n' | grep -qx admin && echo yes || echo no)   Log: $LOG"
[ "$TI" = "$HOME/ti" ] && echo "   Note: no write access to /Applications, so tools go in ~/ti. CCStudio finding SDKs there is untested."
if [ "$(uname -m)" != "x86_64" ]; then
  echo "   Apple Silicon Mac. Never tried with this script. TI's tools are Intel programs and need Rosetta 2."
  if arch -x86_64 /usr/bin/true 2>/dev/null; then echo "   Rosetta 2: installed."; else
    echo "   Rosetta 2: not installed. Install it with:  softwareupdate --install-rosetta --agree-to-license"
    echo "   (it may ask for an admin password; on a company Mac, IT may have to do it)"
    PROBLEMS=$((PROBLEMS+1)); [ "$DRY" = 1 ] || exit 1
  fi
  if [ "$ZEPHYR" = 1 ]; then echo "   The Zephyr step is Intel only. Leaving it out."; ZEPHYR=0; fi
fi
if profiles status -type enrollment 2>/dev/null | grep -q "MDM enrollment: Yes"; then
  echo "   This Mac is managed by an organisation (MDM). Its policy may block unsigned installers"
  echo "   or the debug probe's USB access. The script cannot tell in advance. If a step is blocked, the log will show it."
fi
case "$(sw_vers -productVersion)" in 14.*|15.*|26.*) ;; *) echo "   macOS is not on TI's validated list for CCStudio 21.0.1 (14, 15, 26). Stopping."; [ "$DRY" = 1 ] || exit 1 ;; esac
xcode-select -p >/dev/null 2>&1 || { echo "   Apple's command line tools are missing (they provide git). Run: xcode-select --install"; [ "$DRY" = 1 ] || exit 1; }
if [ "$ALL" = 1 ]; then need_gb 25; else need_gb 12; fi
curl -sI --max-time 15 https://dr-download.ti.com/ >/dev/null 2>&1 && echo "   Network: can reach TI's download server." || { echo "   Network: cannot reach dr-download.ti.com. A proxy or VPN may be in the way."; PROBLEMS=$((PROBLEMS+1)); }
# "|| true": on a clean Mac no folder matches, and under set -e -o pipefail the loop's last false test would end the script
for d in "$TI"/ccs* /Applications/ti/ccs* "$HOME"/ti/ccs*; do [ -d "$d" ] && echo "   Already here: $d"; done | sort -u || true
ls -d "$TI"/ccs* /Applications/ti/ccs* "$HOME"/ti/ccs* 2>/dev/null | grep -v ccs2101 | head -1 | grep -q . && echo "   An older CCStudio is installed. This script adds 21.0.1 beside it in $TI/ccs2101 and does not touch the old one."
if [ "$CHECK" = 1 ]; then
  say "Check finished. Nothing was changed."
  [ "$(free_gb)" -lt 12 ] && PROBLEMS=$((PROBLEMS+1))
  if [ "$PROBLEMS" = 0 ]; then echo "   This Mac looks ready. Next: ./bootstrap.sh"; else echo "   $PROBLEMS thing(s) to fix first, listed above."; fi
  echo "   Log saved to $LOG"; exit 0
fi

# ---------------------------------------------------------------- 2. licences
say "2. Licences"
cat <<'MSG'
   The installers run in no-click mode, which accepts each TI licence for you.
   Read them first. Each download page links its licence and manifest:
     https://www.ti.com/tool/download/CCSTUDIO/21.0.1
     https://www.ti.com/tool/download/MSPM0-SDK
MSG
[ "$ALL" = 1 ] && cat <<'MSG'
     https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK
     https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK
     https://www.ti.com/tool/download/SIMPLELINK-WIFI-TOOLBOX
     https://www.ti.com/tool/download/MSPM33-SDK
     https://www.ti.com/tool/download/AM13E2X-SDK
     https://www.ti.com/tool/download/UNIFLASH
MSG
if [ "$DRY" = 1 ]; then echo "   [dry-run] would ask you to type: I accept"; else
  printf '   Type "I accept" to continue: '; read -r ans; case "$(printf '%s' "$ans" | tr '[:upper:]' '[:lower:]' | sed 's/^ *//;s/ *$//')" in "i accept") ;; *) echo "   Stopped. Nothing was installed."; exit 1 ;; esac
fi
run mkdir -p "$WORK"

# verify <file> <sha256>: stop unless the file matches the checksum published on TI's download page
verify() { echo "$2  $1" | shasum -a 256 -c - >/dev/null 2>&1 || { echo "   Checksum mismatch for $1."; echo "   It does not match the SHA-256 on TI's download page. Not running it. Delete it and download again."; exit 1; }; echo "   Checksum matches TI's published SHA-256."; }

# wait_for <glob in ~/Downloads> <page url> <what to click>
wait_for() {
  local pat="$1" url="$2" what="$3" f
  f=$(ls -d "$DL"/$pat 2>/dev/null | grep -v -E '\.(download|crdownload|part)$' | head -1 || true)
  if [ -n "$f" ]; then echo "   Found $f"; FOUND="$f"; return; fi
  echo "   Opening $url"
  echo "   In the browser: log in to myTI, answer the export question, click: $what"
  if [ "$DRY" = 1 ]; then echo "   [dry-run] would wait for $pat to appear in Downloads"; FOUND="$DL/$pat"; return; fi
  open "$url"
  printf '   Waiting for the download'
  while :; do
    f=$(ls -d "$DL"/$pat 2>/dev/null | grep -v -E '\.(download|crdownload|part)$' | head -1 || true)
    if [ -n "$f" ] && ! ls -d "$DL"/$pat.download "$DL"/$pat.crdownload >/dev/null 2>&1; then break; fi
    printf '.'; sleep 5
  done
  echo " got it."; FOUND="$f"
}

# ---------------------------------------------------------------- 3. CCStudio (login 1 of 2)
say "3. CCStudio 21.0.1  (browser login 1 of 2)"
if [ -d "$TI/ccs2101/ccs" ]; then echo "   Already installed at $TI/ccs2101. Skipping."; else
  wait_for 'CCS_21.0.1.*_mac_x86.dmg' "https://www.ti.com/tool/download/CCSTUDIO/21.0.1" "macOS x86_64 single file installer"
  # Device families for this kit only. A full install is 8.1 GB; this is smaller.
  COMPONENTS="PF_MSPM0,PF_MSPM33,PF_WCONN,PF_AM13X,PF_MMWAVE"
  CCS_SHA=9b392e0707c1b3a21476fe0194e0517a659475bbb09c5b6c26c9d60bbd96530d
  if [ "$DRY" = 1 ]; then echo "   [dry-run] would verify the checksum, mount the image and run its installer with --mode unattended --prefix $TI/ccs2101 --enable-components $COMPONENTS"; else
    verify "$FOUND" "$CCS_SHA"
    VOL=$(hdiutil attach -nobrowse "$FOUND" | tail -1 | sed 's/.*\(\/Volumes\/.*\)/\1/')
    APP=$(ls -d "$VOL"/ccs_setup_*.app | head -1)
    echo "   Installing. About 7 minutes, no clicks."
    # Run the Intel installer program directly when it is there. TI's launcher script refuses
    # to start on Apple Silicon ("OS X version is not supported"); the program itself runs under Rosetta.
    INST="$APP/Contents/MacOS/osx-x86_64"; [ -x "$INST" ] || INST="$APP/Contents/MacOS/installbuilder.sh"
    "$INST" --mode unattended --prefix "$TI/ccs2101" --enable-components "$COMPONENTS" \
      || { echo "   The no-click install failed. Opening the installer so you can click through it."; open "$APP"; echo "   Re-run this script when the installer finishes."; exit 1; }
    hdiutil detach "$VOL" >/dev/null
    rm -f "$FOUND"
  fi
fi

# ---------------------------------------------------------------- 4. MSPM0 SDK (login 2 of 2)
say "4. MSPM0 SDK 2.11.00.07  (browser login 2 of 2)"
if [ -d "$TI/mspm0_sdk_2_11_00_07" ]; then echo "   Already installed. Skipping."; else
  # The checksum is for the .zip, so the .zip is required. Safari unzips downloads by default
  # and throws the .zip away; an unzipped installer cannot be verified and is not run.
  M0_SHA=87542b92fd7010cc80bf9aa6974b61c3370c5bdbfd1fca4f2fb4f18b3f8d3826
  if [ -d "$DL/mspm0_sdk_2_11_00_07.app" ] && [ ! -f "$DL/mspm0_sdk_2_11_00_07.app.zip" ] && [ "$DRY" = 0 ]; then
    echo "   Found an unzipped installer in Downloads but no .zip. It cannot be checked against TI's checksum."
    echo "   Delete it. In Safari: Settings > General, untick \"Open safe files after downloading\". Then re-run."
    exit 1
  fi
  echo "   Safari users: untick Settings > General > \"Open safe files after downloading\" first, or the .zip disappears."
  wait_for 'mspm0_sdk_2_11_00_07.app.zip' "https://www.ti.com/tool/download/MSPM0-SDK" "mspm0_sdk_2_11_00_07.app.zip (MSPM0 SDK for MacOS)"
  if [ "$DRY" = 1 ]; then echo "   [dry-run] would verify the checksum and install with --mode unattended --prefix $TI"; else
    verify "$FOUND" "$M0_SHA"
    (cd "$WORK" && unzip -q -o "$FOUND"); APP="$WORK/mspm0_sdk_2_11_00_07.app"
    "$APP/Contents/MacOS/osx-x86_64" --mode unattended --unattendedmodeui none --prefix "$TI"
    rm -rf "$APP" "$FOUND"
  fi
fi

# ---------------------------------------------------------------- 5. Tier 2 (no login needed)
# name | url | sha256 | kind (zip = InstallBuilder .app.zip, dmg = InstallBuilder app in a disk image) | prefix | folder that proves it is installed
B=https://dr-download.ti.com/software-development
TIER2=(
"Wi-Fi Toolbox 4.3.23|$B/application-software-framework/MD-xUhL0Nqy3m/4.3.23/noterize_simplelink_wifi_toolbox_macos_4_3_23.app.zip|6e8f76f178ee6c109bd31eefbc725503e56f2ca1cc2ffd2352ce91e927a560fd|zip|$TI|$TI/simplelink_wifi_toolbox_macos_4_3_23"
"AM13E2x SDK 26.01.00.03|$B/software-development-kit-sdk/MD-RnlgGIB5Yq/26.01.00.03.STS/am13e230x_sdk_26_01_00_03_STS-osx-installer.dmg|57df81be9e114ecc0df4e6cbe5b9a8d78a03e47a9d6a12aacedb65ff307eae3d|dmg|$TI|$TI/am13e230x_sdk_26_01_00_03"
"MSPM33 SDK 1.04.00.00|$B/software-development-kit-sdk/MD-vudosq1FZH/1.04.00.00/mspm33_sdk_1_04_00_00.app.zip|fe21189c64521db89b473ee392f59034779e8921315db432850197a6b565ebcb|zip|$TI|$TI/mspm33_sdk_1_04_00_00"
"UniFlash 9.6.0|$B/software-programming-tool/MD-QeJBJLj8gq/9.6.0/uniflash_sl.9.6.0.5764.dmg|4b4d7c85c922823eaa2d84753d035579606bdbc1865158c4265a36fbcf85fade|dmg|$TI/uniflash_9.6.0|$TI/uniflash_9.6.0"
"Low Power F3 SDK 9.21.00.36|$B/software-development-kit-sdk/MD-emMPuXshOG/9.21.00.36.LTS/simplelink_lowpower_f3_sdk_9_21_00_36_LTS.app.zip|634e90a7fa503579a813a12afd97b8255665d9bceb8dbf1a2a3d34aa98554512|zip|$TI|$TI/simplelink_lowpower_f3_sdk_9_21_00_36_LTS"
"Wi-Fi SDK 10.20.00.39|$B/software-development-kit-sdk/MD-omxWrKJDYa/10.20.00.39/simplelink_wifi_sdk_10_20_00_39.app.zip|0ee600883c9370f112f4dd1a58d7cd1ce353565b4ae35bb13ed1ab90db032730|zip|$TI|$TI/simplelink_wifi_sdk_10_20_00_39"
)
if [ "$ALL" = 1 ]; then
  say "5. Tier 2: six packages, no login, no clicks"
  for row in "${TIER2[@]}"; do
    IFS='|' read -r name url sha kind prefix proof <<<"$row"
    if [ -d "$proof" ]; then echo "   $name: already installed. Skipping."; continue; fi
    file="$WORK/${url##*/}"
    echo "   $name: download, verify, install"
    if [ "$DRY" = 1 ]; then echo "   [dry-run] curl $url ; check sha256 ; install with --mode unattended --prefix $prefix"; continue; fi
    need_gb 4
    curl -fL --progress-bar -o "$file" "$url"
    echo "$sha  $file" | shasum -a 256 -c - >/dev/null || { echo "   Checksum mismatch for $name. Not installing it. TI may have replaced the file."; exit 1; }
    if [ "$kind" = zip ]; then
      (cd "$WORK" && unzip -q -o "$file"); app=$(ls -dt "$WORK"/*.app | head -1)
      "$app/Contents/MacOS/osx-x86_64" --mode unattended --unattendedmodeui none --prefix "$prefix"
      rm -rf "$app"
    else
      vol=$(hdiutil attach -nobrowse "$file" | tail -1 | sed 's/.*\(\/Volumes\/.*\)/\1/'); app=$(ls -d "$vol"/*.app | head -1)
      "$app/Contents/MacOS/osx-x86_64" --mode unattended --unattendedmodeui none --prefix "$prefix"
      hdiutil detach "$vol" >/dev/null
    fi
    rm -f "$file"
  done
else
  say "5. Tier 2 skipped (run with --all to add the wireless, MSPM33, AM13 and UniFlash packages)"
fi

# ---------------------------------------------------------------- 6. Zephyr
if [ "$ZEPHYR" = 1 ]; then
  say "6. Zephyr 4.4.2 for the LP-MSPM0G3507 (build only, nothing is flashed)"
  Z="$HERE/../mspm0g3507/zephyr/setup-intel-mac.sh"
  if [ ! -f "$Z" ]; then echo "   Cannot find $Z. Clone the mspm0g3507 repo next to this one."; else
    need_gb 3
    run env NO_FLASH=1 bash "$Z"
  fi
fi

# ---------------------------------------------------------------- 7. what is left for you
say "Done in $(elapsed). Installed under $TI:"
ls "$TI" 2>/dev/null | grep -v Icon | sed 's/^/   /' || true
cat <<'MSG'

== What only you can do
   1. Open CCStudio from Finder: /Applications/ti/ccs2101/ccs/Code Composer Studio.app
      (If it was already open, quit and reopen it. It only notices new SDKs at launch.)
   2. In CCStudio: Help > Get Started with AI > Claude Code. Scope: Workspace.
      Leave the token and URL boxes blank. Keep the MCP servers box ticked. Apply.
      Click Open Claude Code once and log in with your Claude subscription if asked.
   3. Plug in a board with a data cable. If your Mac only has USB-C, you need an adapter.
      Two boards plugged in at once confuses the flashing tools. Use one at a time.
   4. Ask the chat: "Import the gpio_toggle_output example for my LaunchPad, build it,
      flash it and run it." Then ask it to change the blink rate, so you know it was yours.
MSG
