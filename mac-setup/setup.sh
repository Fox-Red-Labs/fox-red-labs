#!/bin/bash
# Tier 1 Mac setup for the Fox Red Labs journey. Scriptable parts only. Intel (x86_64) Mac.
# No Rosetta: CCStudio and the TI tools are x86_64 and run natively here.
# No Homebrew: it compiles from source on this Mac. Vendor installers only.
# TI downloads need a myTI login and an export approval, so this opens the pages instead of fetching them.
# Run: ./setup.sh   (time it; the minutes go in STOPWATCH.md)
set -u
step() { printf '\n== %s\n' "$1"; }

step "1. Host check"
sw_vers
echo "CPU: $(sysctl -n machdep.cpu.brand_string) ($(uname -m))"
echo "RAM: $(( $(sysctl -n hw.memsize) / 1073741824 )) GB"
df -h /System/Volumes/Data | tail -1 | awk '{print "Disk: " $2 " total, " $4 " free"}'
[ "$(uname -m)" = "x86_64" ] || { echo "This script is for an Intel Mac. Stop."; exit 1; }
case "$(sw_vers -productVersion)" in
  14.*|15.*|26.*) echo "macOS version is on TI's validated list for CCStudio 21.0.1." ;;
  *) echo "macOS version is not on TI's validated list (14, 15, 26). Stop."; exit 1 ;;
esac
free_gb=$(df -g /System/Volumes/Data | tail -1 | awk '{print $4}')
[ "$free_gb" -ge 6 ] || { echo "Under 6 GB free. Clear space first."; exit 1; }

step "2. Command line tools (git)"
xcode-select -p >/dev/null 2>&1 && echo "command line tools present, git $(git --version | cut -d' ' -f3)" || xcode-select --install

step "3. Claude Code"
if command -v claude >/dev/null; then echo "claude present: $(claude --version 2>/dev/null | head -1)"; else
  curl -fsSL https://claude.ai/install.sh | bash; fi
echo "If not logged in yet: run 'claude' once and sign in with your Claude subscription."

step "4 to 6. TI downloads (click, log in to myTI, approve export, install to the default location)"
open "https://www.ti.com/tool/download/CCSTUDIO/21.0.1"
open "https://www.ti.com/tool/download/MSPM0-SDK"
cat <<'MSG'
  CCStudio: pick "macOS x86_64 single file installer". Install as your user (not sudo).
  MSPM0 SDK: pick the macOS .app.zip. Keep the default install location.
  CCStudio lands in /Applications/ti/ccs2101. No driver script to run on macOS; the installer handles drivers.
MSG

step "7. Turn on the AI assistant inside CCS"
echo "Help > Get Started with AI > Claude Code. Leave Auth Token and Base URL blank. Tick 'Enable CCStudio ecosystem MCP servers'. Apply. Log in in the chat panel."

step "8. Node.js"
if command -v node >/dev/null; then echo "node present: $(node --version)"; else
  echo "Node missing. Opening the installer page; take the macOS .pkg, current LTS."; open "https://nodejs.org/en/download"; fi

step "Check"
ls /Applications/ti 2>/dev/null || echo "(/Applications/ti appears once the CCStudio installer finishes)"
echo "Record each step's minutes in STOPWATCH.md."
