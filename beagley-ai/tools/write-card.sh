#!/bin/bash
# Write the BeagleY-AI Edge AI image to a micro-SD card on a Mac. Checks the download's SHA-256 first. Destroys the card.
# Usage: beagley-ai/tools/write-card.sh diskN path/to/beagley-ai-edge-ai-11.00.00.08-2025-09-06.img.xz [work folder]
# (N from `diskutil list`). Needs xz (`brew install xz`). The image is 10.6 GB unpacked: use a 32 GB card, and have
# 11 GB free in the work folder (default: the image's folder; an external drive is fine).
# Works with USB card readers and with the Mac's built-in SD slot, which lists the card as an internal disk.
# Raw writes to /dev/rdiskN must be whole 512-byte sectors, so the image is unpacked to a file and padded first.
set -euo pipefail
D="${1:?usage: write-card.sh diskN image.img.xz [work folder]}"; IMG="${2:?usage: write-card.sh diskN image.img.xz [work folder]}"
WORK="${3:-$(dirname "$IMG")}"
WANT=15590838f83501a636364199da2f1bf7cda501bbdc918c64beb36f18a92604e6   # BeagleBoard distros page, 2026-11-01
[[ "$D" =~ ^disk[0-9]+$ ]] || { echo "give a whole disk such as disk4, not $D"; exit 1; }
INFO=$(diskutil info "/dev/$D") || { echo "no such disk"; exit 1; }
echo "$INFO" | grep -E "Device / Media Name|Disk Size|Removable Media|Protocol|Device Location"
BOOT=$(diskutil info / | awk -F': *' '/Part of Whole/ {print $2}')
APFS_STORE=$(diskutil info / | awk -F': *' '/APFS Physical Store/ {print $2}' | sed 's/s[0-9]*$//')
[ "$D" != "$BOOT" ] && [ "$D" != "$APFS_STORE" ] || { echo "refusing $D: it holds this Mac's system"; exit 1; }
if ! echo "$INFO" | grep -q "Removable Media:.*Removable"; then
  echo "$INFO" | grep -qE "Protocol:.*Secure Digital|SD Card Reader|Media Name:.*SD" \
    || { echo "refusing $D: not removable and not an SD card"; exit 1; }
  echo "built-in SD slot: allowed"
fi
BYTES=$(echo "$INFO" | sed -n 's/.*Disk Size:.*(\([0-9]*\) Bytes).*/\1/p')
[ -n "$BYTES" ] && [ "$BYTES" -ge 16000000000 ] && [ "$BYTES" -le 600000000000 ] \
  || { echo "refusing $D: size ${BYTES:-unknown} bytes is not a 16 to 512 GB card"; exit 1; }
GOT=$(shasum -a 256 "$IMG" | cut -d' ' -f1)
[ "$GOT" = "$WANT" ] || { echo "SHA-256 mismatch: got $GOT, want $WANT"; exit 1; }
FREE=$(df -k "$WORK" | awk 'NR==2 {print $4}')
[ "$FREE" -ge 11500000 ] || { echo "need about 11 GB free in $WORK (have $((FREE/1024/1024)) GB); pass a work folder on another drive"; exit 1; }
RAW="$WORK/$(basename "${IMG%.xz}")"
trap 'rm -f "$RAW"' EXIT
echo "unpacking to $RAW"; xz -dc "$IMG" > "$RAW"
SIZE=$(stat -f %z "$RAW"); PAD=$(( (512 - SIZE % 512) % 512 ))
[ "$PAD" -eq 0 ] || head -c "$PAD" /dev/zero >> "$RAW"
echo "image $SIZE bytes, padded by $PAD"
diskutil unmountDisk "/dev/$D"
echo "writing to /dev/r$D at $(date +%T)"
sudo dd if="$RAW" of="/dev/r$D" bs=4m status=progress
sync; diskutil eject "/dev/$D"; echo "done $(date +%T); put the card in the BeagleY-AI"
