#!/bin/bash
# Make a raw FAT image for the host SD simulator: make_sd_image.sh <out.img> <16|32> <file>...
# Uses hdiutil (macOS): the image is a bare FAT volume with no partition table, which FatFs and Petit FatFs both mount.
set -e
OUT="$1"; BITS="$2"; shift 2
D="$(mktemp -d)"; rm -f "$OUT" "$D/v.dmg"
if [ "$BITS" = 32 ]; then hdiutil create -size 48m -fs "MS-DOS FAT32" -layout NONE -volname SHARPSIM -type UDIF "$D/v" >/dev/null
else hdiutil create -size 16m -fs "MS-DOS FAT16" -layout NONE -volname SHARPSIM -type UDIF "$D/v" >/dev/null; fi
mkdir "$D/mnt"; hdiutil attach -imagekey diskimage-class=CRawDiskImage -mountpoint "$D/mnt" "$D/v.dmg" >/dev/null
for f in "$@"; do cp "$f" "$D/mnt/"; done
sync; hdiutil detach "$D/mnt" >/dev/null
mv "$D/v.dmg" "$OUT"; rm -rf "$D"
