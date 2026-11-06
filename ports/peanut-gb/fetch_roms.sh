#!/bin/bash
# One command: bash fetch_roms.sh
# Downloads the two freely licensed ROMs used for tests into roms/ (git-ignored, never committed) and checks their hashes.
#   tobu.gb       Tobu Tobu Girl by Tangram Games. Code MIT, assets CC BY 4.0. https://github.com/SimonLarsen/tobutobugirl
#                 file from the Homebrew Hub database https://github.com/gbdev/database (entries/tobutobugirl)
#   dmg-acid2.gb  dmg-acid2 by Matt Currie, MIT. https://github.com/mattcurrie/dmg-acid2 (release v1.0)
set -e
cd "$(dirname "$0")"
mkdir -p roms
get() { # name url sha256
  [ -f roms/$1 ] || curl -fsSL --max-time 60 "$2" -o roms/$1
  got=$(shasum -a 256 roms/$1 | cut -d' ' -f1)
  [ "$got" = "$3" ] || { echo "hash mismatch for $1: $got"; exit 1; }
  echo "ok roms/$1"
}
get tobu.gb "https://raw.githubusercontent.com/gbdev/database/master/entries/tobutobugirl/tobu.gb" 5d3871cae77db2287807e8914fd21f76203e73aa3fec20955489d45cb4571d8f
get dmg-acid2.gb "https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb" 464e14b7d42e7feea0b7ede42be7071dc88913f75b9ffa444299424b63d1dff1
